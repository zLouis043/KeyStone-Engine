#include "memory/pool_allocator.hpp"

#include "core/log.h"

#include <algorithm>

void PoolAllocator::initialize_free_list()
{
    free_list = nullptr;

    for (size_t i = block_count; i > 0; --i) {
        FreeBlock* block = reinterpret_cast<FreeBlock*>(buffer + (i - 1) * block_size);
        block->next = free_list;
        free_list = block;
    }
}

PoolAllocator::PoolAllocator(size_t block_size, size_t block_count) :  
    block_size(std::max(block_size, sizeof(FreeBlock*))), 
      block_count(block_count), allocated_count(0), owns_memory(true)
{
    buffer = new uint8_t[this->block_size * block_count];
    initialize_free_list();
}

PoolAllocator::~PoolAllocator()
{
    if (buffer && owns_memory) {
        try {
            delete[] buffer;
        } catch (...) {
            KS_LOG_ERROR("Failed to delete pool buffer during destruction");
        }
        buffer = nullptr;
    }
}

void *PoolAllocator::allocate()
{
    if (!free_list) {
        return nullptr;
    }

    void* ptr = free_list;
    free_list = free_list->next;
    allocated_count++;

    return ptr;
}

void PoolAllocator::deallocate(void *ptr)
{
    if (!ptr) return;
    FreeBlock* block = static_cast<FreeBlock*>(ptr);
    block->next = free_list;
    free_list = block;
    allocated_count--;
}

void PoolAllocator::reset(){
    allocated_count = 0;
    initialize_free_list();
}

size_t PoolAllocator::get_block_size() const
{
    return block_size;
}

size_t PoolAllocator::get_block_count() const
{
    return block_count;
}

size_t PoolAllocator::get_allocated_count() const
{
    return allocated_count;
}

size_t PoolAllocator::get_free_count() const
{
    return block_count - allocated_count;
}

size_t PoolAllocator::get_capacity() const {
    return block_count * block_size;
}

size_t PoolAllocator::get_used_memory() const {
    return allocated_count * block_size;
}