/**
 * @file assets_manager.h
 * @brief Central manager that handles loading and caching of resources.
 *
 * @defgroup Assets Asset Management
 * @brief Resource Managing System (loading, caching, release).
 * @{
 */
#pragma once

#include "asset.h"
#include "../core/handle.h"

/**
 * @brief Opaque pointer to the internal implementation of the Assets Manager.
 */
typedef void *Ks_AssetsManager_Impl;

/**
 * @brief Handle structure for the Assets Manager.
 */
struct Ks_AssetsManager {
  Ks_AssetsManager_Impl impl;
};

/**
 * @brief Creates a new instance of the Assets Manager.
 * @return A new Assets Manager handle.
 */
KS_API Ks_AssetsManager ks_assets_manager_create();

/**
 * @brief Destroys an Assets Manager instance and all managed assets.
 * @param am The manager to destroy.
 */
KS_API ks_no_ret ks_assets_manager_destroy(Ks_AssetsManager am);

/**
 * @brief Registers a new asset type definition with the manager.
 *
 * @param am The Assets Manager.
 * @param type_name The unique string identifier for the asset type (e.g. "Texture").
 * @param asset_interface The interface containing load/destroy callbacks for this type.
 */
KS_API ks_no_ret ks_assets_manager_register_asset_type(Ks_AssetsManager am,
                                                  ks_str type_name,
                                                  Ks_IAsset asset_interface);

/**
 * @brief Loads an asset from a file.
 *
 * If the asset is already loaded (checked by name), its reference count is incremented.
 *
 * @param am The Assets Manager.
 * @param type_name The type of the asset (must be registered).
 * @param asset_name A unique name to identify this specific asset instance.
 * @param file_path Path to the file on disk.
 * @return Handle to the loaded asset, or KS_INVALID_HANDLE on failure.
 */
KS_API Ks_Handle ks_assets_manager_load_asset_from_file(
    Ks_AssetsManager am, ks_str type_name, ks_str asset_name,
    ks_str file_path);

/**
 * @brief Loads an asset from raw data in memory.
 *
 * @param am The Assets Manager.
 * @param type_name The type of the asset.
 * @param asset_name A unique name to identify this specific asset instance.
 * @param data Pointer to the raw data.
 * @return Handle to the loaded asset, or KS_INVALID_HANDLE on failure.
 */
KS_API Ks_Handle ks_assets_manager_load_asset_from_data(
    Ks_AssetsManager am, ks_str type_name, ks_str asset_name,
    const ks_byte  *data);

/**
 * @brief Retrieves a handle to an already loaded asset by name.
 * Increments the reference count.
 *
 * @param am The Assets Manager.
 * @param asset_name The name of the asset to find.
 * @return Handle to the asset, or KS_INVALID_HANDLE if not found.
 */
KS_API Ks_Handle ks_assets_manager_get_asset(Ks_AssetsManager am,
    ks_str asset_name);

/**
 * @brief Gets the raw data pointer associated with an asset handle.
 *
 * @param am The Assets Manager.
 * @param handle The asset handle.
 * @return Pointer to the asset data structure.
 */
KS_API Ks_AssetData ks_assets_manager_get_data(Ks_AssetsManager am,
    Ks_Handle handle);

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
 * @brief Releases a reference to an asset.
 *
 * If the reference count reaches zero, the asset is destroyed using its registered destroy callback.
 *
 * @param am The Assets Manager.
 * @param handle The asset handle to release.
 */
KS_API ks_no_ret ks_assets_manager_asset_release(Ks_AssetsManager am,
    Ks_Handle handle);

/**
 * @brief Checks if an asset handle is currently valid within the manager.
 *
 * @param am The Assets Manager.
 * @param handle The asset handle.
 * @return ks_true if valid, ks_false otherwise.
 */
KS_API ks_bool ks_assets_is_handle_valid(Ks_AssetsManager am,
    Ks_Handle handle);

/** @} */