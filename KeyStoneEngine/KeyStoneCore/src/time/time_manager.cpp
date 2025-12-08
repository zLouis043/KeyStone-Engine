#include "../../include/time/time_manager.h"
#include "../../include/time/timer.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"
#include <chrono>
#include <vector>
#include <algorithm>

struct TimerEntry {
    Ks_Handle handle;
    ks_uint64 duration_ns;
    ks_uint64 elapsed_ns;
    bool loop;
    bool running;
    bool pending_delete;

    ks_timer_callback callback;
    ks_ptr user_data;
};

struct TimeManager_Impl {
    using Clock = std::chrono::high_resolution_clock;

    Clock::time_point start_tp;
    Clock::time_point last_tp;

    ks_uint64 total_elapsed_ns = 0;
    float delta_time_sec = 0.0f;
    float time_scale = 1.0f;

    std::vector<TimerEntry> timers;
    Ks_Handle_Id time_handle_id;

    TimeManager_Impl() {
        start_tp = Clock::now();
        last_tp = start_tp;
        time_handle_id = ks_handle_register("TimeManager");
    }

    void update() {
        auto now = Clock::now();
        auto frame_duration = now - last_tp;
        last_tp = now;

        long long frame_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(frame_duration).count();

        long long scaled_ns = (long long)(frame_ns * time_scale);

        total_elapsed_ns += scaled_ns;

        delta_time_sec = scaled_ns / 1'000'000'000.0f;

        if (delta_time_sec > 0.1f) delta_time_sec = 0.1f;
    }

    void process_timers() {
        ks_uint64 step_ns = (ks_uint64)(delta_time_sec * 1'000'000'000.0f);

        for (auto& t : timers) {
            if (!t.running || t.pending_delete) continue;

            t.elapsed_ns += step_ns;

            if (t.elapsed_ns >= t.duration_ns) {
                if (t.callback) {
                    t.callback(t.user_data);
                }

                if (t.loop) {
                    while (t.elapsed_ns >= t.duration_ns && t.duration_ns > 0) {
                        t.elapsed_ns -= t.duration_ns;

                    }
                }
                else {
                    t.running = false;
                    t.elapsed_ns = 0;
                    t.pending_delete = true;
                }
            }
        }

        auto it = std::remove_if(timers.begin(), timers.end(), [](const TimerEntry& t) { return t.pending_delete; });
        if (it != timers.end()) timers.erase(it, timers.end());
    }

    TimerEntry* find_timer(Ks_Handle h) {
        for (auto& t : timers) {
            if (t.handle == h && !t.pending_delete) return &t;
        }
        return nullptr;
    }

    Ks_Handle create_timer(ks_uint64 duration, bool loop) {
        TimerEntry t;
        t.handle = ks_handle_make(time_handle_id);
        t.duration_ns = duration;
        t.loop = loop;
        t.elapsed_ns = 0;
        t.running = false;
        t.pending_delete = false;
        t.callback = nullptr;
        t.user_data = nullptr;
        timers.push_back(t);
        return t.handle;
    }
};

KS_API Ks_TimeManager ks_time_manager_create() {
    return new (ks_alloc(sizeof(TimeManager_Impl), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA)) TimeManager_Impl();
}
KS_API ks_no_ret ks_time_manager_destroy(Ks_TimeManager tm) {
    if (tm) {
        static_cast<TimeManager_Impl*>(tm)->~TimeManager_Impl();
        ks_dealloc(tm);
    }
}

KS_API ks_no_ret ks_time_manager_update(Ks_TimeManager tm) { static_cast<TimeManager_Impl*>(tm)->update(); }
KS_API ks_no_ret ks_time_manager_process_timers(Ks_TimeManager tm) { static_cast<TimeManager_Impl*>(tm)->process_timers(); }

KS_API ks_uint64 ks_time_get_total_ns(Ks_TimeManager tm) { return static_cast<TimeManager_Impl*>(tm)->total_elapsed_ns; }
KS_API ks_float ks_time_get_delta_sec(Ks_TimeManager tm) { return static_cast<TimeManager_Impl*>(tm)->delta_time_sec; }
KS_API ks_no_ret ks_time_set_scale(Ks_TimeManager tm, ks_float scale) { static_cast<TimeManager_Impl*>(tm)->time_scale = scale; }
KS_API ks_float ks_time_get_scale(Ks_TimeManager tm) { return static_cast<TimeManager_Impl*>(tm)->time_scale; }

KS_API Ks_Handle ks_timer_create(Ks_TimeManager tm, ks_uint64 duration_ns, ks_bool loop) {
    return static_cast<TimeManager_Impl*>(tm)->create_timer(duration_ns, loop);
}

KS_API ks_no_ret ks_timer_destroy(Ks_TimeManager tm, Ks_Handle handle) {
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    if (t) t->pending_delete = true;
}

KS_API ks_no_ret ks_timer_start(Ks_TimeManager tm, Ks_Handle handle) {
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    if (t) { t->running = true; }
}

KS_API ks_no_ret ks_timer_stop(Ks_TimeManager tm, Ks_Handle handle) {
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    if (t) t->running = false;
}

KS_API ks_no_ret ks_timer_reset(Ks_TimeManager tm, Ks_Handle handle) {
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    if (t) t->elapsed_ns = 0;
}

KS_API ks_bool ks_timer_is_running(Ks_TimeManager tm, Ks_Handle handle) {
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    return t ? t->running : false;
}

ks_bool ks_timer_is_looping(Ks_TimeManager tm, Ks_Handle handle)
{
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    return t ? t->loop : false;
}

KS_API ks_no_ret ks_timer_set_duration(Ks_TimeManager tm, Ks_Handle handle, ks_uint64 duration_ns) {
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    if (t) t->duration_ns = duration_ns;
}

KS_API ks_no_ret ks_timer_set_loop(Ks_TimeManager tm, Ks_Handle handle, ks_bool loop) {
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    if (t) t->loop = loop;
}

KS_API ks_no_ret ks_timer_set_callback(Ks_TimeManager tm, Ks_Handle handle, ks_timer_callback callback, ks_ptr user_data) {
    auto* t = static_cast<TimeManager_Impl*>(tm)->find_timer(handle);
    if (t) {
        t->callback = callback;
        t->user_data = user_data;
    }
}