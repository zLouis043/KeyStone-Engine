#pragma once 

#include "asset/asset.h"

typedef void*  Ks_AssetsManager_Impl;

struct Ks_AssetsManager {
	Ks_AssetsManager_Impl impl;
};

KS_API Ks_AssetsManager ks_assets_manager_create();
KS_API void ks_assets_manager_destroy(Ks_AssetsManager am);

KS_API void ks_assets_manager_register_asset_type(
	Ks_AssetsManager am, 
	const char* type_name, 
	Ks_IAsset asset_interface
);

KS_API Ks_AssetHandle ks_assets_manager_load_asset_from_file(
	Ks_AssetsManager am, 
	const char* type_name, 
	const char* asset_name, 
	const char* file_path
);

KS_API Ks_AssetHandle ks_assets_manager_load_asset_from_data(
	Ks_AssetsManager am, 
	const char* type_name, 
	const char* asset_name, 
	const uint8_t* data
);

KS_API Ks_AssetData ks_assets_manager_get_data(
	Ks_AssetsManager am,
	Ks_AssetHandle handle
);

KS_API void ks_assets_manager_asset_release(
	Ks_AssetsManager am,
	Ks_AssetHandle handle
);

KS_API void ks_assets_manager_asset_unload(
	Ks_AssetsManager am, 
	const char* asset_name
);

KS_API bool ks_assets_is_handle_valid(
	Ks_AssetsManager am,
	Ks_AssetHandle handle
);