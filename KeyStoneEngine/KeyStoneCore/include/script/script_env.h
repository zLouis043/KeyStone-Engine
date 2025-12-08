#pragma once

#include "../core/defines.h"
#include "../core/types.h"
#include "../script/script_engine.h"

#include "../asset/assets_manager.h"
#include "../state/state_manager.h"
#include "../event/events_manager.h"
#include "../time/time_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_ScriptEnv;

KS_API Ks_ScriptEnv ks_script_env_create(
    Ks_EventManager em,
    Ks_StateManager sm,
    Ks_AssetsManager am,
    Ks_TimeManager tm
);

KS_API ks_no_ret ks_script_env_destroy(Ks_ScriptEnv env);

KS_API ks_no_ret ks_script_env_init(Ks_ScriptEnv env, ks_str entry_path);

KS_API ks_no_ret ks_script_env_update(Ks_ScriptEnv env);

KS_API Ks_Script_Ctx ks_script_env_get_ctx(Ks_ScriptEnv env);

#ifdef __cplusplus
}
#endif