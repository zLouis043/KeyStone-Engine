#include "../../include/time/time_binding.h"
#include "../../include/time/timer.h"
#include "../../include/script/script_engine.h"
#include "../../include/memory/memory.h"

#include <unordered_map>
#include <mutex>

#define NS_PER_SEC 1000000000ULL
#define NS_PER_MS  1000000ULL
#define NS_PER_US  1000ULL
#define NS_PER_MIN (60ULL * NS_PER_SEC)
#define NS_PER_HOUR (60ULL * NS_PER_MIN)

static Ks_TimeManager get_tm(Ks_Script_Ctx ctx) {
    Ks_Script_Object up = ks_script_func_get_upvalue(ctx, 1);
    return (Ks_TimeManager)ks_script_lightuserdata_get_ptr(ctx, up);
}

struct LuaTimerData {
    Ks_Script_Ctx ctx;
    Ks_Script_Object func_ref;
    Ks_TimeManager tm;
    Ks_Handle timer_handle;
};

static std::mutex s_timers_mutex;
static std::unordered_map<ks_uint32, LuaTimerData*> s_lua_timers;

static void cleanup_lua_timer_data(ks_uint32 handle) {
    std::lock_guard<std::mutex> lock(s_timers_mutex);
    auto it = s_lua_timers.find(handle);
    if (it != s_lua_timers.end()) {
        LuaTimerData* data = it->second;
        ks_script_free_obj(data->ctx, data->func_ref);
        ks_dealloc(data);
        s_lua_timers.erase(it);
    }
}

static void lua_timer_thunk(void* user_data) {
    LuaTimerData* data = (LuaTimerData*)user_data;
    if (!data) return;

    if (ks_script_obj_is_valid(data->ctx, data->func_ref)) {
        ks_script_func_call(data->ctx, data->func_ref, 0, 0);
    }

    bool is_loop = ks_timer_is_looping(data->tm, data->timer_handle);
    if (!is_loop) {
        cleanup_lua_timer_data(data->timer_handle);
    }
}

void ks_time_manager_binding_shutdown(Ks_TimeManager target_tm) {
    std::lock_guard<std::mutex> lock(s_timers_mutex);

    auto it = s_lua_timers.begin();
    while (it != s_lua_timers.end()) {
        LuaTimerData* data = it->second;

        if (data->tm == target_tm) {
            ks_script_free_obj(data->ctx, data->func_ref);
            ks_dealloc(data);
            it = s_lua_timers.erase(it);
        }
        else {
            ++it;
        }
    }
}

ks_returns_count l_time_elapsed_ns(Ks_Script_Ctx ctx) { ks_script_stack_push_integer(ctx, (ks_int64)ks_time_get_total_ns(get_tm(ctx))); return 1; }
ks_returns_count l_time_elapsed_s(Ks_Script_Ctx ctx) { ks_script_stack_push_number(ctx, (double)ks_time_get_total_ns(get_tm(ctx)) / (double)NS_PER_SEC); return 1; }
ks_returns_count l_time_dt(Ks_Script_Ctx ctx) { ks_script_stack_push_number(ctx, ks_time_get_delta_sec(get_tm(ctx))); return 1; }

ks_returns_count l_conv_seconds(Ks_Script_Ctx ctx) {
    double v = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    ks_script_stack_push_integer(ctx, (ks_int64)(v * NS_PER_SEC));
    return 1;
}
ks_returns_count l_conv_milliseconds(Ks_Script_Ctx ctx) {
    double v = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    ks_script_stack_push_integer(ctx, (ks_int64)(v * NS_PER_MS));
    return 1;
}

ks_returns_count l_conv_minutes(Ks_Script_Ctx ctx) {
    double v = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    ks_script_stack_push_integer(ctx, (ks_int64)(v * NS_PER_MIN));
    return 1;
}

ks_returns_count l_conv_hours(Ks_Script_Ctx ctx) {
    double v = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    ks_script_stack_push_integer(ctx, (ks_int64)(v * NS_PER_HOUR));
    return 1;
}

struct TimerUserData {
    Ks_Handle handle;
    Ks_TimeManager tm;
};

ks_returns_count l_timer_start(Ks_Script_Ctx ctx) {
    auto* ud = (TimerUserData*)ks_script_get_self(ctx);
    if (ud) ks_timer_start(ud->tm, ud->handle);
    return 0;
}

ks_returns_count l_timer_stop(Ks_Script_Ctx ctx) {
    auto* ud = (TimerUserData*)ks_script_get_self(ctx);
    if (ud) ks_timer_stop(ud->tm, ud->handle);
    return 0;
}

ks_returns_count l_timer_set_duration(Ks_Script_Ctx ctx) {
    auto* ud = (TimerUserData*)ks_script_get_self(ctx);
    ks_int64 dur = ks_script_obj_as_integer(ctx, ks_script_get_arg(ctx, 1));
    if (ud) ks_timer_set_duration(ud->tm, ud->handle, (ks_uint64)dur);
    return 0;
}

ks_returns_count l_timer_set_loop(Ks_Script_Ctx ctx) {
    auto* ud = (TimerUserData*)ks_script_get_self(ctx);
    bool loop = ks_script_obj_as_boolean(ctx, ks_script_get_arg(ctx, 1));
    if (ud) ks_timer_set_loop(ud->tm, ud->handle, loop);
    return 0;
}

ks_returns_count l_timer_set_function(Ks_Script_Ctx ctx) {
    auto* ud = (TimerUserData*)ks_script_get_self(ctx);
    Ks_Script_Object func = ks_script_get_arg(ctx, 1);

    if (ud && func.type == KS_TYPE_SCRIPT_FUNCTION) {
        cleanup_lua_timer_data(ud->handle);

        LuaTimerData* data = (LuaTimerData*)ks_alloc(sizeof(LuaTimerData), KS_LT_USER_MANAGED, KS_TAG_SCRIPT);
        data->ctx = ctx;
        data->tm = ud->tm;
        data->timer_handle = ud->handle;

        data->func_ref = ks_script_ref_obj(ctx, func);

        {
            std::lock_guard<std::mutex> lock(s_timers_mutex);
            s_lua_timers[ud->handle] = data;
        }

        ks_timer_set_callback(ud->tm, ud->handle, lua_timer_thunk, data);
    }
    return 0;
}

ks_returns_count l_create_timer(Ks_Script_Ctx ctx) {
    Ks_TimeManager tm = get_tm(ctx);
    ks_int64 duration = ks_script_obj_as_integer_or(ctx, ks_script_get_arg(ctx, 1), 0);
    bool loop = ks_script_obj_as_boolean_or(ctx, ks_script_get_arg(ctx, 2), false);
    Ks_Script_Object func = ks_script_get_arg(ctx, 3);

    Ks_Handle h = ks_timer_create(tm, (ks_uint64)duration, loop);

    if (func.type == KS_TYPE_SCRIPT_FUNCTION) {
        LuaTimerData* data = (LuaTimerData*)ks_alloc(sizeof(LuaTimerData), KS_LT_USER_MANAGED, KS_TAG_SCRIPT);
        data->ctx = ctx;
        data->tm = tm;
        data->timer_handle = h;

        ks_script_promote(ctx, func);
        data->func_ref = func;

        {
            std::lock_guard<std::mutex> lock(s_timers_mutex);
            s_lua_timers[h] = data;
        }

        ks_timer_set_callback(tm, h, lua_timer_thunk, data);
    }

    Ks_Script_Userdata ud_obj = ks_script_create_usertype_instance(ctx, "Timer");
    TimerUserData* ud = (TimerUserData*)ks_script_usertype_get_ptr(ctx, ud_obj);
    ud->handle = h;
    ud->tm = tm;

    ks_script_stack_push_obj(ctx, ud_obj);
    return 1;
}

ks_returns_count l_destroy_timer(Ks_Script_Ctx ctx) {
    Ks_Script_Object arg = ks_script_get_arg(ctx, 1);
    if (arg.type == KS_TYPE_USERDATA) {
        TimerUserData* ud = (TimerUserData*)ks_script_usertype_get_ptr(ctx, arg);
        cleanup_lua_timer_data(ud->handle);
        ks_timer_destroy(ud->tm, ud->handle);
    }
    return 0;
}

KS_API void ks_time_manager_lua_bind(Ks_Script_Ctx ctx, Ks_TimeManager tm) {
    auto b = ks_script_usertype_begin(ctx, "Timer", sizeof(TimerUserData));
    ks_script_usertype_add_method(b, "set_duration", KS_SCRIPT_FUNC(l_timer_set_duration, KS_TYPE_INT));
    ks_script_usertype_add_method(b, "set_loop", KS_SCRIPT_FUNC(l_timer_set_loop, KS_TYPE_BOOL));
    ks_script_usertype_add_method(b, "set_function", KS_SCRIPT_FUNC(l_timer_set_function, KS_TYPE_SCRIPT_FUNCTION));
    ks_script_usertype_add_method(b, "start", KS_SCRIPT_FUNC_VOID(l_timer_start));
    ks_script_usertype_add_method(b, "stop", KS_SCRIPT_FUNC_VOID(l_timer_stop));
    ks_script_usertype_end(b);

    Ks_Script_Object tm_ptr = ks_script_create_lightuserdata(ctx, tm);
    Ks_Script_Table tbl = ks_script_create_named_table(ctx, "time");

    auto reg = [&](const char* n, ks_script_cfunc f) {
        ks_script_stack_push_obj(ctx, tm_ptr);
        Ks_Script_Function fn = ks_script_create_cfunc_with_upvalues(ctx, KS_SCRIPT_FUNC_VOID(f), 1);
        ks_script_table_set(ctx, tbl, ks_script_create_cstring(ctx, n), fn);
        };

    reg("elapsed_time_in_nanoseconds", l_time_elapsed_ns);
    reg("elapsed_time_in_seconds", l_time_elapsed_s);
    reg("delta_time", l_time_dt);
    reg("seconds", l_conv_seconds);
    reg("milliseconds", l_conv_milliseconds);

    reg("create_timer", l_create_timer);
    reg("destroy_timer", l_destroy_timer);
}