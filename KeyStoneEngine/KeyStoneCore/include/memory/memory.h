/**
 * @file memory.h
 * @brief Centralized memory management system.
 * Handles allocation tracking, lifetime management (Frame vs Permanent), and profiling tags.
 * @ingroup Memory
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
* @brief Allocation lifetime strategy.
* Defines how long the memory is expected to persist.
*/
typedef enum {
    KS_LT_USER_MANAGED, ///< Explicitly managed. Must be freed with ks_dealloc().
    KS_LT_PERMANENT,    ///< Persists until engine shutdown. Cannot be freed individually.
    KS_LT_FRAME,        ///< Automatically freed at the end of the current frame. Do NOT free manually.
    KS_LT_SCOPED        ///< Scoped lifetime (experimental/reserved).
} Ks_Lifetime;

/**
 * @brief Memory categorization tags.
 * Used for profiling memory usage per subsystem.
 */
typedef enum {
    KS_TAG_INTERNAL_DATA, ///< Internal engine structures (vectors, maps, etc.).
    KS_TAG_RESOURCE,      ///< Large assets (Textures, Meshes, Audio).
    KS_TAG_SCRIPT,        ///< Scripting engine allocations (Lua state, closures).
    KS_TAG_PLUGIN_DATA,   ///< Memory allocated by external plugins.
    KS_TAG_JOB_SYSTEM,    ///< Memory allocated from the job system.
    KS_TAG_GARBAGE,       ///< Temporary buffers or uncategorized data.
    KS_TAG_COUNT          ///< Total number of tags (helper).
} Ks_Tag;

/**
 * @brief Detailed memory statistics.
 */
typedef struct {
    size_t total_allocated;         ///< Total bytes currently allocated via the engine.
    size_t frame_used;              ///< Bytes used in the current frame allocator.
    size_t frame_capacity;          ///< Total capacity of the frame allocator.
    size_t permanent_allocated;     ///< Bytes allocated in the permanent pool.
    size_t resource_pools_used;
    size_t resource_pools_capacity;

    struct {
        size_t count;               ///< Number of active allocations for this tag.
        size_t total_size;          ///< Total bytes used by this tag.
    } tag_stats[KS_TAG_COUNT];
} Ks_Memory_Stats;

/**
 * @brief Initializes the memory system.
 * Must be called once at startup before any allocation.
 */
KS_API ks_no_ret ks_memory_init();

/**
 * @brief Shuts down the memory system.
 * Reports leaks (if any) and cleans up permanent resources.
 */
KS_API ks_no_ret ks_memory_shutdown();

/**
 * @brief Retrieves current memory usage statistics.
 */
KS_API Ks_Memory_Stats ks_memory_get_stats();
 
/**
 * @brief Allocates a block of memory.
 *
 * @param size_in_bytes Size of the block.
 * @param lifetime Lifetime strategy.
 * @param tag Profiling category.
 * @return Pointer to the allocated memory, or NULL on failure.
 */
KS_API ks_ptr ks_alloc(ks_size size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag);

/**
 * @brief Allocates memory with a debug name for tracking.
 * Same as ks_alloc but attaches a name for debug tools/leak reports.
 */
KS_API ks_ptr ks_alloc_debug(ks_size size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag, ks_str debug_name);

/**
 * @brief Reallocates a user-managed memory block.
 * @warning Only valid for KS_LT_USER_MANAGED allocations.
 * @param ptr Pointer to existing memory (can be NULL).
 * @param new_size_in_bytes New size.
 * @return Pointer to the new block (may move).
 */
KS_API ks_ptr ks_realloc(ks_ptr ptr, ks_size new_size_in_bytes);

/**
 * @brief Frees a user-managed memory block.
 * @note No-op if ptr is NULL or if ptr belongs to Frame/Permanent allocators.
 * @param ptr Pointer to free.
 */
KS_API ks_no_ret ks_dealloc(ks_ptr ptr);

/**
 * @brief Sets the total capacity for the frame allocator (linear allocator).
 *
 * @param frame_mem_capacity_in_bytes Size in bytes.
 */
KS_API ks_no_ret ks_set_frame_capacity(ks_size frame_mem_capacity_in_bytes);

/**
 * @brief Resets the frame allocator.
 * Typically called internally by the engine loop at the end of a frame.
 */
KS_API ks_no_ret ks_frame_cleanup();

#ifdef __cplusplus
} // extern "C"
#endif
/** @} */