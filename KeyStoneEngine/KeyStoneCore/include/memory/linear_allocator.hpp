#pragma once

#include <stdint.h>
#include <vector>

class LinearAllocator {
    uint8_t * buffer;
    size_t size;
    size_t offset;
    std::vector<void*> allocations;
    bool owns_memory;
public:
    LinearAllocator() : buffer(nullptr), size(0), offset(0), owns_memory(false) {};
    LinearAllocator(size_t total_size);
    ~LinearAllocator();

    void* allocate(size_t bytes, size_t alignment = sizeof(void*));
    void cleanup_all(); 

    size_t get_used_memory() const;
    size_t get_free_memory() const;
    size_t get_allocation_count() const;
};