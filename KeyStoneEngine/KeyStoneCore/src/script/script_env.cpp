#include "../../include/script/script_env.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"
#include "../../include/core/types_binding.h"
#include "../../include/event/events_binding.h"
#include "../../include/asset/assets_binding.h"
#include "../../include/state/state_binding.h"
#include "../../include/time/time_binding.h"
#include "../../include/filesystem/file_watcher.h"

#include <string>
#include <new>
#include <unordered_map>
#include <vector>

struct ScriptEnv_Impl {
    Ks_EventManager event_mgr;
    Ks_StateManager state_mgr;
    Ks_AssetsManager assets_mgr;
    Ks_JobManager job_mgr;
    Ks_TimeManager time_mgr;

    Ks_Script_Ctx script_ctx = nullptr;
    Ks_FileWatcher file_watcher = nullptr;

    std::string entry_script_path;

    std::unordered_map<std::string, std::string> path_to_module;
    std::vector<std::string> pending_reloads;

    ScriptEnv_Impl(Ks_EventManager em, Ks_StateManager sm, Ks_AssetsManager am, Ks_JobManager jm, Ks_TimeManager tm)
        : event_mgr(em), state_mgr(sm), assets_mgr(am), job_mgr(jm), time_mgr(tm)
    {
        file_watcher = ks_file_watcher_create();
    }

    ~ScriptEnv_Impl() {
        if (script_ctx) {
            ks_job_manager_destroy(job_mgr);
            ks_event_manager_lua_shutdown(event_mgr);
            ks_time_manager_binding_shutdown(time_mgr);
            ks_script_destroy_ctx(script_ctx);
        }
        ks_file_watcher_destroy(file_watcher);
    }

    static void on_file_changed(ks_str path, ks_ptr user_data) {
        auto* self = (ScriptEnv_Impl*)user_data;
        self->pending_reloads.push_back(path);
    }

    static ks_returns_count development_searcher(Ks_Script_Ctx ctx) {
        Ks_Script_Object self_obj = ks_script_func_get_upvalue(ctx, 1);
        auto* self = (ScriptEnv_Impl*)ks_script_lightuserdata_get_ptr(ctx, self_obj);

        const char* mod_name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
        if (!mod_name || !self) return 0;

        const char* filename = ks_script_resolve_module_path(ctx, mod_name);

        if (!filename) {
            ks_script_stack_push_string(ctx, "\n\t[ScriptEnv]: Module path not found via package.path");
            return 1;
        }

        ks_file_watcher_watch_file(self->file_watcher, filename, on_file_changed, self);

        self->path_to_module[filename] = mod_name;
        Ks_Script_Function chunk = ks_script_load_file(ctx, filename);

        ks_dealloc((void*)filename);

        if (!ks_script_obj_is_valid(ctx, chunk)) {
            ks_str err = ks_script_get_last_error_str(ctx);
            ks_script_stack_push_string(ctx, err ? err : "Unknown load error");
            return 1;
        }

        ks_script_stack_push_obj(ctx, chunk);
        return 1;
    }

    void install_dev_searcher() {
        Ks_Script_Object self_ptr = ks_script_create_lightuserdata(script_ctx, this);
        ks_script_stack_push_obj(script_ctx, self_ptr);

        Ks_Script_Function searcher_func = ks_script_create_cfunc_with_upvalues(script_ctx,
            KS_SCRIPT_FUNC(development_searcher, KS_TYPE_CSTRING), 1);

        ks_script_add_searcher(script_ctx, searcher_func, 2);
    }


    void reload_module(const std::string& path) {
        if (path == entry_script_path) {
            auto res = ks_script_do_file(script_ctx, path.c_str());
            if (!ks_script_call_succeded(script_ctx, res)) {
                KS_LOG_ERROR("ScriptEnv: Reload Error: %s", ks_script_get_last_error_str(script_ctx));
            }
            return;
        }

        auto it = path_to_module.find(path);
        if (it != path_to_module.end()) {
            std::string mod_name = it->second;
            ks_script_invalidate_module(script_ctx, mod_name.c_str());
            ks_script_require(script_ctx, mod_name.c_str());
        }
    }

    void setup_context() {
        script_ctx = ks_script_create_ctx();

        ks_types_lua_bind(script_ctx);
        ks_event_manager_lua_bind(event_mgr, script_ctx);
        ks_state_manager_lua_bind(state_mgr, script_ctx);
        ks_assets_manager_lua_bind(script_ctx, assets_mgr, job_mgr);
        ks_time_manager_lua_bind(script_ctx, time_mgr);

        install_dev_searcher();
    }

    void init(const char* path) {
        entry_script_path = path;

        setup_context();

        ks_file_watcher_watch_file(file_watcher, path, on_file_changed, this);

        auto res = ks_script_do_file(script_ctx, path);
        if (!ks_script_call_succeded(script_ctx, res)) {
            KS_LOG_ERROR("ScriptEnv: Entry script error: %s", ks_script_get_last_error_str(script_ctx));
        }
    }

    void update() {
        ks_file_watcher_poll(file_watcher);
        ks_assets_manager_update(assets_mgr);
        ks_time_manager_update(time_mgr);
        ks_time_manager_process_timers(time_mgr);

        if (!pending_reloads.empty()) {
            for (const auto& path : pending_reloads) {
                reload_module(path);
            }
            pending_reloads.clear();
        }
    }
};

KS_API Ks_ScriptEnv ks_script_env_create(Ks_EventManager em, Ks_StateManager sm, Ks_AssetsManager am, Ks_JobManager jm, Ks_TimeManager tm) {

    void* raw_data = ks_alloc_debug(sizeof(ScriptEnv_Impl), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA, "KsScriptEnvImpl");

    return (Ks_ScriptEnv) new(raw_data) ScriptEnv_Impl(em, sm, am, jm, tm);
}

KS_API ks_no_ret ks_script_env_destroy(Ks_ScriptEnv env) {
    if (env) {
        ScriptEnv_Impl* se = static_cast<ScriptEnv_Impl*>(env);
        se->~ScriptEnv_Impl();
        ks_dealloc(se);
    }
}

KS_API ks_no_ret ks_script_env_init(Ks_ScriptEnv env, ks_str entry_path) {
    static_cast<ScriptEnv_Impl*>(env)->init(entry_path);
}

KS_API ks_no_ret ks_script_env_update(Ks_ScriptEnv env) {
    static_cast<ScriptEnv_Impl*>(env)->update();
}

KS_API Ks_Script_Ctx ks_script_env_get_ctx(Ks_ScriptEnv env) {
    return static_cast<ScriptEnv_Impl*>(env)->script_ctx;
}