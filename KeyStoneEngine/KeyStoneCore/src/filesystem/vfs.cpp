#include "../../include/filesystem/vfs.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"
#include "../../include/core/error.h"
#include "../../include/profiler/profiler.h"

#include <string>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>
#include <mutex>
#include <shared_mutex>

namespace fs = std::filesystem;

struct PathCache {
    std::unordered_map<std::string, std::string> cache;
    std::mutex mutex;
    size_t max_entries = 1024;

    std::optional<std::string> get(const std::string& vfs_path) {
        std::lock_guard lock(mutex);
        auto it = cache.find(vfs_path);
        return it != cache.end() ? std::optional(it->second) : std::nullopt;
    }

    void put(const std::string& vfs_path, const std::string& resolved) {
        std::lock_guard lock(mutex);
        if (cache.size() >= max_entries) {
            cache.clear();
        }
        cache[vfs_path] = resolved;
    }

    void invalidate(const std::string& prefix = "") {
        std::lock_guard lock(mutex);
        if (prefix.empty()) {
            cache.clear();
        }
        else {
            for (auto it = cache.begin(); it != cache.end();) {
                if (it->first.starts_with(prefix)) {
                    it = cache.erase(it);
                }
                else {
                    ++it;
                }
            }
        }
    }
};

struct VFS_Impl {
    std::unordered_map<std::string, std::string> mount_points;
    mutable std::shared_mutex mount_mutex;
    PathCache path_cache;

    bool parse_uri(const std::string& uri, std::string& out_alias, std::string& out_path) {
        size_t pos = uri.find("://");
        if (pos == std::string::npos) return false;

        out_alias = uri.substr(0, pos);
        out_path = uri.substr(pos + 3);
        return true;
    }

    std::string resolve_internal(const std::string& virtual_path) {

        if (auto cached = path_cache.get(virtual_path)) {
            KS_PROFILE_SCOPE("VFS_Cache_Hit");
            return *cached;
        }

        KS_PROFILE_SCOPE("VFS_Cache_Miss");
        std::string alias, relative_path;
        if (!parse_uri(virtual_path, alias, relative_path)) {
            return "";
        }

        std::string base_path;
        {
            std::shared_lock lock(mount_mutex);
            auto it = mount_points.find(alias);
            if (it == mount_points.end()) return "";
            base_path = it->second;
        }


        try {
            fs::path p = fs::path(base_path) / relative_path;

            path_cache.put(virtual_path, p.string());

            return p.string();
        }
        catch (...) {
            return "";
        }
    }

    void invalidate_cache(const std::string& mount_point = "") {
        path_cache.invalidate(mount_point);
    }
};

static VFS_Impl* g_vfs = nullptr;

KS_API ks_bool ks_vfs_init() {
    if (g_vfs) return ks_false;
    g_vfs = new(ks_alloc(sizeof(VFS_Impl), KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA)) VFS_Impl();
    return ks_true;
}

KS_API ks_no_ret ks_vfs_shutdown() {
    if (g_vfs) {
        g_vfs->~VFS_Impl();
        ks_dealloc(g_vfs);
        g_vfs = nullptr;
    }
}

KS_API ks_bool ks_vfs_mount(ks_str alias, ks_str physical_path, ks_bool overwrite) {
    if (!alias || !physical_path) return false;

    std::unique_lock lock(g_vfs->mount_mutex);

    if (!overwrite && g_vfs->mount_points.count(alias)) {
        ks_epush_s_fmt(KS_ERROR_LEVEL_WARNING, "VFS", KS_VFS_ERROR_PATH_ALREADY_MOUNTED, "[VFS] Alias '%s' already mounted", alias);
        return false;
    }

    try {
        fs::path abs_path = fs::absolute(physical_path);
        if (!fs::exists(abs_path)) {
            ks_epush_s_fmt(KS_ERROR_LEVEL_WARNING, "VFS", KS_VFS_ERROR_PATH_DOES_NOT_EXIST, "[VFS] Mounting non-existent path: %s", physical_path);
        }
        g_vfs->mount_points[alias] = abs_path.string();
        KS_LOG_INFO("[VFS] Mounted '%s' -> '%s'", alias, abs_path.string().c_str());

        lock.unlock();

        std::string prefix = std::string(alias) + "://";
        g_vfs->invalidate_cache(prefix);

        return true;
    }
    catch (...) {
        return false;
    }
}

KS_API ks_bool ks_vfs_unmount(ks_str alias) {
    if (!alias) return false;

    std::unique_lock lock(g_vfs->mount_mutex);

    bool erased = g_vfs->mount_points.erase(alias) > 0;

    if (erased) {
        std::string prefix = std::string(alias) + "://";
        g_vfs->invalidate_cache(prefix);
    }

    lock.unlock();

    return erased;
}

KS_API ks_bool ks_vfs_resolve(ks_str virtual_path, char* out_path, ks_size max_len) {
    if (!virtual_path || !out_path) return false;

    std::string res = g_vfs->resolve_internal(virtual_path);
    if (res.empty()) return false;

    if (res.length() >= max_len) return false;

    memcpy(out_path, res.c_str(), res.length() + 1);
    return true;
}

KS_API ks_bool ks_vfs_exists(ks_str virtual_path) {
    if (!virtual_path) return false;
    std::string path = g_vfs->resolve_internal(virtual_path);
    if (path.empty()) return false;
    return fs::exists(path) && fs::is_regular_file(path);
}

KS_API ks_str ks_vfs_read_file(ks_str virtual_path, ks_size* out_size) {
    KS_PROFILE_FUNCTION();
    if (out_size) *out_size = 0;
    if (!virtual_path) return nullptr;

    std::string path = g_vfs->resolve_internal(virtual_path);
    if (path.empty()) return nullptr;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        ks_epush_s_fmt(KS_ERROR_LEVEL_BASE, "VFS", KS_VFS_ERROR_FAILED_TO_OPEN_FILE, "[VFS] Failed to open file: %s", path.c_str());
        return nullptr;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size <= 0) return nullptr;

    ks_str data = (ks_str)ks_alloc((size_t)size + 1, KS_LT_USER_MANAGED, KS_TAG_RESOURCE);

    if (file.read((char*)data, size)) {
        ((char*)data)[size] = '\0';
        if (out_size) *out_size = (ks_size)size;
        return data;
    }
    else {
        ks_dealloc((ks_ptr)data);
        return nullptr;
    }
}

KS_API ks_bool ks_vfs_write_file(ks_str virtual_path, const void* data, ks_size size) {
    if (!virtual_path || !data) return false;

    std::string path = g_vfs->resolve_internal(virtual_path);
    if (path.empty()) return false;

    fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.write((const char*)data, size);
    return true;
}