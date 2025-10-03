#include "memory/arena_allocator.hpp"

#include "core/log.h"

ArenaAllocator::ArenaAllocator(size_t arena_size) : 
    size(arena_size), offset(0), owns_memory(true)
{
    data = new uint8_t[arena_size];
}

ArenaAllocator::ArenaAllocator(void *memory, size_t arena_size) : 
    data(static_cast<uint8_t*>(memory)), size(arena_size), offset(0), owns_memory(false)
{

}

ArenaAllocator::~ArenaAllocator()
{   
    if (data && owns_memory) {
        try {
            delete[] data;
        } catch (...) {
            KS_LOG_ERROR("Failed to delete arena data during destruction");
        }
        data = nullptr;
    }
}

void *ArenaAllocator::allocate(size_t bytes, size_t alignment){
    size_t aligned_offset = (offset + alignment - 1) & ~(alignment - 1);

    if (aligned_offset + bytes > size) {
        return nullptr; // Out of memory
    }

    void* ptr = data + aligned_offset;
    offset = aligned_offset + bytes;
    return ptr;
}

void ArenaAllocator::reset(){
    offset = 0;
}

size_t ArenaAllocator::get_used_memory() const
{
    return offset;
}

size_t ArenaAllocator::get_free_memory() const
{
    return size - offset;
}

size_t ArenaAllocator::get_capacity() const
{
    return size;
}