#pragma once

#include "../core/defines.h"
#include "../core/types.h"

#include "assets_manager.h"
#include "../script/script_engine.h"
#include "../job/job.h"

#ifdef __cplusplus
extern "C" {
#endif


KS_API ks_no_ret ks_assets_manager_lua_bind(Ks_Script_Ctx ctx, Ks_AssetsManager am, Ks_JobManager js);

#ifdef __cplusplus
}
#endif
