#pragma once

#include "event.h"
#include "events_manager.h"
#include "../script/script_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

KS_API Ks_Script_Table ks_event_get_script_tbl(Ks_Event_Payload data, ks_size index);
KS_API Ks_Script_Ctx ks_event_get_script_ctx(Ks_Event_Payload data);

KS_API ks_no_ret ks_event_manager_lua_bind(Ks_EventManager em, Ks_Script_Ctx ctx);

#ifdef __cplusplus
}
#endif