#pragma once

#include "../core/defines.h"
#include "../core/types.h"
#include "../core/handle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_TimeManager;

KS_API Ks_TimeManager ks_time_manager_create();
KS_API ks_no_ret ks_time_manager_destroy(Ks_TimeManager tm);

KS_API ks_no_ret ks_time_manager_update(Ks_TimeManager tm);
KS_API ks_no_ret ks_time_manager_process_timers(Ks_TimeManager tm);

KS_API ks_uint64 ks_time_get_total_ns(Ks_TimeManager tm);
KS_API ks_float ks_time_get_delta_sec(Ks_TimeManager tm);

KS_API ks_no_ret ks_time_set_scale(Ks_TimeManager tm, ks_float scale);
KS_API ks_float ks_time_get_scale(Ks_TimeManager tm);

KS_API Ks_Handle ks_timer_create(Ks_TimeManager tm, ks_uint64 duration_ns, ks_bool loop);

KS_API ks_no_ret ks_timer_destroy(Ks_TimeManager tm, Ks_Handle handle);

#ifdef __cplusplus
}
#endif