#pragma once

#include "asset.h"

typedef void *Ks_AssetsManager_Impl;

struct Ks_AssetsManager {
  Ks_AssetsManager_Impl impl;
};

KS_API Ks_AssetsManager ks_assets_manager_create();
KS_API ks_no_ret ks_assets_manager_destroy(Ks_AssetsManager am);

KS_API ks_no_ret ks_assets_manager_register_asset_type(Ks_AssetsManager am,
                                                  ks_str type_name,
                                                  Ks_IAsset asset_interface);

KS_API Ks_AssetHandle ks_assets_manager_load_asset_from_file(
    Ks_AssetsManager am, ks_str type_name, ks_str asset_name,
    ks_str file_path);

KS_API Ks_AssetHandle ks_assets_manager_load_asset_from_data(
    Ks_AssetsManager am, ks_str type_name, ks_str asset_name,
    const ks_byte  *data);

KS_API Ks_AssetHandle ks_assets_manager_get_asset(Ks_AssetsManager am,
    ks_str asset_name);

KS_API Ks_AssetData ks_assets_manager_get_data(Ks_AssetsManager am,
                                               Ks_AssetHandle handle);

KS_API uint32_t ks_assets_manager_get_ref_count(Ks_AssetsManager am,
                                                Ks_AssetHandle handle);

KS_API ks_no_ret ks_assets_manager_asset_release(Ks_AssetsManager am,
                                            Ks_AssetHandle handle);

KS_API ks_bool ks_assets_is_handle_valid(Ks_AssetsManager am,
                                      Ks_AssetHandle handle);
