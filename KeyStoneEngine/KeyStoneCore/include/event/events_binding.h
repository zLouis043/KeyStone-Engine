#pragma once

#include "event.h"
#include "events_manager.h"
#include "../script/script_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ScriptEvent {
    Ks_Script_Object layout;
    Ks_Script_Object payload;
    const char* event_name;
} ScriptEvent;

KS_API ks_no_ret ks_event_manager_lua_bind(Ks_EventManager em, Ks_Script_Ctx ctx);

#ifdef __cplusplus
}
#endif