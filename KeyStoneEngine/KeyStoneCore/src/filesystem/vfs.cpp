#include "../../include/filesystem/vfs.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"

#include <string>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

struct VFS_Impl {
    std::unordered_map<std::string, std::string> mount_points;

    bool parse_uri(const std::string& uri, std::string& out_alias, std::string& out_path) {
        size_t pos = uri.find("://");
        if (pos == std::string::npos) return false;

        out_alias = uri.substr(0, pos);
        out_path = uri.substr(pos + 3);
        return true;
    }

    std::string resolve_internal(const std::string& virtual_path) {
        std::string alias, relative_path;
        if (!parse_uri(virtual_path, alias, relative_path)) {
            return "";
        }

        auto it = mount_points.find(alias);
        if (it == mount_points.end()) return "";

        try {
            fs::path p = fs::path(it->second) / relative_path;
            return p.string();
        }
        catch (...) {
            return "";
        }
    }
};

static VFS_Impl* impl(Ks_VFS vfs) { return (VFS_Impl*)vfs; }

KS_API Ks_VFS ks_vfs_create() {
    return new(ks_alloc(sizeof(VFS_Impl), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA)) VFS_Impl();
}

KS_API ks_no_ret ks_vfs_destroy(Ks_VFS vfs) {
    if (vfs) {
        impl(vfs)->~VFS_Impl();
        ks_dealloc(vfs);
    }
}

KS_API ks_bool ks_vfs_mount(Ks_VFS vfs, ks_str alias, ks_str physical_path, ks_bool overwrite) {
    if (!vfs || !alias || !physical_path) return false;
    VFS_Impl* i = impl(vfs);

    if (!overwrite && i->mount_points.count(alias)) {
        KS_LOG_WARN("[VFS] Alias '%s' already mounted", alias);
        return false;
    }

    try {
        fs::path abs_path = fs::absolute(physical_path);
        if (!fs::exists(abs_path)) {
            KS_LOG_WARN("[VFS] Mounting non-existent path: %s", physical_path);
        }
        i->mount_points[alias] = abs_path.string();
        KS_LOG_INFO("[VFS] Mounted '%s' -> '%s'", alias, abs_path.string().c_str());
        return true;
    }
    catch (...) {
        return false;
    }
}

KS_API ks_bool ks_vfs_unmount(Ks_VFS vfs, ks_str alias) {
    if (!vfs || !alias) return false;
    return impl(vfs)->mount_points.erase(alias) > 0;
}

KS_API ks_bool ks_vfs_resolve(Ks_VFS vfs, ks_str virtual_path, char* out_path, ks_size max_len) {
    if (!vfs || !virtual_path || !out_path) return false;

    std::string res = impl(vfs)->resolve_internal(virtual_path);
    if (res.empty()) return false;

    if (res.length() >= max_len) return false;

    strcpy(out_path, res.c_str());
    return true;
}

KS_API ks_bool ks_vfs_exists(Ks_VFS vfs, ks_str virtual_path) {
    if (!vfs || !virtual_path) return false;
    std::string path = impl(vfs)->resolve_internal(virtual_path);
    if (path.empty()) return false;
    return fs::exists(path) && fs::is_regular_file(path);
}

KS_API ks_str ks_vfs_read_file(Ks_VFS vfs, ks_str virtual_path, ks_size* out_size) {
    if (out_size) *out_size = 0;
    if (!vfs || !virtual_path) return nullptr;

    std::string path = impl(vfs)->resolve_internal(virtual_path);
    if (path.empty()) return nullptr;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        KS_LOG_ERROR("[VFS] Failed to open file: %s", path.c_str());
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

KS_API ks_bool ks_vfs_write_file(Ks_VFS vfs, ks_str virtual_path, const void* data, ks_size size) {
    if (!vfs || !virtual_path || !data) return false;

    std::string path = impl(vfs)->resolve_internal(virtual_path);
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