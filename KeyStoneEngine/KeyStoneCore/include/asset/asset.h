#pragma once

#include "../core/defines.h"
#include "../core/types.h"

#include <stdint.h>

typedef ks_uint32 Ks_AssetHandle;
typedef ks_ptr Ks_AssetData;

#define KS_INVALID_ASSET_HANDLE 0
#define KS_INVALID_ASSET_DATA ((void *)0)

typedef Ks_AssetData (*asset_load_from_file_fn)(ks_str file_path);
typedef Ks_AssetData (*asset_load_from_data_fn)(const ks_byte* data);
typedef ks_no_ret (*asset_destroy_fn)(Ks_AssetData asset);

struct Ks_IAsset {
  asset_load_from_file_fn load_from_file_fn;
  asset_load_from_data_fn load_from_data_fn;
  asset_destroy_fn destroy_fn;
};
