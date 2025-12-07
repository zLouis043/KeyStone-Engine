#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
#include <list>
#include <map>
#include <unordered_map>

#include "arena_allocator.hpp"
#include "pool_allocator.hpp"
#include "linear_allocator.hpp"

class MemoryManager {
public:
    enum Lifetime {
        USER_MANAGED,
        PERMANENT,
        FRAME,
        SCOPED
    };
    
    enum Tag {
        INTERNAL_DATA,
        RESOURCE,
        SCRIPT,
        PLUGIN_DATA,
        GARBAGE,
        TAG_COUNT
    };

public:
    MemoryManager();
    ~MemoryManager();

    void set_frame_capacity(size_t frame_mem_capacity_in_bytes = 64 * 1024 /*64kb*/);
    void set_resource_pools_config(const std::vector<std::pair<size_t, size_t>>& configs);

    static MemoryManager& get_instance();
    static void shutdown();

    void * alloc(size_t size_in_bytes, Lifetime lt = USER_MANAGED, Tag tag = RESOURCE, const char* debug_name = "", size_t count = 1);
    void * realloc(void* ptr, size_t new_size_in_bytes);
    void dealloc(void* ptr);

    void reset_frame();
    void cleanup_permanent();

    struct MemoryStats {
        size_t total_allocated = 0;
        size_t frame_used = 0;
        size_t frame_capacity = 0;
        size_t permanent_allocated = 0;
        size_t resource_pools_used = 0;
        size_t resource_pools_capacity = 0;

        struct TagStats {
            size_t count = 0;
            size_t total_size = 0;
        };
        
        TagStats tag_stats[TAG_COUNT];
    };

    MemoryStats get_stats() const;
    void print_stats() const;

    static bool is_shutdown()  {
        return s_shutdown_flag.load();
    }

private:
    void safe_cleanup();
    void cleanup_user_managed_allocations();

private:
    struct AllocInfo {
        size_t size;
        Lifetime lt;
        Tag tag;
        const char* debug_name = "";
        void* allocator_ptr;
    };

    ArenaAllocator frame_arena;
    std::vector<std::unique_ptr<PoolAllocator>> resource_pools;
    LinearAllocator permanent_allocator;

    std::unordered_map<void*, AllocInfo> allocation_map;
    mutable std::mutex allocation_mutex;

    static std::unique_ptr<MemoryManager> s_instance;
    static std::mutex s_instance_mutex;
    static std::atomic<bool> s_shutdown_flag;
    bool is_initialized;
    
    PoolAllocator* find_suitable_pool(size_t size);
    void* allocate_from_system(size_t size);
    void deallocate_to_system(void* ptr);
    
    void track_allocation(void* ptr, size_t size, Lifetime lt, Tag tag, const char* debug_name, void* allocator_ptr);
    void untrack_allocation(void* ptr);
};