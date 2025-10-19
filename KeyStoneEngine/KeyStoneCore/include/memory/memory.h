#pragma once

#include "../core/defines.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

enum Ks_Lifetime {
    KS_LT_USER_MANAGED,
    KS_LT_PERMANENT,
    KS_LT_FRAME,
    KS_LT_SCOPED
};

enum Ks_Tag {
    KS_TAG_INTERNAL_DATA,
    KS_TAG_RESOURCE,
    KS_TAG_SCRIPT,
    KS_TAG_PLUGIN_DATA,
    KS_TAG_GARBAGE,
    KS_TAG_COUNT
};

KS_API void ks_memory_init();
KS_API void ks_memory_shutdown();

KS_API void* ks_alloc(size_t size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag);
KS_API void* ks_alloc_debug(size_t size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag, const char* debug_name);
KS_API void* ks_realloc(void* ptr, size_t new_size_in_bytes);
KS_API void  ks_dealloc(void* ptr);

KS_API void ks_set_frame_capacity(size_t frame_mem_capacity_in_bytes);
KS_API void ks_frame_cleanup();

#ifdef __cplusplus
} // extern "C"
#endif
