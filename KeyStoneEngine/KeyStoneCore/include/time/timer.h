#pragma once 

#include "time_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*ks_timer_callback)(ks_ptr user_data);

KS_API ks_no_ret ks_timer_start(Ks_TimeManager tm, Ks_Handle handle);
KS_API ks_no_ret ks_timer_stop(Ks_TimeManager tm, Ks_Handle handle);
KS_API ks_no_ret ks_timer_reset(Ks_TimeManager tm, Ks_Handle handle);
KS_API ks_bool ks_timer_is_running(Ks_TimeManager tm, Ks_Handle handle);
KS_API ks_bool ks_timer_is_looping(Ks_TimeManager tm, Ks_Handle handle);

KS_API ks_no_ret ks_timer_set_duration(Ks_TimeManager tm, Ks_Handle handle, ks_uint64 duration_ns);
KS_API ks_no_ret ks_timer_set_loop(Ks_TimeManager tm, Ks_Handle handle, ks_bool loop);
KS_API ks_no_ret ks_timer_set_callback(Ks_TimeManager tm, Ks_Handle handle, ks_timer_callback callback, ks_ptr user_data);

#ifdef __cplusplus
}
#endif