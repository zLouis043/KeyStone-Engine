#include "memory/memory.hpp"

#include <algorithm>
#include <assert.h>

#include "core/log.h"

std::unique_ptr<MemoryManager> MemoryManager::s_instance = nullptr;
std::mutex MemoryManager::s_instance_mutex;
std::atomic<bool> MemoryManager::s_shutdown_flag{false};

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
    if (!s_instance && !s_shutdown_flag.load()) {
        s_instance = std::make_unique<MemoryManager>();
        
        // Register cleanup function to avoid static destruction order issues
        std::atexit([]() {
            s_shutdown_flag.store(true);
            std::lock_guard<std::mutex> lock(s_instance_mutex);
            s_instance.reset();
        });
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

void *MemoryManager::alloc(size_t size_in_bytes, Lifetime lt, Tag tag, const char *debug_name, size_t count)
{
    size_t total_size = size_in_bytes * count;
    void* ptr = nullptr;
    void* allocator_ptr = nullptr;

    switch (lt) {
        case FRAME: {
            ptr = frame_arena.allocate(total_size);
            allocator_ptr = &frame_arena;
            break;
        }
        
        case PERMANENT: {
            ptr = permanent_allocator.allocate(total_size);
            allocator_ptr = &permanent_allocator;
            break;
        }
        case SCOPED: 
        case USER_MANAGED: {

            if (tag == RESOURCE) {
                PoolAllocator* pool = find_suitable_pool(total_size);
                if (pool) {
                    ptr = pool->allocate();
                    allocator_ptr = pool;
                }
            }
            
            if (!ptr) {
                ptr = allocate_from_system(total_size);
                allocator_ptr = nullptr;
            }
            break;
        }

    }

    if (ptr) {
        track_allocation(ptr, total_size, lt, tag, debug_name, allocator_ptr);
    }

    return ptr;
}

void *MemoryManager::realloc(void *ptr, size_t new_size_in_bytes)
{
    if (!ptr) {
        return allocate_from_system(new_size_in_bytes);
    }

    std::lock_guard<std::mutex> lock(allocation_mutex);
    auto it = allocation_map.find(ptr);
    if (it == allocation_map.end()) {
        return std::realloc(ptr, new_size_in_bytes);
    }

    if (it->second.lt != USER_MANAGED || it->second.allocator_ptr != nullptr) {
        assert(false && "realloc not supported for arena/pool allocations");
        return nullptr;
    }

    void* new_ptr = std::realloc(ptr, new_size_in_bytes);
    if (new_ptr && new_ptr != ptr) {
        AllocInfo info = it->second;
        allocation_map.erase(it);
        info.size = new_size_in_bytes;
        allocation_map[new_ptr] = info;
    } else if (new_ptr) {
        it->second.size = new_size_in_bytes;
    }
    
    return new_ptr;
}

void MemoryManager::dealloc(void *ptr)
{
    if (!ptr || s_shutdown_flag.load()) return;

    std::lock_guard<std::mutex> lock(allocation_mutex);
    auto it = allocation_map.find(ptr);
    if (it == allocation_map.end()) {
        if (!s_shutdown_flag.load()) {
            deallocate_to_system(ptr);
        }
        return;
    }

    const AllocInfo& info = it->second;

    if (info.lt == FRAME) {
    } else if (info.lt == PERMANENT) {
    } else if (info.lt == USER_MANAGED) {
        if (info.allocator_ptr) {
            static_cast<PoolAllocator*>(info.allocator_ptr)->deallocate(ptr);
        } else {
            deallocate_to_system(ptr);
        }
    }
    
    allocation_map.erase(it);
}

void MemoryManager::reset_frame()
{
    std::lock_guard<std::mutex> lock(allocation_mutex);
    auto it = allocation_map.begin();
    while (it != allocation_map.end()) {
        if (it->second.lt == FRAME) {
            it = allocation_map.erase(it);
        } else {
            ++it;
        }
    }

    frame_arena.reset();
}

void MemoryManager::cleanup_permanent()
{
    std::lock_guard<std::mutex> lock(allocation_mutex);
    permanent_allocator.cleanup_all();
    auto it = allocation_map.begin();
    while (it != allocation_map.end()) {
        if (it->second.lt == PERMANENT) {
            it = allocation_map.erase(it);
        } else {
            ++it;
        }
    }
}

MemoryManager::MemoryStats MemoryManager::get_stats() const
{
    std::lock_guard<std::mutex> lock(allocation_mutex);
    MemoryStats stats;
    stats.frame_used = frame_arena.get_used_memory();
    stats.frame_capacity = frame_arena.get_capacity();
    stats.permanent_allocated = permanent_allocator.get_used_memory();

    for (const auto& pool : resource_pools) {
        stats.resource_pools_used += pool->get_used_memory();
        stats.resource_pools_capacity += pool->get_capacity();
    }
    
    for (const auto& [ptr, info] : allocation_map) {
        stats.total_allocated += info.size;
        stats.tag_stats[info.tag].count++;
        stats.tag_stats[info.tag].total_size += info.size;
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
    std::lock_guard<std::mutex> lock(allocation_mutex);
    
    auto it = allocation_map.begin();
    while (it != allocation_map.end()) {
        const AllocInfo& info = it->second;
        
        if (info.lt == USER_MANAGED) {
            void* ptr = const_cast<void*>(it->first);
            
            if (info.allocator_ptr == nullptr) {
                deallocate_to_system(ptr);
            }
            
            it = allocation_map.erase(it);
        } else {
            ++it;
        }
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

void MemoryManager::track_allocation(void *ptr, size_t size, Lifetime lt, Tag tag, const char *debug_name, void *allocator_ptr)
{
    std::lock_guard<std::mutex> lock(allocation_mutex);
    allocation_map[ptr] = {size, lt, tag, debug_name, allocator_ptr};
}

void MemoryManager::untrack_allocation(void *ptr)
{
    std::lock_guard<std::mutex> lock(allocation_mutex);
    allocation_map.erase(ptr);
}







