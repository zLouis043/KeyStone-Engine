#pragma once 

#include <string>
#include <unordered_map>

#include <filesystem>

#include "memory.h"
#include "result.h"

class IAsset {
public:
    virtual ~IAsset() = default;
    
    virtual bool load_from_file(const std::string& file_path) = 0;
    virtual bool load_from_data(const void* data, size_t size) = 0;
    virtual bool is_valid() const = 0;
    virtual std::string get_error() const = 0;
    virtual std::string get_path() const = 0;
    virtual std::string get_asset_type() const = 0;
    
protected:
    std::string error_message;
    std::string file_path;
    bool valid = false;
};

struct lua_State;
class TypeManager;

class AssetsManager {
private:
    std::unordered_map<std::string, ke::shared_ptr<IAsset>> assets;
    
    std::string extract_asset_id_from_path(const std::string& path) {
        std::filesystem::path fs_path(path);
        return fs_path.filename().string();
    }

public:
    
    template<typename T>
    Result<ke::shared_ptr<T>> load_asset(const std::string& asset_id, const std::string& file_path) {
        static_assert(std::is_base_of_v<IAsset, T>, "T must inherit from IAsset");

        auto it = assets.find(asset_id);
        if (it != assets.end()) {
            auto existing_asset = std::dynamic_pointer_cast<T>(it->second);
            if (existing_asset && existing_asset->is_valid()) {
                LOG_INFO("Asset '{}' already loaded, returning existing instance", asset_id);
                return Result<ke::shared_ptr<T>>::Ok(existing_asset);
            } else {
                assets.erase(it);
            }
        }
        
        auto asset = ke::make_shared<T>(
            MemoryManager::RESOURCE,
            ("Asset:" + asset_id).c_str()
        );
        
        if (!asset) {
            return Result<ke::shared_ptr<T>>::Err("Failed to allocate memory for asset '{}'", asset_id);
        }
        
        bool success = asset->load_from_file(file_path);
        
        if (success && asset->is_valid()) {
            assets[asset_id] = asset;
            return Result<ke::shared_ptr<T>>::Ok(asset);
        } else {
            return Result<ke::shared_ptr<T>>::Err(
                "Failed to load asset '{}' from '{}': {}", 
                            asset_id, file_path, asset->get_error()
            );
        }
    }
    
    template<typename T>
    Result<ke::shared_ptr<T>> load_asset(const std::string& file_path) {
        std::string asset_id = extract_asset_id_from_path(file_path);
        return load_asset<T>(asset_id, file_path);
    }
    
    template<typename T>
    Result<ke::shared_ptr<T>> load_asset_from_data(const std::string& asset_id, 
                                         const void* data, size_t size) {
        static_assert(std::is_base_of_v<IAsset, T>, "T must inherit from IAsset");
        
        auto asset = ke::make_shared<T>(
            MemoryManager::RESOURCE,
            ("Asset:" + asset_id).c_str()
        );
        
        if (!asset) {
            return Result<ke::shared_ptr<T>>::Err("Failed to allocate memory for asset '{}'", asset_id);
        }
        
        bool success = asset->load_from_data(data, size);
        
        if (success && asset->is_valid()) {
            assets[asset_id] = asset;
            return Result<ke::shared_ptr<T>>::Ok(asset);
        } else {
            return Result<ke::shared_ptr<T>>::Err(
                "Failed to load asset '{}' from '<data>': {}", 
                            asset_id, asset->get_error()
            );
        }
    }
    
    template<typename T>
    Result<ke::shared_ptr<T>> get_asset(const std::string& asset_id) {
        static_assert(std::is_base_of_v<IAsset, T>, "T must inherit from IAsset");
        
        auto it = assets.find(asset_id);
        if (it != assets.end()) {
            return Result<ke::shared_ptr<T>>::Ok(std::dynamic_pointer_cast<T>(it->second));
        }

        return Result<ke::shared_ptr<T>>::Err("Asset '{}' not found", asset_id);
    }
    
    bool unload_asset(const std::string& asset_id) {
        auto it = assets.find(asset_id);
        if (it != assets.end()) {
            assets.erase(it);
            return true;
        }
        return false;
    }
    
    bool has_asset(const std::string& asset_id) const {
        return assets.find(asset_id) != assets.end();
    }
    
    std::vector<std::string> get_loaded_assets() const {
        std::vector<std::string> ids;
        ids.reserve(assets.size());
        for (const auto& pair : assets) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
    void clear_all_assets() {
        size_t count = assets.size();
        assets.clear();
    }
    
    size_t get_asset_count() const {
        return assets.size();
    }
    
    void init_lua_bindings(lua_State* lua_state, TypeManager* tm);  
};

#define IASSET_BASE sol::base_classes, sol::bases<IAsset>()