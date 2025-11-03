#pragma once

#include "../core/defines.h"
#include "../core/types.h"

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

KS_API ks_no_ret ks_memory_init();
KS_API ks_no_ret ks_memory_shutdown();

KS_API ks_ptr ks_alloc(ks_size size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag);
KS_API ks_ptr ks_alloc_debug(ks_size size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag, ks_str debug_name);
KS_API ks_ptr ks_realloc(ks_ptr ptr, ks_size new_size_in_bytes);
KS_API ks_no_ret  ks_dealloc(ks_ptr ptr);

KS_API ks_no_ret ks_set_frame_capacity(ks_size frame_mem_capacity_in_bytes);
KS_API ks_no_ret ks_frame_cleanup();

#ifdef __cplusplus
} // extern "C"
#endif
