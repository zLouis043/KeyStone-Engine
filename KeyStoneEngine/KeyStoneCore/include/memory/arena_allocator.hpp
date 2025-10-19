#pragma once

#include <stdint.h>
#include <cstddef>

class ArenaAllocator{
    uint8_t* data;
    size_t size; 
    size_t offset;
    bool owns_memory;
public:

    ArenaAllocator() : data(nullptr), size(0), offset(0), owns_memory(false) {};
    ArenaAllocator(size_t arena_size);
    ArenaAllocator(void* memory, size_t arena_size);

    ~ArenaAllocator();

    ArenaAllocator(ArenaAllocator&& other) noexcept :
        data(other.data), size(other.size), offset(other.offset), owns_memory(other.owns_memory)
    {
        other.data = nullptr;
        other.owns_memory = false;
    }

    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept {
        if (this != &other) {
            if (data && owns_memory) {
                delete[] data;
            }
            
            // Move from other
            data = other.data;
            size = other.size;
            offset = other.offset;
            owns_memory = other.owns_memory;
            
            other.data = nullptr;
            other.owns_memory = false;
        }
        return *this;
    }

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;


    void* allocate(size_t bytes, size_t alignment = sizeof(void*));
    void reset();

    size_t get_used_memory() const;
    size_t get_free_memory() const;
    size_t get_capacity() const ;
};
