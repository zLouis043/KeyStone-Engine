/**
 * @file serializer.h
 * @brief JSON Serialization and Deserialization module.
 * * Uses an opaque handle approach where the Serializer instance owns all memory
 * for the JSON nodes created. This avoids manual memory management for intermediate nodes.
 * @ingroup Serialization
 */
#pragma once

#include "../core/defines.h"
#include "../core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to the serializer context. Owns the DOM. */
typedef ks_ptr Ks_Serializer;

/** @brief Opaque handle to a JSON node (Value). Valid only as long as Serializer is alive. */
typedef ks_ptr Ks_Json;

typedef enum {
    KS_JSON_NULL = 0,
    KS_JSON_BOOLEAN,
    KS_JSON_OBJECT,
    KS_JSON_ARRAY,
    KS_JSON_STRING,
    KS_JSON_NUMBER
} Ks_JsonType;

/**
 * @brief Creates a new serializer.
 * Initializes an empty document and memory pool.
 */
KS_API Ks_Serializer ks_serializer_create();

/**
 * @brief Destroys the serializer.
 * Frees the document and ALL JSON nodes created by this serializer.
 */
KS_API ks_no_ret ks_serializer_destroy(Ks_Serializer ser);

// --- IO Operations ---
KS_API ks_bool ks_serializer_load_from_file(Ks_Serializer ser, ks_str path);
KS_API ks_bool ks_serializer_load_from_string(Ks_Serializer ser, ks_str json_string);

/**
 * @brief Dumps the current document to a string.
 * @return Pointer to an internal buffer containing the JSON string. Valid until next dump/destroy.
 */
KS_API ks_str ks_serializer_dump_to_string(Ks_Serializer ser);
KS_API ks_bool ks_serializer_dump_to_file(Ks_Serializer ser, ks_str path);

/**
 * @brief Gets the root node of the JSON document.
 */
KS_API Ks_Json ks_serializer_get_root(Ks_Serializer ser);

// --- Factory Methods ---
// Create detached nodes owned by the serializer. 
// They must be added to the tree via object_add/array_push to be part of the output.
KS_API Ks_Json ks_json_create_object(Ks_Serializer ser);
KS_API Ks_Json ks_json_create_array(Ks_Serializer ser);
KS_API Ks_Json ks_json_create_null(Ks_Serializer ser);
KS_API Ks_Json ks_json_create_bool(Ks_Serializer ser, ks_bool val);
KS_API Ks_Json ks_json_create_number(Ks_Serializer ser, ks_double val);
KS_API Ks_Json ks_json_create_string(Ks_Serializer ser, ks_str val);

// --- Manipulation & Access ---

KS_API Ks_JsonType ks_json_get_type(Ks_Json json);

/**
 * @brief Adds a property to a JSON object.
 * Moves 'value' into 'obj'. 'value' handle remains valid but points to null/empty after move.
 */
KS_API ks_no_ret ks_json_object_add(Ks_Serializer ser, Ks_Json obj, ks_str key, Ks_Json value);
KS_API ks_bool ks_json_object_has(Ks_Json obj, ks_str key);
KS_API Ks_Json ks_json_object_get(Ks_Json obj, ks_str key);

/**
 * @brief Pushes an element to a JSON array.
 * Moves 'value' into 'arr'.
 */
KS_API ks_no_ret ks_json_array_push(Ks_Serializer ser, Ks_Json arr, Ks_Json value);
KS_API ks_size ks_json_array_size(Ks_Json arr);
KS_API Ks_Json ks_json_array_get(Ks_Json arr, ks_size index);

// Primitive Getters
KS_API ks_double ks_json_get_number(Ks_Json json);
KS_API ks_bool ks_json_get_bool(Ks_Json json);
KS_API ks_str ks_json_get_string(Ks_Json json);

#ifdef __cplusplus
}
#endif