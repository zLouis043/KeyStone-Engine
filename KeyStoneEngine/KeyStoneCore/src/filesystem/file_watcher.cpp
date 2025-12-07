#include "../../include/filesystem/file_watcher.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"

#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

struct WatchedFile {
    std::string path;
    fs::file_time_type last_write_time;
    ks_file_change_callback callback;
    ks_ptr user_data;
};

class FileWatcher_Impl {
public:
    void watch(const char* path, ks_file_change_callback cb, ks_ptr user_data) {
        fs::path p(path);
        if (!fs::exists(p)) {
            return;
        }

        for (const auto& item : watched_files) {
            if (item.path == path) return;
        }

        WatchedFile wf;
        wf.path = path;
        wf.callback = cb;
        wf.user_data = user_data;

        std::error_code ec;
        wf.last_write_time = fs::last_write_time(p, ec);

        watched_files.push_back(wf);
    }

    void unwatch(const char* path) {
        std::string s_path = path;
        auto it = std::remove_if(watched_files.begin(), watched_files.end(),
            [&](const WatchedFile& wf) { return wf.path == s_path; });

        if (it != watched_files.end()) {
            watched_files.erase(it, watched_files.end());
        }
    }

    void poll() {
        for (auto& wf : watched_files) {
            fs::path p(wf.path);
            std::error_code ec;

            if (!fs::exists(p)) continue;

            auto current_time = fs::last_write_time(p, ec);
            if (ec) continue;

            if (current_time > wf.last_write_time) {
                wf.last_write_time = current_time;
                if (wf.callback) {
                    wf.callback(wf.path.c_str(), wf.user_data);
                }
            }
        }
    }

private:
    std::vector<WatchedFile> watched_files;
};

KS_API Ks_FileWatcher ks_file_watcher_create() {
    return new (ks_alloc(sizeof(FileWatcher_Impl), KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA)) FileWatcher_Impl();
}

KS_API ks_no_ret ks_file_watcher_destroy(Ks_FileWatcher watcher) {
    if (watcher) {
        static_cast<FileWatcher_Impl*>(watcher)->~FileWatcher_Impl();
        ks_dealloc(watcher);
    }
}

KS_API ks_no_ret ks_file_watcher_watch_file(Ks_FileWatcher watcher, ks_str file_path, ks_file_change_callback callback, ks_ptr user_data) {
    static_cast<FileWatcher_Impl*>(watcher)->watch(file_path, callback, user_data);
}

KS_API ks_no_ret ks_file_watcher_unwatch_file(Ks_FileWatcher watcher, ks_str file_path) {
    static_cast<FileWatcher_Impl*>(watcher)->unwatch(file_path);
}

KS_API ks_no_ret ks_file_watcher_poll(Ks_FileWatcher watcher) {
    static_cast<FileWatcher_Impl*>(watcher)->poll();
}