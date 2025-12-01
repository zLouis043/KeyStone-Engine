#include "asset/assets_manager.h"

#include <string>
#include <unordered_map>
#include <tuple>

#include <assert.h>
#include <atomic>

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
	void register_asset(Ks_Handle handle, Ks_AssetEntry& entry);

	Ks_Handle get_asset(const std::string& asset_name);
	Ks_Handle generate_handle();

	Ks_AssetData get_asset_data_from_handle(Ks_Handle handle);
	std::string  get_asset_name_from_handle(Ks_Handle handle);
	std::string&  get_asset_type_from_handle(Ks_Handle handle);
	const char* get_asset_type_from_handle_raw(Ks_Handle handle);
	uint32_t  get_asset_ref_count_from_handle(Ks_Handle handle);

	bool is_handle_valid(Ks_Handle handle);

	void acquire_asset(Ks_Handle handle);
	void release_asset(Ks_Handle handle);

private:
	std::unordered_map<std::string, Ks_IAsset> assets_interfaces;
	std::unordered_map<Ks_Handle, Ks_AssetEntry> assets_entries;
	std::unordered_map<std::string, Ks_Handle> assets_name_to_handle;

	Ks_Handle_Id asset_type_id;
};

AssetManager_Impl::AssetManager_Impl() {
	asset_type_id = ks_handle_register("Asset");
}

AssetManager_Impl::~AssetManager_Impl() {
	for (auto& [handle, entry] : assets_entries) {
		if (entry.data) {
			auto interface = get_asset_interface(entry.type_name);
			if (interface.destroy_fn) {
				interface.destroy_fn(entry.data);
			}
		}
	}
	assets_entries.clear();
}


Ks_Handle AssetManager_Impl::generate_handle() {
	return ks_handle_make(asset_type_id);
}

Ks_AssetData AssetManager_Impl::get_asset_data_from_handle(Ks_Handle handle)
{
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return nullptr;
	return found->second.data;
}

std::string AssetManager_Impl::get_asset_name_from_handle(Ks_Handle handle)
{
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return "nullptr";
	return found->second.asset_name;
}

std::string& AssetManager_Impl::get_asset_type_from_handle(Ks_Handle handle)
{
	static std::string empty = "nullptr";
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return empty;
	return found->second.type_name;
}

const char* AssetManager_Impl::get_asset_type_from_handle_raw(Ks_Handle handle) {
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return nullptr;
	return found->second.type_name.c_str();
}

uint32_t AssetManager_Impl::get_asset_ref_count_from_handle(Ks_Handle handle)
{
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return 0;
	return found->second.ref_count;
}

bool AssetManager_Impl::is_handle_valid(Ks_Handle handle)
{
	if (!ks_handle_is_type(handle, asset_type_id)) return false;
	return assets_entries.find(handle) != assets_entries.end();
}

void AssetManager_Impl::acquire_asset(Ks_Handle handle)
{
	auto found = assets_entries.find(handle);
	if (found != assets_entries.end()) {
		found->second.ref_count++;
	}
}

void AssetManager_Impl::release_asset(Ks_Handle handle)
{
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return;

	Ks_AssetEntry& entry = found->second;
	entry.ref_count--;

	if (entry.ref_count == 0) {

		if (entry.data) {
			auto interface = get_asset_interface(entry.type_name);
			if (interface.destroy_fn) {
				interface.destroy_fn(entry.data);
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

void AssetManager_Impl::register_asset(Ks_Handle handle, Ks_AssetEntry& entry)
{
	assets_entries.emplace(handle, entry);
	assets_name_to_handle.emplace(entry.asset_name, handle);
}

Ks_Handle AssetManager_Impl::get_asset(const std::string& asset_name)
{
	auto found = assets_name_to_handle.find(asset_name);
	if (found == assets_name_to_handle.end()) return KS_INVALID_HANDLE;
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

KS_API ks_no_ret ks_assets_manager_destroy(Ks_AssetsManager am)
{
	if (am.impl) {
		ks_dealloc(am.impl);
	}
}

KS_API ks_no_ret ks_assets_manager_register_asset_type(Ks_AssetsManager am, ks_str type_name, Ks_IAsset asset_interface)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);
	iam->register_interface(type_name, asset_interface);
}

KS_API Ks_Handle ks_assets_manager_load_asset_from_file(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, ks_str file_path)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);
	
	auto asset_handle = iam->get_asset(asset_name);

	if (asset_handle != KS_INVALID_HANDLE) {
		iam->acquire_asset(asset_handle);
		return asset_handle;
	}

	Ks_IAsset iasset = iam->get_asset_interface(type_name);

	if (!iasset.load_from_file_fn) {
		KS_LOG_ERROR("Asset Interface for asset: '%s' does not have a load_from_file fn", asset_name);
		return KS_INVALID_HANDLE;
	}

	Ks_AssetData asset_data = iasset.load_from_file_fn(file_path);

	if (asset_data == KS_INVALID_ASSET_DATA) {
		return KS_INVALID_HANDLE;
	}

	Ks_Handle handle = iam->generate_handle();
	Ks_AssetEntry entry;
	entry.data = asset_data;
	entry.asset_name = asset_name;
	entry.type_name = type_name;
	entry.ref_count = 1;

	iam->register_asset(handle, entry);

	return handle;
}

KS_API Ks_Handle ks_assets_manager_load_asset_from_data(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, const ks_byte* data)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	auto asset_handle = iam->get_asset(asset_name);

	if (asset_handle != KS_INVALID_HANDLE) {
		iam->acquire_asset(asset_handle);
		return asset_handle;
	}

	Ks_IAsset iasset = iam->get_asset_interface(type_name);

	if (!iasset.load_from_file_fn) {
		KS_LOG_ERROR("Asset Interface for asset: '%s' does not have a load_from_data fn", asset_name);
		return KS_INVALID_HANDLE;
	}

	Ks_AssetData asset_data = iasset.load_from_data_fn(data);

	if (asset_data == KS_INVALID_ASSET_DATA) {
		return KS_INVALID_HANDLE;
	}

	Ks_Handle handle = iam->generate_handle();
	Ks_AssetEntry entry;
	entry.data = asset_data;
	entry.asset_name = asset_name;
	entry.type_name = type_name;
	entry.ref_count = 1;

	iam->register_asset(handle, entry);

	return handle;
}

KS_API Ks_Handle ks_assets_manager_get_asset(Ks_AssetsManager am, ks_str asset_name)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	auto asset_handle = iam->get_asset(asset_name);

	if (asset_handle == KS_INVALID_HANDLE) {
		return KS_INVALID_HANDLE;
	}

	iam->acquire_asset(asset_handle);

	return asset_handle;
}

KS_API Ks_AssetData ks_assets_manager_get_data(Ks_AssetsManager am, Ks_Handle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	if (handle == KS_INVALID_HANDLE) {
		return nullptr;
	}

	return iam->get_asset_data_from_handle(handle);
}

KS_API ks_str ks_assets_manager_get_type_name(Ks_AssetsManager am, Ks_Handle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);
	if (handle == KS_INVALID_HANDLE) return nullptr;
	return iam->get_asset_type_from_handle_raw(handle);
}

KS_API ks_uint32 ks_assets_manager_get_ref_count(Ks_AssetsManager am, Ks_Handle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);
	return iam->get_asset_ref_count_from_handle(handle);
}

ks_no_ret ks_assets_manager_asset_release(Ks_AssetsManager am, Ks_Handle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	iam->release_asset(handle);
}

ks_bool ks_assets_is_handle_valid(Ks_AssetsManager am, Ks_Handle handle)
{
	if (handle == KS_INVALID_HANDLE) return false;

	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am.impl);

	return iam->is_handle_valid(handle);
}
