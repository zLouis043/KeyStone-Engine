#pragma once
#include "../script/script_engine.h"
#include "time_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

KS_API void ks_time_manager_lua_bind(Ks_Script_Ctx ctx, Ks_TimeManager tm);
KS_API void ks_time_manager_binding_shutdown(Ks_TimeManager tm);

#ifdef __cplusplus
}
#endif