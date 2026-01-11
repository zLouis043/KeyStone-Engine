#include <doctest/doctest.h>
#include <keystone.h>
#include "../include/common.h"
#include <string.h>

static int g_ecs_res_int = 0;
static float g_ecs_res_float = 0.0f;

void reset_ecs_vars() {
    g_ecs_res_int = 0;
    g_ecs_res_float = 0.0f;
}

struct Position {
    float x, y;
};

struct Velocity {
    float x, y;
};

struct Health {
    int hp;
    int max_hp;
};

void register_ecs_structs_reflection() {
    if (!ks_reflection_get_type(ks_type_id(Position))) {
        ks_reflect_struct(Position,
            ks_reflect_field(float, x),
            ks_reflect_field(float, y)
        );
    }

    if (!ks_reflection_get_type(ks_type_id(Velocity))) {
        ks_reflect_struct(Velocity,
            ks_reflect_field(float, x),
            ks_reflect_field(float, y)
        );
    }
}

TEST_CASE("Bindings: ECS Integration") {
    ks_memory_init();
    ks_reflection_init();
    reset_ecs_vars();

    register_ecs_structs_reflection();

    Ks_Script_Ctx ctx = ks_script_create_ctx();
    ks_types_lua_bind(ctx);

    Ks_Ecs_World world = ks_ecs_create_world();
    ks_ecs_lua_bind(world, ctx);

    {
        auto b = ks_script_usertype_begin_from_ref(ctx, "Position");
        REQUIRE(b != nullptr);
        ks_script_usertype_end(b);

        b = ks_script_usertype_begin_from_ref(ctx, "Velocity");
        REQUIRE(b != nullptr);
        ks_script_usertype_end(b);
    }

    SUBCASE("Entity Creation & Native Component (UserData)") {
        const char* script = R"(
            local e = ecs.Entity("Hero")

            local pos = Position()
            pos.x = 10.0
            pos.y = 20.0
            
            e:add(pos)
            
            local get_pos = e:get("Position")
            if get_pos.x ~= 10.0 then return -1 end

            get_pos.x = 99.0
            return 0
        )";

        Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res)) == 0);

        Ks_Entity hero = ks_ecs_lookup(world, "Hero");
        CHECK(hero != 0);

        Position* p = (Position*)ks_ecs_get_component_mut(world, hero, "Position");
        CHECK(p != nullptr);
        CHECK(p->x == doctest::Approx(99.0f));
    }

    SUBCASE("Script Components (Pure Lua Tables)") {
        const char* script = R"(
            local Health = ecs.Component("Health", { hp = 100 })
            
            local e = ecs.Entity("Mage", {
                Health { hp = 50 }
            })
            
            local h = e:get("Health")
            if h.hp ~= 50 then return -1 end
            
            h.hp = 10
            return 0
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res)) == 0);

        Ks_Entity mage = ks_ecs_lookup(world, "Mage");
        CHECK(ks_ecs_has_component(world, mage, "Health"));
    }

    SUBCASE("Prefabs & Instantiation") {
        const char* script = R"(
            local OrcInfo = ecs.Component("OrcInfo", { rank = 1 })

            local OrcPrefab = ecs.Prefab("OrcBase", {
                OrcInfo { rank = 1 },
                Position()
            })
            
            local grunt = ecs.create_instance("OrcBase")
            local info = grunt:get("OrcInfo")
            
            if info.rank ~= 1 then return -1 end
            
            info.rank = 2
            
            local grunt2 = ecs.create_instance("OrcBase")
            if grunt2:get("OrcInfo").rank ~= 1 then return -2 end
            
            return 1
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res)) == 1);
    }

    SUBCASE("Entity Lifecycle: Destroy") {
        const char* script = R"(
            local e = ecs.Entity("Temp")
            e:destroy()
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));

        CHECK(ks_ecs_is_alive(world, ks_ecs_lookup(world, "Temp")) == false);
    }

    SUBCASE("Script Component Memory Saturation Test (10 Batches)") {
        const char* setup = R"(
        Health = ecs.Component("Health", { hp = 100 })
        )";
        ks_script_do_cstring(ctx, setup);

        for (int k = 0; k < 5; k++) ks_script_gc_collect(ctx);
        
        ks_size baseline = ks_memory_get_stats().tag_stats[KS_TAG_SCRIPT].total_size;
        ks_size prev_mem = baseline;

        KS_LOG_INFO("=== START SATURATION TEST (Baseline: %llu) ===", baseline);

        for (int batch = 0; batch < 10; ++batch) {
            ks_script_begin_scope(ctx);
            for (int i = 0; i < 100; i++) {
                const char* script = R"(
                local e = ecs.Entity("Player", { 
                    Health { hp = 50 } 
                })
                e:destroy()
             )";
                ks_script_do_cstring(ctx, script);
            }
            ks_script_end_scope(ctx);

            for (int k = 0; k < 5; k++) ks_script_gc_collect(ctx);

            ks_size current_mem = ks_memory_get_stats().tag_stats[KS_TAG_SCRIPT].total_size;
            long long delta = (long long)current_mem - (long long)prev_mem;

            KS_LOG_INFO("Batch %d End Mem: %llu (Delta: %lld)", batch + 1, current_mem, delta);
            ks_size refs_alive = ks_script_debug_get_registry_size(ctx);
            KS_LOG_INFO("Registry refs alive: %llu", refs_alive);
            prev_mem = current_mem;
        }

        ks_size total_growth = prev_mem - baseline;
        KS_LOG_INFO("Total Growth after 1000 entities: %llu bytes", total_growth);

        CHECK(total_growth < 20000);
    }

    for (int k = 0; k < 10; k++) {
    ks_script_gc_collect(ctx);
    }

    ks_ecs_lua_shutdown(world);
    ks_ecs_destroy_world(world);
    ks_script_destroy_ctx(ctx);
    ks_reflection_shutdown();
    ks_memory_shutdown();
}