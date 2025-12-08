#include "asset/assets_manager.h"
#include "../../include/filesystem/file_watcher.h"

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
	std::string source_path;
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

	bool reload_asset(Ks_Handle handle);
	bool reload_asset(const std::string& source_path);
	void update();

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

	Ks_FileWatcher get_watcher();

private:
	Ks_FileWatcher file_watcher = nullptr;
	std::unordered_map<std::string, Ks_Handle> path_to_handle;
	std::unordered_map<std::string, Ks_IAsset> assets_interfaces;
	std::unordered_map<Ks_Handle, Ks_AssetEntry> assets_entries;
	std::unordered_map<std::string, Ks_Handle> assets_name_to_handle;

	Ks_Handle_Id asset_type_id;
};

static void on_asset_file_changed(ks_str path, ks_ptr user_data) {
	AssetManager_Impl* am = static_cast<AssetManager_Impl*>(user_data);
	am->reload_asset(path);
}

AssetManager_Impl::AssetManager_Impl() {
	asset_type_id = ks_handle_register("Asset");
	file_watcher = ks_file_watcher_create();
}

AssetManager_Impl::~AssetManager_Impl() {

	ks_file_watcher_destroy(file_watcher);

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
	if (found == assets_entries.end()) return "";
	return found->second.asset_name;
}

std::string& AssetManager_Impl::get_asset_type_from_handle(Ks_Handle handle)
{
	static std::string empty = "";
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


		std::string asset_name = entry.asset_name;

		if (!entry.source_path.empty()) {
			ks_file_watcher_unwatch_file(file_watcher, entry.source_path.c_str());
			path_to_handle.erase(entry.source_path);
		}

		assets_name_to_handle.erase(entry.asset_name);
		assets_entries.erase(found);
		
	}
}

Ks_FileWatcher AssetManager_Impl::get_watcher()
{
	return file_watcher;
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

	if (!entry.source_path.empty()) {
		path_to_handle[entry.source_path] = handle;
	}
}

void AssetManager_Impl::update() {
	ks_file_watcher_poll(file_watcher);
}

bool AssetManager_Impl::reload_asset(Ks_Handle handle) {
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return false;

	Ks_AssetEntry& entry = found->second;
	if (entry.source_path.empty()) return false;

	auto interface = get_asset_interface(entry.type_name);
	if (!interface.load_from_file_fn) return false;

	Ks_AssetData new_data = interface.load_from_file_fn(entry.source_path.c_str());
	if (new_data == KS_INVALID_ASSET_DATA) {
		return false;
	}

	if (entry.data && interface.destroy_fn) {
		interface.destroy_fn(entry.data);
	}

	entry.data = new_data;

	return true;
}

bool AssetManager_Impl::reload_asset(const std::string& source_path) {

	auto found = path_to_handle.find(source_path);
	if (found == path_to_handle.end()) return false;

	auto entry_found = assets_entries.find(found->second);
	if (entry_found == assets_entries.end()) return false;

	Ks_AssetEntry& entry = entry_found->second;

	auto interface = get_asset_interface(entry.type_name);
	if (!interface.load_from_file_fn) return false;

	Ks_AssetData new_data = interface.load_from_file_fn(entry.source_path.c_str());
	if (new_data == KS_INVALID_ASSET_DATA) {
		return false;
	}

	if (entry.data && interface.destroy_fn) {
		interface.destroy_fn(entry.data);
	}

	entry.data = new_data;

	return true;
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
	am = reinterpret_cast<AssetManager_Impl*>(
		ks_alloc(
			sizeof(AssetManager_Impl),
			KS_LT_USER_MANAGED,
			KS_TAG_INTERNAL_DATA
		)
	);

	new (am) AssetManager_Impl();

	return am;
}

KS_API ks_no_ret ks_assets_manager_destroy(Ks_AssetsManager am)
{
	if (am) {
		static_cast<AssetManager_Impl*>(am)->~AssetManager_Impl();
		ks_dealloc(am);
	}
}

KS_API ks_no_ret ks_assets_manager_register_asset_type(Ks_AssetsManager am, ks_str type_name, Ks_IAsset asset_interface)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);
	iam->register_interface(type_name, asset_interface);
}

KS_API Ks_Handle ks_assets_manager_load_asset_from_file(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, ks_str file_path)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);
	
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
	entry.source_path = file_path;
	entry.ref_count = 1;

	iam->register_asset(handle, entry);

	ks_file_watcher_watch_file(iam->get_watcher(), file_path, on_asset_file_changed, iam);

	return handle;
}

KS_API Ks_Handle ks_assets_manager_load_asset_from_data(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, const Ks_UserData data)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);

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
	entry.source_path = "";
	entry.ref_count = 1;

	iam->register_asset(handle, entry);

	return handle;
}

KS_API ks_no_ret ks_assets_manager_update(Ks_AssetsManager am) {
	static_cast<AssetManager_Impl*>(am)->update();
}

KS_API ks_bool ks_assets_manager_reload_asset(Ks_AssetsManager am, Ks_Handle handle) {
	if (handle == KS_INVALID_HANDLE) return false;
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);
	return iam->reload_asset(handle);
}

KS_API Ks_Handle ks_assets_manager_get_asset(Ks_AssetsManager am, ks_str asset_name)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);

	auto asset_handle = iam->get_asset(asset_name);

	if (asset_handle == KS_INVALID_HANDLE) {
		return KS_INVALID_HANDLE;
	}

	iam->acquire_asset(asset_handle);

	return asset_handle;
}

KS_API Ks_AssetData ks_assets_manager_get_data(Ks_AssetsManager am, Ks_Handle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);

	if (handle == KS_INVALID_HANDLE) {
		return nullptr;
	}

	return iam->get_asset_data_from_handle(handle);
}

KS_API ks_str ks_assets_manager_get_type_name(Ks_AssetsManager am, Ks_Handle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);
	if (handle == KS_INVALID_HANDLE) return nullptr;
	return iam->get_asset_type_from_handle_raw(handle);
}

KS_API ks_uint32 ks_assets_manager_get_ref_count(Ks_AssetsManager am, Ks_Handle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);
	return iam->get_asset_ref_count_from_handle(handle);
}

ks_no_ret ks_assets_manager_asset_release(Ks_AssetsManager am, Ks_Handle handle)
{
	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);

	iam->release_asset(handle);
}

ks_bool ks_assets_is_handle_valid(Ks_AssetsManager am, Ks_Handle handle)
{
	if (handle == KS_INVALID_HANDLE) return false;

	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);

	return iam->is_handle_valid(handle);
}
