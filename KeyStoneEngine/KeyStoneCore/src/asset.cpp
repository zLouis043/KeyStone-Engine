#include "asset.h"

#include <sol/sol.hpp>

#include "typemanager.h"

void AssetsManager::init_lua_bindings(lua_State* lua_state, TypeManager* tm){
    sol::state_view lua(lua_state);
        
    tm->register_type<IAsset>("IAsset",
        "is_valid", &IAsset::is_valid,
        "get_error", &IAsset::get_error,
        "get_path", &IAsset::get_path,
        "get_asset_type", &IAsset::get_asset_type
    );
    
    tm->register_type<AssetsManager>("AssetsManager",
        "has_asset", &AssetsManager::has_asset,
        "unload_asset", &AssetsManager::unload_asset,
        "clear_all_assets", &AssetsManager::clear_all_assets,
        "get_asset_count", &AssetsManager::get_asset_count,
        "get_loaded_assets", &AssetsManager::get_loaded_assets
    );
    
    sol::table assets_table = lua.create_named_table("assets");
    assets_table["manager"] = this;
}