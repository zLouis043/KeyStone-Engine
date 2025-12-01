/**
 * @file handle.h
 * @brief Unified opaque handle system.
 *
 * This system provides a generic way to reference objects safely across the engine and scripting boundary.
 * A generic Ks_Handle is a 32-bit integer composed of:
 * - **Type ID (8 bits):** Identifies the category of the object (e.g., Asset, Event).
 * - **Index (24 bits):** The unique index of the object within its category.
 *
 * @defgroup Core Core System
 * @{
 */

#pragma once

#include "defines.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A unified 32-bit handle representing a reference to an engine object.
 *
 * The handle embeds type information to prevent using an Asset handle as an Event handle
 * or vice versa.
 * Format: `[TTTTTTTT IIIIIIII IIIIIIII IIIIIIII]` (T=Type, I=Index)
 */
typedef ks_uint32 Ks_Handle;

/**
 * @brief Unique identifier for a handle type (0-255).
 */
typedef ks_uint8 Ks_Handle_Id;

#define KS_INVALID_ID (0)
#define KS_INVALID_HANDLE (0)

/**
 * @brief Registers a new handle type name and returns its unique ID.
 *
 * If the type name is already registered, returns the existing ID.
 *
 * @param handle_type The string name of the type (e.g., "Asset", "Event").
 * @return The unique Type ID assigned to this name.
 */
KS_API Ks_Handle_Id ks_handle_register(ks_str handle_type);

/**
 * @brief Retrieves the Type ID for a registered handle type name.
 *
 * @param handle_type The string name to look up.
 * @return The Type ID, or KS_INVALID_ID if not found.
 */
KS_API Ks_Handle_Id ks_handle_get_id(ks_str handle_type);

/**
 * @brief Retrieves the string name associated with a Type ID.
 *
 * @param id The Type ID.
 * @return The string name, or NULL if the ID is invalid.
 */
KS_API ks_str ks_handle_get_id_name(Ks_Handle_Id id);

/**
 * @brief Generates a new unique handle for the specified Type ID.
 *
 * This function automatically increments the internal counter for the given type
 * and combines it with the Type ID to form a complete handle.
 *
 * @param id The Type ID for which to generate a handle.
 * @return A new unique Ks_Handle, or KS_INVALID_HANDLE on failure/overflow.
 */
KS_API Ks_Handle ks_handle_make(Ks_Handle_Id id);

/**
 * @brief Checks if a handle belongs to a specific type.
 * * @param handle The handle to check.
 * @param type_id The expected Type ID.
 * @return ks_true if the handle matches the type, ks_false otherwise.
 */
KS_API ks_bool ks_handle_is_type(Ks_Handle handle, Ks_Handle_Id type_id);

#ifdef __cplusplus
}
#endif

/** @} */