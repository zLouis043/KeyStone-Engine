#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
#include <list>
#include <map>
#include <unordered_map>

//#include "logger.h"

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

/*
    template<typename T, typename... Args>
    T* alloc_t(Lifetime lt, Tag tag, const char* debug_name, Args&&... args){
        void * raw_ptr = alloc(sizeof(T), lt, tag, debug_name, 1);
        if(!raw_ptr) return nullptr;
        T* typed_ptr = new(raw_ptr) T(std::forward<Args>(args)...);
        return typed_ptr;
    }

    template<typename T, typename... Args>
    T* alloc_array_t(Lifetime lt, Tag tag, const char* debug_name, size_t count){
        void * raw_ptr = alloc(sizeof(T), lt, tag, debug_name, count);
        if(!raw_ptr) return nullptr;
        T* typed_ptr = static_cast<T*>(raw_ptr);
        for (size_t i = 0; i < count; ++i) {
            new(&typed_ptr[i]) T();
        }
        return typed_ptr;
    }
*/

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

/*

namespace ke {

    template<typename T>
    struct MemoryDeleter {

        MemoryDeleter() = default;

        void operator()(T* ptr) const {
            if (ptr && !MemoryManager::is_shutdown()) {
                try {
                    ptr->~T(); 
                    MemoryManager::get_instance().dealloc(ptr);
                } catch (...) {
                }
            }
        };

        template<typename U>
        MemoryDeleter(const MemoryDeleter<U>&) {}
    };

    template<typename T>
    struct ArrayDeleter {
        size_t count;

        explicit ArrayDeleter(size_t n) : count(n) {}

        void operator()(T* ptr) const {
            if (!ptr) return;
            for (size_t i = count; i > 0; --i) {
                ptr[i - 1].~T();
            }
            MemoryManager::get_instance().dealloc(ptr);
        }
    };

    template<typename T>
    using unique_ptr = std::unique_ptr<T, MemoryDeleter<T>>;

    template<typename T, typename... Args>
    unique_ptr<T> make_unique(MemoryManager::Tag tag = MemoryManager::RESOURCE, const char* debug_name  = "", Args&&... args) {
        try {
        T* ptr = MemoryManager::get_instance().alloc_t<T>(
            MemoryManager::SMART_MANAGED, tag, debug_name, std::forward<Args>(args)...);
            if (!ptr) return nullptr;
            return unique_ptr<T>(ptr);
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to create unique_ptr: {}", e.what());
            return nullptr;
        } catch (...) {
            LOG_ERROR("Unknown error creating unique_ptr");
            return nullptr;
        }
    }

    template<typename T>
    using unique_array_ptr = std::unique_ptr<T[], ArrayDeleter<T>>;

    template<typename T>
    class UniqueArray {
    private:
        unique_array_ptr<T> ptr_; 
        size_t size_;

    public:
        UniqueArray(unique_array_ptr<T>&& ptr, size_t size)
            : ptr_(std::move(ptr)), size_(size) {}

        UniqueArray(UniqueArray&&) = default;
        UniqueArray& operator=(UniqueArray&&) = default;
        UniqueArray(const UniqueArray&) = delete;
        UniqueArray& operator=(const UniqueArray&) = delete;

        size_t size() const { return size_; }
        
        T& operator[](size_t index) { return ptr_[index]; }
        const T& operator[](size_t index) const { return ptr_[index]; }
        
        T* data() { return ptr_.get(); }
        const T* data() const { return ptr_.get(); }
        
        T* begin() { return ptr_.get(); }
        T* end() { return ptr_.get() + size_; }
        const T* begin() const { return ptr_.get(); }
        const T* end() const { return ptr_.get() + size_; }
    };

    template<typename T>
    UniqueArray<T> make_unique_array_custom( 
        size_t count,
        MemoryManager::Tag tag = MemoryManager::RESOURCE,
        const char* debug_name = "")
    {
        void* raw_ptr = MemoryManager::get_instance().alloc(sizeof(T), MemoryManager::SMART_MANAGED, tag, debug_name, count);
        if (!raw_ptr) {
            return UniqueArray<T>(unique_array_ptr(nullptr, ArrayDeleter<T>(0)), 0);
        }
        T* typed_ptr = static_cast<T*>(raw_ptr);
        for (size_t i = 0; i < count; ++i) {
            new(&typed_ptr[i]) T();
        }
        
        auto unique_ptr = unique_array_ptr<T>(typed_ptr, ArrayDeleter<T>(count));
        
        return UniqueArray<T>(std::move(unique_ptr), count);
    }

    template<typename T>
    using shared_ptr = std::shared_ptr<T>;

    template<typename T, typename... Args>
    shared_ptr<T> make_shared(
        MemoryManager::Tag tag = MemoryManager::RESOURCE, 
        const char* debug_name = "", 
        Args&&... args) 
    {
        if (MemoryManager::is_shutdown()) {
            return nullptr;
        }
        
        try {
            T* ptr = MemoryManager::get_instance().alloc_t<T>(
                MemoryManager::SMART_MANAGED, 
                tag, 
                debug_name, 
                std::forward<Args>(args)...
            );
            
            if (!ptr) {
                return nullptr;
            }
        
            return std::shared_ptr<T>(ptr, [](T* p) {
                if (p && !MemoryManager::is_shutdown()) {
                    try {
                        p->~T();
                        MemoryManager::get_instance().dealloc(p);
                    } catch (...) {
                    }
                }
            });
        } catch (...) {
            return nullptr;
        }
    }

    template<typename T>
    class weak_ptr {
    private:
        std::weak_ptr<T> weak_ptr_;
        
    public:
        weak_ptr() = default;
        weak_ptr(const ke::shared_ptr<T>& shared_ptr) : weak_ptr_(shared_ptr) {}
        
        ke::shared_ptr<T> lock() const {
            if (auto shared = weak_ptr_.lock()) {
                return ke::shared_ptr<T>(shared.get(), [](T*){});
            }
            return nullptr;
        }
        
        bool expired() const { return weak_ptr_.expired(); }
    };

    template<typename T>
    class MemoryManagerAllocator {
    public:
        using value_type = T;
        
        MemoryManagerAllocator() = default;
        
        template<typename U>
        MemoryManagerAllocator(const MemoryManagerAllocator<U>&) {}
        
        T* allocate(std::size_t n) {
            return static_cast<T*>(MemoryManager::get_instance().alloc(
                n * sizeof(T), MemoryManager::SMART_MANAGED, 
                MemoryManager::SYSTEM_DATA, "STL Container"));
        }
        
        void deallocate(T* p, std::size_t n) {
            MemoryManager::get_instance().dealloc(p);
        }
        
        bool operator==(const MemoryManagerAllocator&) const { return true; }
        bool operator!=(const MemoryManagerAllocator&) const { return false; }
    };

    template<typename T>
    using vector = std::vector<T, MemoryManagerAllocator<T>>;

    template<typename T>
    using list = std::list<T, MemoryManagerAllocator<T>>;

    template<typename K, typename V, typename Compare = std::less<K>>
    using map = std::map<K, V, Compare, MemoryManagerAllocator<std::pair<const K, V>>>;


    template<typename K, typename V, typename Compare = std::less<K>>
    using unordered_map = std::unordered_map<K, V, Compare, MemoryManagerAllocator<std::pair<const K, V>>>;
};

*/