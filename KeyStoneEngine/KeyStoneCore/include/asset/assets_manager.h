/**
 * @file assets_manager.h
 * @brief Resource management system.
 * Handles loading, caching, reference counting, and hot-reloading of assets.
 * @ingroup Assets
 */
#pragma once

#include "asset.h"
#include "../core/handle.h"
#include "../job/job.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Opaque handle to the Assets Manager instance.
*/
typedef ks_ptr Ks_AssetsManager;

/**
 * @brief Creates a new Assets Manager.
 * Initializes the internal file watcher for hot-reloading.
 */
KS_API Ks_AssetsManager ks_assets_manager_create();

/**
 * @brief Destroys the manager and releases all loaded assets.
 * Ensures all `destroy_fn` callbacks are invoked for active assets.
 */
KS_API ks_no_ret ks_assets_manager_destroy(Ks_AssetsManager am);

/**
 * @brief Registers a new asset type.
 * * @param am The manager instance.
 * @param type_name Unique string identifier (e.g., "Texture", "Sound").
 * @param asset_interface VTable containing load/destroy callbacks.
 */
KS_API ks_no_ret ks_assets_manager_register_asset_type(Ks_AssetsManager am, ks_str type_name, Ks_IAsset asset_interface);

/**
 * @brief Loads an asset from disk.
 *
 * If the asset is already loaded (matched by name), its reference count is incremented
 * and the existing handle is returned.
 *
 * @param am The manager instance.
 * @param type_name Registered type name.
 * @param asset_name Unique name/ID for the asset.
 * @param file_path Path to the source file.
 * @return Handle to the asset, or KS_INVALID_HANDLE on failure.
 */
KS_API Ks_Handle ks_assets_manager_load_asset_from_file(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, ks_str file_path);

/**
 * @brief Loads an asset from memory buffer.
 * Useful for procedural assets or embedded resources.
 */
KS_API Ks_Handle ks_assets_manager_load_asset_from_data(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, const Ks_UserData data);

/**
 * @brief Starts asyncronous loading of an asset.
 * @param am Asset Manager
 * @param type_name Type of the asset
 * @param asset_name Unique name/ID for the asset.
 * @param file_path Path to the source file.
 * @param js the Job System needed for the worker thread 
 * @return Handle to the asset, or KS_INVALID_HANDLE on failure. Use ks_assets_get_state to check whenever it is ready or not.
 */
KS_API Ks_Handle ks_assets_manager_load_async(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, ks_str file_path, Ks_JobManager js);

/**
 * @brief Polling function for hot-reloading.
 * Checks file watcher status and reloads modified assets automatically.
 * Should be called once per frame.
 */
KS_API ks_no_ret ks_assets_manager_update(Ks_AssetsManager am);

/**
 * @brief Manually reloads an asset.
 * Re-reads the file from disk and updates the internal data pointer while keeping the handle valid.
 */
KS_API ks_bool ks_assets_manager_reload_asset(Ks_AssetsManager am, Ks_Handle handle);

/**
 * @brief Retrieves an existing asset handle by name.
 * @note Increments the reference count. Caller must eventually call ks_assets_manager_asset_release.
 */
KS_API Ks_Handle ks_assets_manager_get_asset(Ks_AssetsManager am, ks_str asset_name);

/**
 * @brief Gets the raw data pointer for an asset.
 * @return Pointer to the asset data structure, or NULL if handle is invalid.
 */
KS_API Ks_AssetData ks_assets_manager_get_data(Ks_AssetsManager am, Ks_Handle handle);

/**
 * @brief Gets the registered type name of the asset (e.g. "Texture", "Sound").
 * @param am The Assets Manager.
 * @param handle The asset handle.
 * @return The type name string, or NULL if invalid.
 */
KS_API ks_str ks_assets_manager_get_type_name(Ks_AssetsManager am, Ks_Handle handle);

/**
 * @brief Gets the current reference count of an asset.
 *
 * @param am The Assets Manager.
 * @param handle The asset handle.
 * @return The number of references to the asset.
 */
KS_API uint32_t ks_assets_manager_get_ref_count(Ks_AssetsManager am,
    Ks_Handle handle);

/**
 * @brief Decrements the reference count of an asset.
 * If the count reaches zero, the asset is unloaded and its memory freed.
 */
KS_API ks_no_ret ks_assets_manager_asset_release(Ks_AssetsManager am, Ks_Handle handle);

/**
 * @brief Checks if a handle refers to a valid, loaded asset.
 */
KS_API ks_bool ks_assets_is_handle_valid(Ks_AssetsManager am, Ks_Handle handle);

/**
 * @brief Gets the state of an asset loading asynchrously
 */
KS_API Ks_AssetState ks_assets_get_state(Ks_AssetsManager am, Ks_Handle handle);

#ifdef __cplusplus
}
#endif


/** @} */