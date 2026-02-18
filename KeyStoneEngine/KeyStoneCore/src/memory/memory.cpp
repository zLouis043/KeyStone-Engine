#include "memory/memory.h"
#include "memory/memory.hpp"
#include "../include/core/error.h"

#include <algorithm>
#include <string.h>
#include <assert.h>

#include "core/log.h"

std::unique_ptr<MemoryManager> MemoryManager::s_instance = nullptr;
std::mutex MemoryManager::s_instance_mutex;
std::atomic<bool> MemoryManager::s_shutdown_flag{false};

static std::mutex s_stats_mutex;
static MemoryManager::MemoryStats s_live_stats;

static const uint32_t KS_MEMORY_MAGIC = 0xDEADBEEF;

struct AllocationHeader {
    size_t size;
    void* allocator_ptr;
    MemoryManager::Tag tag;
    uint32_t magic;
    AllocationHeader* prev;
    AllocationHeader* next;
};

static AllocationHeader* s_alloc_head = nullptr;

static void update_stats_alloc(AllocationHeader* h) {
    std::lock_guard<std::mutex> lock(s_stats_mutex);

    s_live_stats.total_allocated += h->size;
    s_live_stats.tag_stats[h->tag].count++;
    s_live_stats.tag_stats[h->tag].total_size += h->size;

    h->next = s_alloc_head;
    h->prev = nullptr;

    if (s_alloc_head) {
        s_alloc_head->prev = h;
    }
    s_alloc_head = h;
}

static void update_stats_dealloc(AllocationHeader* h) {
    std::lock_guard<std::mutex> lock(s_stats_mutex);

    s_live_stats.total_allocated -= h->size;
    s_live_stats.tag_stats[h->tag].count--;
    s_live_stats.tag_stats[h->tag].total_size -= h->size;

    if (h->prev) {
        h->prev->next = h->next;
    }
    else {
        s_alloc_head = h->next;
    }

    if (h->next) {
        h->next->prev = h->prev;
    }
}

static void update_stats_dealloc(size_t size, MemoryManager::Tag tag) {
    std::lock_guard<std::mutex> lock(s_stats_mutex);
    s_live_stats.total_allocated -= size;
    s_live_stats.tag_stats[tag].count--;
    s_live_stats.tag_stats[tag].total_size -= size;
}

MemoryManager::MemoryManager() : 
    permanent_allocator(8 * 1024 * 1024)
{
    set_frame_capacity(64 * 1024);

    std::vector<std::pair<size_t, size_t>> default_pools = {
        {32, 1000},    // Small objects: 32 bytes, 1000 count
        {64, 500},     // Medium objects: 64 bytes, 500 count  
        {128, 250},    // Large objects: 128 bytes, 250 count
        {256, 100},    // Extra large: 256 bytes, 100 count
        {512, 50},     // Huge objects: 512 bytes, 50 count
        {1024, 25},    // Massive objects: 1024 bytes, 25 count
    };

    set_resource_pools_config(default_pools);

    ks_error_make_module_prefix("MemoryManager");

    is_initialized = true;
}

MemoryManager::~MemoryManager()
{
    if (is_initialized && !s_shutdown_flag.load()) {
        safe_cleanup();
    }
}

void MemoryManager::set_frame_capacity(size_t frame_mem_capacity_in_bytes)
{
    frame_arena = ArenaAllocator(frame_mem_capacity_in_bytes);
}

void MemoryManager::set_resource_pools_config(const std::vector<std::pair<size_t, size_t>> &configs)
{
    resource_pools.clear();
    resource_pools.reserve(configs.size());

    for (const auto& config : configs) {
        resource_pools.emplace_back(std::make_unique<PoolAllocator>(config.first, config.second));
    }
    
    std::sort(resource_pools.begin(), resource_pools.end(),
        [](const std::unique_ptr<PoolAllocator>& a, const std::unique_ptr<PoolAllocator>& b) {
            return a->get_block_size() < b->get_block_size();
        });
}

MemoryManager& MemoryManager::get_instance() {
    std::lock_guard<std::mutex> lock(s_instance_mutex);
    if (!s_instance) {
        s_shutdown_flag.store(false);

        s_instance = std::make_unique<MemoryManager>();
            
        static bool atexit_registered = false;
        if (!atexit_registered) {
            std::atexit([]() {
                s_shutdown_flag.store(true);
                std::lock_guard<std::mutex> lock(s_instance_mutex);
                s_instance.reset();
                });
            atexit_registered = true;
        }
    }
    return *s_instance;
}

void MemoryManager::shutdown() {
    std::lock_guard<std::mutex> lock(s_instance_mutex);
    s_shutdown_flag.store(true);
    if (s_instance) {
        s_instance->safe_cleanup();
        s_instance.reset();
    }
}

void* MemoryManager::alloc(size_t size_in_bytes, Lifetime lt, Tag tag, const char* debug_name, size_t count)
{
    if (s_shutdown_flag.load()) return nullptr;
    size_t user_size = size_in_bytes * count;

    size_t header_size = sizeof(AllocationHeader);
    header_size = (header_size + 15) & ~15;

    size_t total_required = user_size + header_size;

    void* raw_ptr = nullptr;
    void* allocator_ptr = nullptr;

    switch (lt) {
    case FRAME:
        raw_ptr = frame_arena.allocate(user_size);
        if (raw_ptr) return raw_ptr;
        break;

    case PERMANENT:
        raw_ptr = permanent_allocator.allocate(user_size);
        if (raw_ptr) return raw_ptr;
        break;

    case SCOPED:
    case USER_MANAGED:
        if (tag == RESOURCE || tag == SCRIPT) {
            PoolAllocator* pool = find_suitable_pool(total_required);
            if (pool) {
                raw_ptr = pool->allocate();
                allocator_ptr = pool;
            }
        }
        if (!raw_ptr) {
            raw_ptr = allocate_from_system(total_required);
            allocator_ptr = nullptr;
        }
        break;
    }

    if (!raw_ptr) return nullptr;

    AllocationHeader* h = static_cast<AllocationHeader*>(raw_ptr);
    h->size = user_size;
    h->allocator_ptr = allocator_ptr;
    h->tag = tag;
    h->magic = KS_MEMORY_MAGIC;

    update_stats_alloc(h);

    return static_cast<char*>(raw_ptr) + header_size;
}

void* MemoryManager::realloc(void* ptr, size_t new_size_in_bytes)
{
    if(s_shutdown_flag.load()) return nullptr;

    if (!ptr) return alloc(new_size_in_bytes, Lifetime::USER_MANAGED, Tag::SCRIPT, "realloc_new");

    size_t header_size = sizeof(AllocationHeader);
    header_size = (header_size + 15) & ~15;

    void* old_raw = static_cast<char*>(ptr) - header_size;
    AllocationHeader* h = static_cast<AllocationHeader*>(old_raw);

    if (h->magic != KS_MEMORY_MAGIC) {
        assert(false && "Invalid pointer passed to realloc (bad magic)");
        return nullptr;
    }

    size_t old_size = h->size;

    if (h->allocator_ptr) {
        PoolAllocator* pool = static_cast<PoolAllocator*>(h->allocator_ptr);
        if (new_size_in_bytes + header_size <= pool->get_block_size()) {
            update_stats_dealloc(h);
            h->size = new_size_in_bytes;
            update_stats_alloc(h);
            return ptr;
        }
    }

    void* new_ptr = alloc(new_size_in_bytes, Lifetime::USER_MANAGED, h->tag, "realloc_move");
    if (new_ptr) {
        memcpy(new_ptr, ptr, std::min(old_size, new_size_in_bytes));
        dealloc(ptr);
    }
    return new_ptr;
}

void MemoryManager::dealloc(void* ptr)
{
    if (!ptr || s_shutdown_flag.load()) return;

    size_t header_size = sizeof(AllocationHeader);
    header_size = (header_size + 15) & ~15;

    void* raw_ptr = static_cast<char*>(ptr) - header_size;
    AllocationHeader* h = static_cast<AllocationHeader*>(raw_ptr);

    if (h->magic != KS_MEMORY_MAGIC) {
        return;
    }

    update_stats_dealloc(h);

    h->magic = 0;

    if (h->allocator_ptr) {
        static_cast<PoolAllocator*>(h->allocator_ptr)->deallocate(raw_ptr);
    }
    else {
        deallocate_to_system(raw_ptr);
    }
}

void MemoryManager::reset_frame()
{
    frame_arena.reset();
}

void MemoryManager::cleanup_permanent()
{
    permanent_allocator.cleanup_all();
}

MemoryManager::MemoryStats MemoryManager::get_stats() const
{
    std::lock_guard<std::mutex> lock(s_stats_mutex);

    MemoryStats stats = s_live_stats;

    stats.frame_used = frame_arena.get_used_memory();
    stats.frame_capacity = frame_arena.get_capacity();
    stats.permanent_allocated = permanent_allocator.get_used_memory();

    stats.resource_pools_used = 0;
    stats.resource_pools_capacity = 0;
    for (const auto& pool : resource_pools) {
        stats.resource_pools_used += pool->get_used_memory();
        stats.resource_pools_capacity += pool->get_capacity();
    }

    return stats;
}


void MemoryManager::print_stats() const
{
    auto stats = get_stats();

    KS_LOG_INFO("=== Memory Manager Stats ===");
    KS_LOG_INFO("Total Allocated: {} KB", stats.total_allocated / 1024.0f);
    KS_LOG_INFO("Frame: {}/{} KB ({})", 
           stats.frame_used / 1024, stats.frame_capacity / 1024,
           (float)stats.frame_used / stats.frame_capacity * 100.0f);
    KS_LOG_INFO("Permanent: {} KB", stats.permanent_allocated / 1024.0f);
    KS_LOG_INFO("Resource Pools: {}/{} KB ({})",
           stats.resource_pools_used / 1024, stats.resource_pools_capacity / 1024,
           (float)stats.resource_pools_used / stats.resource_pools_capacity * 100.0f);
    
    const char* tag_names[] = {"SYSTEM_DATA", "RESOURCE", "SCRIPT"};
    KS_LOG_INFO("By Tag:");

    for (int i = 0; i < TAG_COUNT; ++i) {
        if (stats.tag_stats[i].count > 0) {
            KS_LOG_INFO("  {}: {} allocations, {} KB", 
                   tag_names[i], stats.tag_stats[i].count, stats.tag_stats[i].total_size / 1024.0f);
        }
    }
    KS_LOG_INFO("============================");
}

void MemoryManager::safe_cleanup() {
        if (!is_initialized) return;
        cleanup_user_managed_allocations();
        cleanup_permanent();
        
        is_initialized = false;
}


void MemoryManager::cleanup_user_managed_allocations() {
    std::lock_guard<std::mutex> lock(s_stats_mutex);

    AllocationHeader* current = s_alloc_head;

    s_alloc_head = nullptr;

    int freed_count = 0;
    while (current != nullptr) {
        AllocationHeader* next = current->next;

        void* raw_ptr = current;

        if (current->allocator_ptr) {
            static_cast<PoolAllocator*>(current->allocator_ptr)->deallocate(raw_ptr);
        }
        else {
            deallocate_to_system(raw_ptr);
        }

        current = next;
        freed_count++;
    }

    if (freed_count > 0) {
        ks_epush_s_fmt(KS_ERROR_LEVEL_WARNING, "MemoryManager", KS_MEMORY_ERROR_GARBAGE_FOUND, "Cleaned up % d leaked allocations at shutdown.", freed_count);
    }
}

PoolAllocator *MemoryManager::find_suitable_pool(size_t size)
{
    for (auto& pool : resource_pools) {
        if (pool->get_block_size() >= size && pool->get_free_count() > 0) {
            return pool.get();
        }
    }
    return nullptr;
}

void *MemoryManager::allocate_from_system(size_t size)
{
    return std::malloc(size);
}

void MemoryManager::deallocate_to_system(void *ptr)
{
    std::free(ptr);
}






