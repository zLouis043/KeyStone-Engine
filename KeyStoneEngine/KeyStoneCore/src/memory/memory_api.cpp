#include "memory/memory.h"
#include "memory/memory.hpp"
#include "core/log.h"

#include <string.h>

ks_no_ret ks_memory_init(){
    MemoryManager::get_instance(); 
}

ks_no_ret ks_memory_shutdown(){
    MemoryManager::shutdown();
}

Ks_Memory_Stats ks_memory_get_stats()
{
    auto stats = MemoryManager::get_instance().get_stats();

    Ks_Memory_Stats result; 
    result.frame_capacity = stats.frame_capacity;
    result.frame_used = stats.frame_used;
    result.permanent_allocated = stats.permanent_allocated;
    result.resource_pools_capacity = stats.resource_pools_capacity;
    result.resource_pools_used = stats.resource_pools_used;
    
    memcpy((void*)&result.tag_stats, (void*)&stats.tag_stats, KS_TAG_COUNT * sizeof(stats.tag_stats[0]));
    result.total_allocated = stats.total_allocated;
    return result;
}

static MemoryManager::Lifetime ks_to_lt(Ks_Lifetime lt){
    switch(lt){
        case KS_LT_USER_MANAGED: return MemoryManager::Lifetime::USER_MANAGED;
        case KS_LT_FRAME:        return MemoryManager::Lifetime::FRAME;
        case KS_LT_PERMANENT:    return MemoryManager::Lifetime::PERMANENT;
        case KS_LT_SCOPED:       return MemoryManager::Lifetime::SCOPED;
    }
    return MemoryManager::Lifetime::USER_MANAGED;
}

static MemoryManager::Tag ks_to_tag(Ks_Tag tag){
    switch(tag){
        case KS_TAG_INTERNAL_DATA: return MemoryManager::Tag::INTERNAL_DATA;
        case KS_TAG_RESOURCE:      return MemoryManager::Tag::RESOURCE;
        case KS_TAG_SCRIPT:        return MemoryManager::Tag::SCRIPT;
        case KS_TAG_PLUGIN_DATA:   return MemoryManager::Tag::PLUGIN_DATA;
        case KS_TAG_JOB_SYSTEM:   return MemoryManager::Tag::JOB_SYSTEM;
        case KS_TAG_GARBAGE:       return MemoryManager::Tag::GARBAGE;
        case KS_TAG_COUNT:         return MemoryManager::Tag::TAG_COUNT;
        default:
            KS_LOG_ERROR("An invalid value was given as Ks_Tag := (%d)", (int)tag);
    }
    return MemoryManager::Tag::TAG_COUNT;
}

ks_ptr ks_alloc(ks_size size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag){
    return ks_alloc_debug(size_in_bytes, lifetime, tag, "--");
}

ks_ptr ks_alloc_debug(ks_size size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag, ks_str debug_name){

    MemoryManager::Lifetime lt = ks_to_lt(lifetime);

    MemoryManager::Tag tg = ks_to_tag(tag);

    if(tg == MemoryManager::Tag::TAG_COUNT){
        KS_LOG_ERROR("An invalid value was given as Ks_Tag := (KS_TAG_COUNT)");
        return NULL;
    }

    return MemoryManager::get_instance().alloc(
        size_in_bytes, 
        lt,
        tg,
        debug_name
    );
}

ks_no_ret  ks_dealloc(ks_ptr ptr){
    MemoryManager::get_instance().dealloc(ptr);
}

ks_ptr ks_realloc(ks_ptr ptr, ks_size new_size_in_bytes){
    return MemoryManager::get_instance().realloc(ptr, new_size_in_bytes);
}

ks_no_ret ks_set_frame_capacity(ks_size frame_mem_capacity_in_bytes){
    MemoryManager::get_instance().set_frame_capacity(frame_mem_capacity_in_bytes);
}

ks_no_ret  ks_frame_cleanup(){
    MemoryManager::get_instance().reset_frame();
}