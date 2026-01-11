#pragma once
#pragma once

#include "ecs.h"
#include "../script/script_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

KS_API ks_no_ret ks_ecs_lua_bind(Ks_Ecs_World world, Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_ecs_lua_shutdown(Ks_Ecs_World world);

#ifdef __cplusplus
}
#endif