#include "asset/assets_manager.h"

#include <string>
#include <unordered_map>
#include <tuple>

#include <assert.h>
#include <format>

#include "memory/memory.h"
#include "core/log.h"

typedef struct Ks_AssetEntry {
	Ks_AssetData data;
	std::string asset_name;
	std::string type_name;
	uint32_t ref_count;
} Ks_AssetEntry;

class AssetManager_Impl {
public:

	AssetManager_Impl();
	~AssetManager_Impl();

	void register_interface(
		const std::string& type_name,
		Ks_IAsset asset_interface
	);

	Ks_IAsset get_asset_interface(const std::string& type_name);
	void register_asset(Ks_AssetHandle handle, Ks_AssetEntry& entry);

	Ks_AssetHandle get_asset(const std::string& asset_name);
	Ks_AssetHandle generate_handle();

	Ks_AssetData get_asset_data_from_handle(Ks_AssetHandle handle);
	std::string  get_asset_name_from_handle(Ks_AssetHandle handle);
	std::string  get_asset_type_from_handle(Ks_AssetHandle handle);

	bool is_handle_valid(Ks_AssetHandle handle);

	void acquire_asset(Ks_AssetHandle handle);
	void release_asset(Ks_AssetHandle handle);

private:
	std::unordered_map<std::string, Ks_IAsset> assets_interfaces;
	std::unordered_map<Ks_AssetHandle, Ks_AssetEntry> assets_entries;
	std::unordered_map<std::string, Ks_AssetHandle> assets_name_to_handle;

	std::atomic<uint32_t> next_handle;
};

AssetManager_Impl::AssetManager_Impl() : next_handle(1) {}

AssetManager_Impl::~AssetManager_Impl() {
	for (auto& [handle, entry] : assets_entries) {
		if (entry.data) {
			auto interface = get_asset_interface(entry.type_name);
			if (interface.asset_destroy_fn) {
				interface.asset_destroy_fn(entry.data);
			}
		}
	}
	assets_entries.clear();
}


Ks_AssetHandle AssetManager_Impl::generate_handle() {
	return next_handle.fetch_add(1, std::memory_order_relaxed);
}

Ks_AssetData AssetManager_Impl::get_asset_data_from_handle(Ks_AssetHandle handle)
{
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return nullptr;
	return found->second.data;
}

std::string AssetManager_Impl::get_asset_name_from_handle(Ks_AssetHandle handle)
{
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return "nullptr";
	return found->second.asset_name;
}

std::string AssetManager_Impl::get_asset_type_from_handle(Ks_AssetHandle handle)
{
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return "nullptr";
	return found->second.type_name;
}

bool AssetManager_Impl::is_handle_valid(Ks_AssetHandle handle)
{
	return assets_entries.find(handle) == assets_entries.end();
}

void AssetManager_Impl::acquire_asset(Ks_AssetHandle handle)
{
	auto found = assets_entries.find(handle);
	if (found != assets_entries.end()) {
		found->second.ref_count++;
	}
}

void AssetManager_Impl::release_asset(Ks_AssetHandle handle)
{
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return;

	Ks_AssetEntry& entry = found->second;
	entry.ref_count--;

	if (entry.ref_count == 0) {

		if (entry.data) {
			auto interface = get_asset_interface(entry.type_name);
			if (interface.asset_destroy_fn) {
				interface.asset_destroy_fn(entry.data);
			}
		}

		assets_entries.erase(found);

		std::string asset_name = get_asset_name_from_handle(handle);
		assets_name_to_handle.erase(asset_name);
	}
}

void AssetManager_Impl::register_interface(const std::string& type_name, Ks_IAsset asset_interface)
{
	assets_interfaces.emplace(type_name, asset_interface);
}

Ks_IAsset AssetManager_Impl::get_asset_interface(const std::string& type_name)
{
	auto found = assets_interfaces.find(type_name);
	if (found == assets_interfaces.end()) return Ks_IAsset{ 0 };
	return found->second;
}

void AssetManager_Impl::register_asset(Ks_AssetHandle handle, Ks_AssetEntry& entry)
{
	assets_entries.emplace(handle, entry);
	assets_name_to_handle.emplace(entry.asset_name, handle);
}

Ks_AssetHandle AssetManager_Impl::get_asset(const std::string& asset_name)
{
	auto found = assets_name_to_handle.find(asset_name);
	if (found == assets_name_to_handle.end()) return KS_INVALID_ASSET_HANDLE;
	return found->second;
}


KS_API Ks_AssetsManager ks_assets_manager_create()
{
	Ks_AssetsManager am;
	am.impl = reinterpret_cast<AssetManager_Impl*>(
		ks_alloc(
			sizeof(AssetManager_Impl),
			KS_LT_USER_MANAGED,
			KS_TAG_INTERNAL_DATA
		)
	);

	new (am.impl) AssetManager_Impl();

	return am;
}

KS_API void ks_assets_manager_destroy(Ks_AssetsManager am)
{
	if (am.impl) {
		ks_dealloc(am.impl);
	}
}

KS_API void ks_assets_manager_register_asset_type(Ks_AssetsManager am, const char* type_name, Ks_IAsset asset_interface)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);
	iam->register_interface(type_name, asset_interface);
}

KS_API Ks_AssetHandle ks_assets_manager_load_asset_from_file(Ks_AssetsManager am, const char* type_name, const char* asset_name, const char* file_path)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);
	
	auto asset_handle = iam->get_asset(asset_name);

	if (asset_handle != KS_INVALID_ASSET_HANDLE) {
		iam->acquire_asset(asset_handle);
		return asset_handle;
	}

	Ks_IAsset iasset = iam->get_asset_interface(type_name);

	if (!iasset.load_from_file_fn) {
		KS_LOG_ERROR("Asset Interface for asset: '%s' does not have a load_from_file fn", asset_name);
		return KS_INVALID_ASSET_HANDLE;
	}

	Ks_AssetData asset_data = iasset.load_from_file_fn(file_path);

	if (!asset_data) {
		return KS_INVALID_ASSET_HANDLE;
	}

	Ks_AssetHandle handle = iam->generate_handle();
	Ks_AssetEntry entry;
	entry.data = asset_data;
	entry.asset_name = asset_name;
	entry.type_name = type_name;

	iam->register_asset(handle, entry);

	return handle;
}

KS_API Ks_AssetHandle ks_assets_manager_load_asset_from_data(Ks_AssetsManager am, const char* type_name, const char* asset_name, const uint8_t* data)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	auto asset_handle = iam->get_asset(asset_name);

	if (asset_handle != KS_INVALID_ASSET_HANDLE) {
		iam->acquire_asset(asset_handle);
		return asset_handle;
	}

	Ks_IAsset iasset = iam->get_asset_interface(type_name);

	if (!iasset.load_from_file_fn) {
		KS_LOG_ERROR("Asset Interface for asset: '%s' does not have a load_from_data fn", asset_name);
		return KS_INVALID_ASSET_HANDLE;
	}

	Ks_AssetData asset_data = iasset.load_from_data_fn(data);

	if (!asset_data) {
		return KS_INVALID_ASSET_HANDLE;
	}

	Ks_AssetHandle handle = iam->generate_handle();
	Ks_AssetEntry entry;
	entry.data = asset_data;
	entry.asset_name = std::string(asset_name);
	entry.type_name = std::string(type_name);

	iam->register_asset(handle, entry);

	return handle;
}

KS_API Ks_AssetData ks_assets_manager_get_data(Ks_AssetsManager am, Ks_AssetHandle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	if (handle == KS_INVALID_ASSET_HANDLE) {
		return nullptr;
	}

	return iam->get_asset_data_from_handle(handle);
}

void ks_assets_manager_asset_release(Ks_AssetsManager am, Ks_AssetHandle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	iam->release_asset(handle);
}

KS_API void ks_assets_manager_asset_unload(Ks_AssetsManager am, const char* asset_name)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	auto asset_handle = iam->get_asset(asset_name);

	if (asset_handle == KS_INVALID_ASSET_HANDLE) {
		return;
	}

	iam->release_asset(asset_handle);
}

bool ks_assets_is_handle_valid(Ks_AssetsManager am, Ks_AssetHandle handle)
{
	if (handle == KS_INVALID_ASSET_HANDLE) return false;

	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	return iam->is_handle_valid(handle);
}
