/**
 * @file memory.h
 * @brief Memory management.
 *
 * @defgroup Memory Memory System
 * @brief Allocators, tracking and management of lifetime in memory.
 * @{
 */
#pragma once

#include "../core/defines.h"
#include "../core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Defines the expected lifetime strategy for a memory allocation.
 */
enum Ks_Lifetime {
    KS_LT_USER_MANAGED, ///< Memory must be manually freed by the user using ks_dealloc.
    KS_LT_PERMANENT,    ///< Memory persists for the entire lifetime of the application.
    KS_LT_FRAME,        ///< Memory is automatically freed at the end of the current frame.
    KS_LT_SCOPED        ///< Memory is tied to a specific scope (experimental).
};

/**
 * @brief Tags used to categorize memory allocations for debugging and profiling purposes.
 */
enum Ks_Tag {
    KS_TAG_INTERNAL_DATA, ///< Internal engine data structures.
    KS_TAG_RESOURCE,      ///< Game resources like textures, models, sounds.
    KS_TAG_SCRIPT,        ///< Memory allocated for the scripting engine.
    KS_TAG_PLUGIN_DATA,   ///< Memory allocated by external plugins.
    KS_TAG_GARBAGE,       ///< Temporary or miscellaneous data.
    KS_TAG_COUNT          ///< Total number of tags.
};

/**
 * @brief Initializes the memory management system.
 * * Must be called before any other memory operation.
 */
KS_API ks_no_ret ks_memory_init();

/**
 * @brief Shuts down the memory management system.
 * * Frees all remaining resources and performs cleanup.
 */
KS_API ks_no_ret ks_memory_shutdown();

/**
 * @brief Allocates a block of memory.
 *
 * @param size_in_bytes The size of the block to allocate.
 * @param lifetime The lifetime strategy for this block.
 * @param tag The category tag for this allocation.
 * @return A pointer to the allocated memory, or NULL if allocation failed.
 */
KS_API ks_ptr ks_alloc(ks_size size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag);

/**
 * @brief Allocates a block of memory with an additional debug name.
 *
 * @param size_in_bytes The size of the block to allocate.
 * @param lifetime The lifetime strategy for this block.
 * @param tag The category tag for this allocation.
 * @param debug_name A string identifier for debugging this allocation.
 * @return A pointer to the allocated memory, or NULL if allocation failed.
 */
KS_API ks_ptr ks_alloc_debug(ks_size size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag, ks_str debug_name);

/**
 * @brief Reallocates a memory block with a new size.
 *
 * @param ptr Pointer to the existing memory block.
 * @param new_size_in_bytes The new size in bytes.
 * @return A pointer to the reallocated memory (may move), or NULL on failure.
 */
KS_API ks_ptr ks_realloc(ks_ptr ptr, ks_size new_size_in_bytes);

/**
 * @brief Frees a previously allocated memory block.
 *
 * @param ptr Pointer to the memory block to free.
 */
KS_API ks_no_ret  ks_dealloc(ks_ptr ptr);

/**
 * @brief Sets the total capacity for the frame allocator (linear allocator).
 *
 * @param frame_mem_capacity_in_bytes Size in bytes.
 */
KS_API ks_no_ret ks_set_frame_capacity(ks_size frame_mem_capacity_in_bytes);

/**
 * @brief Resets the frame allocator.
 * * This is typically called automatically by the engine loop at the end of a frame.
 */
KS_API ks_no_ret ks_frame_cleanup();

#ifdef __cplusplus
} // extern "C"
#endif
/** @} */