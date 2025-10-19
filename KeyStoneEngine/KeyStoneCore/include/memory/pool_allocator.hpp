#pragma once

#include <stdint.h>
#include <cstddef>

class PoolAllocator{
    struct FreeBlock{
        FreeBlock * next;
    };

    uint8_t * buffer;
    FreeBlock* free_list;
    size_t block_size;
    size_t block_count;
    size_t allocated_count;
    bool owns_memory;
public:
    PoolAllocator() : buffer(nullptr), free_list(nullptr), block_size(0), block_count(0), allocated_count(0) ,owns_memory(false){};
    PoolAllocator(size_t block_size, size_t block_count);
    ~PoolAllocator();

    void * allocate();
    void deallocate(void* ptr);
    void reset();

    size_t get_block_size() const;
    size_t get_block_count() const;
    size_t get_allocated_count() const; 
    size_t get_free_count() const;
    size_t get_used_memory() const;
    size_t get_capacity() const ;
private:
    void initialize_free_list();
};
