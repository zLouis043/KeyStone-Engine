#pragma once 

#include "core/defines.h"

#include <stdint.h>

typedef uint32_t Ks_AssetHandle;
typedef void* Ks_AssetData;

#define KS_INVALID_ASSET_HANDLE 0
#define KS_INVALID_ASSET_DATA ((void*)0)

typedef Ks_AssetData (*asset_load_from_file_fn)(const char* file_path);
typedef Ks_AssetData (*asset_load_from_data_fn)(const uint8_t* data);
typedef void   (*asset_destroy_fn)(Ks_AssetData asset);

struct Ks_IAsset {
	asset_load_from_file_fn load_from_file_fn;
	asset_load_from_data_fn load_from_data_fn;
	asset_destroy_fn destroy_fn;
};
