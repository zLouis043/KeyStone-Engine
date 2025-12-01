#pragma once

/**
 * @file asset.h
 * @brief Base interfaces for assets definition.
 * @ingroup Assets
 */

#include "../core/defines.h"
#include "../core/types.h"

#include <stdint.h>

/**
 * @brief Generic pointer to asset data.
 */
typedef ks_ptr Ks_AssetData;

#define KS_INVALID_ASSET_DATA ((void *)0)

/**
 * @brief Function pointer type for loading an asset from a file path.
 * @param file_path Path to the asset file.
 * @return Pointer to the loaded asset data, or NULL on failure.
 */
typedef Ks_AssetData (*asset_load_from_file_fn)(ks_str file_path);

/**
 * @brief Function pointer type for loading an asset from raw memory data.
 * @param data Pointer to the raw byte data.
 * @return Pointer to the loaded asset data, or NULL on failure.
 */
typedef Ks_AssetData (*asset_load_from_data_fn)(const ks_byte* data);

/**
 * @brief Function pointer type for destroying/freeing an asset.
 * @param asset Pointer to the asset data to destroy.
 */
typedef ks_no_ret (*asset_destroy_fn)(Ks_AssetData asset);

/**
 * @brief Interface defining the lifecycle methods for a specific asset type.
 */
struct Ks_IAsset {
  asset_load_from_file_fn load_from_file_fn; ///< Callback to load asset from file.
  asset_load_from_data_fn load_from_data_fn; ///< Callback to load asset from memory.
  asset_destroy_fn destroy_fn;               ///< Callback to destroy the asset.
};