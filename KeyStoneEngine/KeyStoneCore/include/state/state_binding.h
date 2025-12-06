#pragma once

#include "state_manager.h"
#include "../script/script_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

KS_API ks_no_ret ks_state_manager_lua_bind(Ks_StateManager sm, Ks_Script_Ctx ctx);

#ifdef __cplusplus
}
#endif