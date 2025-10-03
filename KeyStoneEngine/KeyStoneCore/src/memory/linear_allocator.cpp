#include "memory/linear_allocator.hpp"

#include "core/log.h"

LinearAllocator::LinearAllocator(size_t total_size) : 
    size(total_size), offset(0), owns_memory(true)
{
    buffer = new uint8_t[total_size];
}

LinearAllocator::~LinearAllocator()
{
    if (buffer && owns_memory) {
        try {
            delete[] buffer;
        } catch (...) {
            KS_LOG_ERROR("Failed to delete linear buffer during destruction");
        }
        buffer = nullptr;
    }
}

void *LinearAllocator::allocate(size_t bytes, size_t alignment)
{
    size_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);
    
    if (aligned_offset + bytes > size) {
        return nullptr; // Out of memory
    }
    
    void* ptr = buffer + aligned_offset;
    offset = aligned_offset + bytes;
    allocations.push_back(ptr);
    return ptr;
}

void LinearAllocator::cleanup_all()
{
    allocations.clear();
    offset = 0;
}

size_t LinearAllocator::get_used_memory() const
{
    return offset;
}

size_t LinearAllocator::get_free_memory() const 
{
    return size - offset;
}

size_t LinearAllocator::get_allocation_count() const
{
    return allocations.size();
}