#include <doctest/doctest.h>
#include <keystone.h>
#include <chrono>
#include <vector>
#include <iostream>

template<typename Func>
long long measure_ms(Func&& f) {
    auto start = std::chrono::high_resolution_clock::now();
    f();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

ks_returns_count vec3_ctor_bench(Ks_Script_Ctx ctx) {
    return 0;
}

TEST_CASE("Performance Benchmarks") {
    ks_memory_init();
    Ks_Script_Ctx ctx = ks_script_create_ctx();
    Ks_EventManager em = ks_event_manager_create();
    ks_event_manager_lua_bind(em, ctx);

    ks_types_lua_bind(ctx);

    SUBCASE("Benchmark: Lua -> C++ Call Overhead (100k calls)") {
        const char* script = R"(
            local h = events.register("PerfEvent", {type.INT})
            
            local sub = events.subscribe(h, function(v) end) 
            
            local start = os.clock()
            for i=1, 100000 do
                events.publish(h, i)
            end
        )";

        long long duration = measure_ms([&]() {
            ks_script_do_cstring(ctx, script);
            });

        //WARN(duration < 500);
        KS_LOG_TRACE("[PERF] 100k Event Publishes (Lua->C++->Lua): %lld ms", duration);
    }

    SUBCASE("Benchmark: Userdata Creation (100k allocs)") {
        auto b = ks_script_usertype_begin(ctx, "Vec3", 12);
        ks_script_usertype_add_constructor(b, KS_SCRIPT_FUNC_VOID(vec3_ctor_bench));
        ks_script_usertype_end(b);

        const char* alloc_script = R"(
            local v
            for i=1, 100000 do
                v = Vec3()
            end
        )";

        long long duration = measure_ms([&]() {
            ks_script_do_cstring(ctx, alloc_script);
            });

        KS_LOG_TRACE("[PERF] 100k Userdata Creations: %lld ms", duration);
    }

    ks_event_manager_destroy(em);
    ks_script_destroy_ctx(ctx);
    ks_memory_shutdown();
}