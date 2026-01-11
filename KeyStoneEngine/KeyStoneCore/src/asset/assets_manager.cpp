#include "../../include/asset/assets_manager.h"
#include "../../include/filesystem/file_watcher.h"
#include "../../include/profiler/profiler.h"

#include <string>
#include <unordered_map>
#include <map>
#include <tuple>

#include <assert.h>
#include <atomic>
#include <mutex>

#include "memory/memory.h"
#include "core/log.h"

typedef struct Ks_AssetEntry {
	Ks_AssetData data;
	std::string asset_name;
	std::string type_name;
	std::string source_path;
	uint32_t ref_count;
	Ks_AssetState state;
} Ks_AssetEntry;

struct AsyncLoadPayload {
	class AssetManager_Impl* mgr;
	Ks_Handle handle;
	std::string path;
	Ks_IAsset iface;
};
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

	Ks_Handle load_sync(const std::string& type_name, const std::string& name, const std::string& path);
	Ks_Handle load_async(const std::string& type_name, const std::string& name, const std::string& path, Ks_JobManager js);
	Ks_Handle load_from_data(const std::string& type_name, const std::string& name, const Ks_UserData data);

	void complete_async_load(Ks_Handle handle, Ks_AssetData data, bool success, Ks_IAsset original_iface);

	bool reload_asset(Ks_Handle handle);
	bool reload_asset(const std::string& source_path);
	void update();

	Ks_Handle get_asset(const std::string& asset_name);
	Ks_Handle generate_handle();

	Ks_AssetData get_asset_data_from_handle(Ks_Handle handle);
	std::string  get_asset_name_from_handle(Ks_Handle handle);
	std::string  get_asset_type_from_handle(Ks_Handle handle);
	const char* get_asset_type_from_handle_raw(Ks_Handle handle);
	uint32_t  get_asset_ref_count_from_handle(Ks_Handle handle);
	Ks_AssetState get_asset_state_from_handle(Ks_Handle handle);

	Ks_IAsset get_asset_interface_nolock(const std::string& type_name);

	bool is_handle_valid(Ks_Handle handle);

	void acquire_asset(Ks_Handle handle);
	void release_asset(Ks_Handle handle);

	Ks_FileWatcher get_watcher();
	std::string resolve_path(const std::string& input_path);

private:
	std::mutex assets_mutex;
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

AssetManager_Impl::AssetManager_Impl()
	: asset_type_id(KS_INVALID_ID)
	, file_watcher(nullptr)
{
	asset_type_id = ks_handle_register("Asset");
	if (asset_type_id == KS_INVALID_ID) {
		KS_LOG_ERROR("[Assets] Failed to register asset handle type");
	}

	file_watcher = ks_file_watcher_create();
	if (!file_watcher) {
		KS_LOG_ERROR("[Assets] Failed to create file watcher");
	}
}

AssetManager_Impl::~AssetManager_Impl() {
	if (file_watcher) {
		// Disabilita callback prima di distruggere per evitare race conditions
		// ks_file_watcher_stop(file_watcher); // Se esistesse una API simile

		for (auto& [handle, entry] : assets_entries) {
			if (!entry.source_path.empty()) {
				ks_file_watcher_unwatch_file(file_watcher, entry.source_path.c_str());
			}
		}
		ks_file_watcher_destroy(file_watcher);
		file_watcher = nullptr;
	}

	std::vector<std::pair<std::string, Ks_AssetData>> to_destroy;

	{
		std::lock_guard<std::mutex> lock(assets_mutex);

		for (auto& [handle, entry] : assets_entries) {
			if (entry.data) {
				to_destroy.emplace_back(entry.type_name, entry.data);
				entry.data = nullptr;
			}
		}

		assets_entries.clear();
		assets_name_to_handle.clear();
		path_to_handle.clear();
	}

	for (auto& [type_name, data] : to_destroy) {
		auto it = assets_interfaces.find(type_name);
		if (it != assets_interfaces.end() && it->second.destroy_fn) {
			it->second.destroy_fn(data);
		}
	}
}

Ks_Handle AssetManager_Impl::generate_handle() {
	return ks_handle_make(asset_type_id);
}

std::string AssetManager_Impl::resolve_path(const std::string& input_path) {
	if (input_path.find("://") == std::string::npos) {
		return input_path;
	}

	char buffer[1024];
	if (ks_vfs_resolve(input_path.c_str(), buffer, 1024)) {
		return std::string(buffer);
	}

	KS_LOG_WARN("[Assets] Failed to resolve VFS path: %s", input_path.c_str());
	return input_path;
}

Ks_IAsset AssetManager_Impl::get_asset_interface_nolock(const std::string& type_name) {
	auto found = assets_interfaces.find(type_name);
	if (found == assets_interfaces.end()) return Ks_IAsset{ 0 };
	return found->second;
}

Ks_AssetData AssetManager_Impl::get_asset_data_from_handle(Ks_Handle handle)
{
	KS_PROFILE_SCOPE("Asset_GetData_Lock");
	std::lock_guard<std::mutex> lock(assets_mutex);
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return nullptr;
	return found->second.data;
}

std::string AssetManager_Impl::get_asset_name_from_handle(Ks_Handle handle)
{
	std::lock_guard<std::mutex> lock(assets_mutex);
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return "";
	return found->second.asset_name;
}

std::string AssetManager_Impl::get_asset_type_from_handle(Ks_Handle handle)
{
	std::lock_guard<std::mutex> lock(assets_mutex);
	static std::string empty = "";
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return empty;
	return found->second.type_name;
}

const char* AssetManager_Impl::get_asset_type_from_handle_raw(Ks_Handle handle) {
	std::lock_guard<std::mutex> lock(assets_mutex);
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return nullptr;
	return found->second.type_name.c_str();
}

uint32_t AssetManager_Impl::get_asset_ref_count_from_handle(Ks_Handle handle)
{
	std::lock_guard<std::mutex> lock(assets_mutex);
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return 0;
	return found->second.ref_count;
}

bool AssetManager_Impl::is_handle_valid(Ks_Handle handle)
{
	std::lock_guard<std::mutex> lock(assets_mutex);
	if (!ks_handle_is_type(handle, asset_type_id)) return false;
	return assets_entries.find(handle) != assets_entries.end();
}

void AssetManager_Impl::acquire_asset(Ks_Handle handle)
{
	std::lock_guard<std::mutex> lock(assets_mutex);
	auto found = assets_entries.find(handle);
	if (found != assets_entries.end()) {
		found->second.ref_count++;
	}
}

void AssetManager_Impl::release_asset(Ks_Handle handle) {
	std::string type_name_to_destroy;
	Ks_AssetData data_to_destroy = nullptr;
	std::string asset_name_to_remove;
	std::string source_path_to_unwatch;

	{
		std::lock_guard<std::mutex> lock(assets_mutex);
		auto found = assets_entries.find(handle);
		if (found == assets_entries.end()) return;

		Ks_AssetEntry& entry = found->second;
		entry.ref_count--;

		if (entry.ref_count == 0) {
			type_name_to_destroy = entry.type_name;
			data_to_destroy = entry.data;
			asset_name_to_remove = entry.asset_name;
			source_path_to_unwatch = entry.source_path;

			if (!entry.source_path.empty()) {
				path_to_handle.erase(entry.source_path);
			}
			assets_name_to_handle.erase(entry.asset_name);
			assets_entries.erase(found);
		}
	}

	if (!source_path_to_unwatch.empty() && file_watcher) {
		ks_file_watcher_unwatch_file(file_watcher, source_path_to_unwatch.c_str());
	}

	if (data_to_destroy) {
		auto it = assets_interfaces.find(type_name_to_destroy);
		if (it != assets_interfaces.end() && it->second.destroy_fn) {
			it->second.destroy_fn(data_to_destroy);
		}
	}
}

Ks_FileWatcher AssetManager_Impl::get_watcher()
{
	return file_watcher;
}

void AssetManager_Impl::register_interface(const std::string& type_name, Ks_IAsset asset_interface)
{
	std::lock_guard<std::mutex> lock(assets_mutex);
	assets_interfaces.emplace(type_name, asset_interface);
}

Ks_IAsset AssetManager_Impl::get_asset_interface(const std::string& type_name)
{
	std::lock_guard<std::mutex> lock(assets_mutex);
	return get_asset_interface_nolock(type_name);
}

Ks_AssetState AssetManager_Impl::get_asset_state_from_handle(Ks_Handle handle) {
	std::lock_guard<std::mutex> lock(assets_mutex);
	auto found = assets_entries.find(handle);
	if (found == assets_entries.end()) return KS_ASSET_STATE_NONE;
	return found->second.state;
}

void AssetManager_Impl::register_asset(Ks_Handle handle, Ks_AssetEntry& entry)
{
	assets_entries.emplace(handle, entry);
	assets_name_to_handle.emplace(entry.asset_name, handle);

	if (!entry.source_path.empty()) {
		path_to_handle[entry.source_path] = handle;
	}
}

Ks_Handle AssetManager_Impl::load_sync(const std::string& type_name, const std::string& asset_name, const std::string& file_path) {

	KS_PROFILE_FUNCTION();

	std::string final_path = resolve_path(file_path);

	std::lock_guard<std::mutex> lock(assets_mutex);

	auto found_name = assets_name_to_handle.find(asset_name);
	if (found_name != assets_name_to_handle.end()) {
		Ks_Handle h = found_name->second;
		assets_entries[h].ref_count++;
		return h;
	}

	auto it_iface = assets_interfaces.find(type_name);
	if (it_iface == assets_interfaces.end() || !it_iface->second.load_from_file_fn) {
		KS_LOG_ERROR("Asset Interface for asset: '%s' invalid", asset_name.c_str());
		return KS_INVALID_HANDLE;
	}

	Ks_AssetData asset_data = it_iface->second.load_from_file_fn(final_path.c_str());
	if (asset_data == KS_INVALID_ASSET_DATA) {
		return KS_INVALID_HANDLE;
	}

	Ks_Handle handle = generate_handle();
	Ks_AssetEntry entry;
	entry.data = asset_data;
	entry.asset_name = asset_name;
	entry.type_name = type_name;
	entry.source_path = final_path;
	entry.ref_count = 1;
	entry.state = KS_ASSET_STATE_READY;

	register_asset(handle, entry);
	ks_file_watcher_watch_file(file_watcher, file_path.c_str(), on_asset_file_changed, this);

	return handle;
}

Ks_Handle AssetManager_Impl::load_async(const std::string& type_name, const std::string& asset_name, const std::string& file_path, Ks_JobManager js) {

	KS_PROFILE_FUNCTION();

	std::string final_path = resolve_path(file_path);

	std::lock_guard<std::mutex> lock(assets_mutex);

	auto found_name = assets_name_to_handle.find(asset_name);
	if (found_name != assets_name_to_handle.end()) {
		Ks_Handle h = found_name->second;
		assets_entries[h].ref_count++;
		return h;
	}

	auto it_iface = assets_interfaces.find(type_name);
	if (it_iface == assets_interfaces.end() || !it_iface->second.load_from_file_fn) {
		return KS_INVALID_HANDLE;
	}

	Ks_Handle handle = generate_handle();
	Ks_AssetEntry entry;
	entry.data = nullptr;
	entry.asset_name = asset_name;
	entry.type_name = type_name;
	entry.source_path = final_path;
	entry.ref_count = 1;
	entry.state = KS_ASSET_STATE_LOADING;

	register_asset(handle, entry);

	AsyncLoadPayload* payload = new(ks_alloc(sizeof(AsyncLoadPayload), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA)) AsyncLoadPayload();
	payload->mgr = this;
	payload->handle = handle;
	payload->path = final_path;
	payload->iface = it_iface->second;

	auto job_fn = [](Ks_Payload p) {
		KS_PROFILE_SCOPE("Async_Asset_Load_Work");
		AsyncLoadPayload* py = (AsyncLoadPayload*)p.data;
		Ks_AssetData data = py->iface.load_from_file_fn(py->path.c_str());
		py->mgr->complete_async_load(py->handle, data, (data != KS_INVALID_ASSET_DATA), py->iface);
		};

	auto free_fn = [](ks_ptr p) {
		AsyncLoadPayload* py = (AsyncLoadPayload*)p;
		py->~AsyncLoadPayload();
		ks_dealloc(p);
		};

	ks_job_dispatch(js, job_fn, .data = payload, .size = 0, .owns_data = true, .free_fn = free_fn);

	return handle;
}

Ks_Handle AssetManager_Impl::load_from_data(const std::string& type_name, const std::string& asset_name, const Ks_UserData data) {
	std::lock_guard<std::mutex> lock(assets_mutex);

	auto found_name = assets_name_to_handle.find(asset_name);
	if (found_name != assets_name_to_handle.end()) {
		Ks_Handle h = found_name->second;
		assets_entries[h].ref_count++;
		return h;
	}

	auto it_iface = assets_interfaces.find(type_name);
	if (it_iface == assets_interfaces.end() || !it_iface->second.load_from_data_fn) {
		KS_LOG_ERROR("[Assets] Interface for type '%s' missing load_from_data_fn", type_name.c_str());
		return KS_INVALID_HANDLE;
	}

	Ks_AssetData asset_data = it_iface->second.load_from_data_fn(data);
	if (asset_data == KS_INVALID_ASSET_DATA) {
		KS_LOG_ERROR("[Assets] Failed to load asset '%s' from data", asset_name.c_str());
		return KS_INVALID_HANDLE;
	}

	Ks_Handle handle = generate_handle();
	Ks_AssetEntry entry;
	entry.data = asset_data;
	entry.asset_name = asset_name;
	entry.type_name = type_name;
	entry.source_path = "";
	entry.ref_count = 1;
	entry.state = KS_ASSET_STATE_READY;
	assets_entries.emplace(handle, entry);
	assets_name_to_handle.emplace(asset_name, handle);

	return handle;
}

void AssetManager_Impl::complete_async_load(Ks_Handle handle, Ks_AssetData data, bool success, Ks_IAsset original_iface) {
	std::lock_guard<std::mutex> lock(assets_mutex);

	if (handle == KS_INVALID_HANDLE) {
		KS_LOG_ERROR("[Assets] Complete async load called with invalid handle");
		if (success && data && original_iface.destroy_fn) {
			original_iface.destroy_fn(data);
		}
		return;
	}


	auto it = assets_entries.find(handle);
	if (it == assets_entries.end()) {
		if (success && data) {
			KS_LOG_WARN("[Assets] Async load finished for released asset. Destroying data immediately.");
			if (original_iface.destroy_fn) {
				original_iface.destroy_fn(data);
			}
		}
		return;
	}

	Ks_AssetEntry& entry = it->second;
	if (success) {
		entry.data = data;
		entry.state = KS_ASSET_STATE_READY;
		ks_file_watcher_watch_file(file_watcher, entry.source_path.c_str(), on_asset_file_changed, this);
		KS_LOG_INFO("[Assets] Async Load Ready: %s", entry.asset_name.c_str());
	}
	else {
		entry.state = KS_ASSET_STATE_FAILED;
		KS_LOG_ERROR("[Assets] Async Load Failed: %s", entry.asset_name.c_str());
	}
}

void AssetManager_Impl::update() {
	ks_file_watcher_poll(file_watcher);
}

bool AssetManager_Impl::reload_asset(Ks_Handle handle) {
	std::string type_name;
	std::string source_path;
	Ks_AssetData old_data = nullptr;

	// 1. Leggiamo i dati necessari SOTTO LOCK
	{
		std::lock_guard<std::mutex> lock(assets_mutex);
		auto found = assets_entries.find(handle);
		if (found == assets_entries.end()) return false;

		Ks_AssetEntry& entry = found->second;
		if (entry.source_path.empty()) return false;

		type_name = entry.type_name;
		source_path = entry.source_path;
		old_data = entry.data;
	}

	// 2. Carichiamo la nuova risorsa SENZA LOCK (perché può essere lento e può richiamare lock)
	// FIX DEADLOCK: Usiamo get_asset_interface che è safe o accediamo direttamente se avessimo accesso.
	// Ma qui dobbiamo trovare l'interfaccia. Facciamo lock per trovare interfaccia, poi unlock.

	Ks_IAsset iface;
	{
		std::lock_guard<std::mutex> lock(assets_mutex);
		iface = get_asset_interface_nolock(type_name);
	}

	if (!iface.load_from_file_fn) return false;

	Ks_AssetData new_data = iface.load_from_file_fn(source_path.c_str());
	if (new_data == KS_INVALID_ASSET_DATA) {
		return false;
	}

	// 3. Scambiamo i dati SOTTO LOCK
	{
		std::lock_guard<std::mutex> lock(assets_mutex);
		auto found = assets_entries.find(handle);
		if (found == assets_entries.end()) {
			// L'asset è stato rilasciato mentre caricavamo?
			if (iface.destroy_fn) iface.destroy_fn(new_data);
			return false;
		}

		found->second.data = new_data;
	}

	// 4. Distruggiamo i vecchi dati SENZA LOCK
	if (old_data && iface.destroy_fn) {
		iface.destroy_fn(old_data);
	}

	return true;
}

bool AssetManager_Impl::reload_asset(const std::string& source_path) {
	Ks_Handle handle = KS_INVALID_HANDLE;

	// 1. Troviamo l'handle SOTTO LOCK e poi deleghiamo
	{
		std::lock_guard<std::mutex> lock(assets_mutex);
		auto found = path_to_handle.find(source_path);
		if (found != path_to_handle.end()) {
			handle = found->second;
		}
	}

	if (handle != KS_INVALID_HANDLE) {
		return reload_asset(handle);
	}

	return false;
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
	am = new(
		ks_alloc(
			sizeof(AssetManager_Impl),
			KS_LT_USER_MANAGED,
			KS_TAG_INTERNAL_DATA
		)
		) AssetManager_Impl();

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
	if (!am) return KS_INVALID_HANDLE;
	return static_cast<AssetManager_Impl*>(am)->load_sync(type_name, asset_name, file_path);
}

KS_API Ks_Handle ks_assets_manager_load_asset_from_data(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, const Ks_UserData data)
{
	if (!am) return KS_INVALID_HANDLE;
	return static_cast<AssetManager_Impl*>(am)->load_from_data(type_name, asset_name, data);
}

KS_API Ks_Handle ks_assets_manager_load_async(Ks_AssetsManager am, ks_str type_name, ks_str asset_name, ks_str file_path, Ks_JobManager js) {
	if (!am) return KS_INVALID_HANDLE;
	return static_cast<AssetManager_Impl*>(am)->load_async(type_name, asset_name, file_path, js);
}

KS_API ks_no_ret ks_assets_manager_update(Ks_AssetsManager am) {
	if (!am) return;
	static_cast<AssetManager_Impl*>(am)->update();
}

KS_API ks_bool ks_assets_manager_reload_asset(Ks_AssetsManager am, Ks_Handle handle) {
	if (!am) return false;
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

Ks_AssetState ks_assets_get_state(Ks_AssetsManager am, Ks_Handle handle)
{
	if (handle == KS_INVALID_HANDLE) return KS_ASSET_STATE_NONE;

	AssetManager_Impl* iam = static_cast<AssetManager_Impl*>(am);

	return iam->get_asset_state_from_handle(handle);
}