#include <doctest/doctest.h>
#include <keystone.h>
#include <thread>
#include <chrono>

struct TimerTestContext {
    int call_count = 0;
    Ks_Handle handle_ref = 0;
};

void c_timer_callback(void* user_data) {
    auto* ctx = (TimerTestContext*)user_data;
    ctx->call_count++;
}

TEST_CASE("C API: Time Manager Core") {
    ks_memory_init();
    Ks_TimeManager tm = ks_time_manager_create();

    REQUIRE(tm != nullptr);

    SUBCASE("Clock Updates & Scaling") {
        ks_time_manager_update(tm);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ks_time_manager_update(tm);

        float dt = ks_time_get_delta_sec(tm);
        CHECK(dt >= 0.04f);
        CHECK(dt <= 0.1f);

        ks_time_set_scale(tm, 2.0f);
        CHECK(ks_time_get_scale(tm) == 2.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ks_time_manager_update(tm);

        float scaled_dt = ks_time_get_delta_sec(tm);
        CHECK(scaled_dt >= 0.09f);

        ks_time_set_scale(tm, 1.0f);
    }

    SUBCASE("One-Shot Timer") {
        TimerTestContext ctx;
        
        ks_time_manager_update(tm); 

        Ks_TimerHandle h = ks_timer_create(tm, 200000000ULL, ks_false);  200ms
        ctx.handle_ref = h;

        ks_timer_set_callback(tm, h, c_timer_callback, &ctx);
        ks_timer_start(tm, h);

        CHECK(ks_timer_is_running(tm, h) == ks_true);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ks_time_manager_update(tm);
        ks_time_manager_process_timers(tm);
        
        CHECK(ctx.call_count == 0); 
        for(int i=0; i<4; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ks_time_manager_update(tm);
            ks_time_manager_process_timers(tm);
            if (ctx.call_count == 1) break;
        }
        
        CHECK(ctx.call_count == 1);
        CHECK(ks_timer_is_running(tm, h) == ks_false);
    }

    SUBCASE("Looping Timer") {
        TimerTestContext ctx;

        ks_time_manager_update(tm);

        Ks_Handle h = ks_timer_create(tm, 50000000ULL, ks_true); // 50ms Loop
        ks_timer_set_callback(tm, h, c_timer_callback, &ctx);
        ks_timer_start(tm, h);

        for (int i = 0; i < 4; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            ks_time_manager_update(tm);
            ks_time_manager_process_timers(tm);
        }

        CHECK(ctx.call_count >= 3);
        CHECK(ks_timer_is_running(tm, h) == ks_true);

        ks_timer_destroy(tm, h);
    }

    SUBCASE("Manual Stop & Destroy") {
        TimerTestContext ctx;
        ks_time_manager_update(tm);
        
        Ks_Handle h = ks_timer_create(tm, 10000000ULL, ks_false); // 10ms
        ks_timer_set_callback(tm, h, c_timer_callback, &ctx);
        ks_timer_start(tm, h);

        ks_timer_stop(tm, h);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ks_time_manager_update(tm);
        ks_time_manager_process_timers(tm);

        CHECK(ctx.call_count == 0);

        ks_timer_start(tm, h);
        ks_timer_destroy(tm, h);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ks_time_manager_update(tm);
        ks_time_manager_process_timers(tm);

        CHECK(ctx.call_count == 0);
    }

    ks_time_manager_destroy(tm);
    ks_memory_shutdown();
}