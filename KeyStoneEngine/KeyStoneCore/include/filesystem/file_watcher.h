#pragma once

#include "../core/defines.h"
#include "../core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_FileWatcher;

typedef void(*ks_file_change_callback)(ks_str path, ks_ptr user_data);

KS_API Ks_FileWatcher ks_file_watcher_create();
KS_API ks_no_ret ks_file_watcher_destroy(Ks_FileWatcher watcher);

KS_API ks_no_ret ks_file_watcher_watch_file(Ks_FileWatcher watcher, ks_str file_path, ks_file_change_callback callback, ks_ptr user_data);
KS_API ks_no_ret ks_file_watcher_unwatch_file(Ks_FileWatcher watcher, ks_str file_path);
KS_API ks_no_ret ks_file_watcher_poll(Ks_FileWatcher watcher);

#ifdef __cplusplus
}
#endif