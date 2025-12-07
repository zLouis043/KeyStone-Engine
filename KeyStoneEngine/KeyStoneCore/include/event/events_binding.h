#pragma once

#include "event.h"
#include "events_manager.h"
#include "../script/script_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

KS_API ks_no_ret ks_event_manager_lua_bind(Ks_EventManager em, Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_event_manager_lua_shutdown(Ks_EventManager em);

#ifdef __cplusplus
}
#endif