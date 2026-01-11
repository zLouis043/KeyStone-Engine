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

void pos_init(Position* self) {
    self->x = 0; self->y = 0;
}

void pos_init_fill(Position* self, float val) {
    self->x = val; self->y = val;
}

void pos_init_set(Position* self, float x, float y) {
    self->x = x; self->y = y;
}

void register_ecs_structs_reflection() {
    if (!ks_reflection_get_type(ks_type_id(Position))) {
        ks_reflect_struct(Position,
            ks_reflect_field(float, x),
            ks_reflect_field(float, y),
            ks_reflect_vtable_begin(Position),
            ks_reflect_constructor(pos_init, ks_no_args()),
            ks_reflect_constructor(pos_init_fill, ks_args(ks_arg(float, val))),
            ks_reflect_constructor(pos_init_set, ks_args(ks_arg(float, x), ks_arg(float, y))),
            ks_reflect_vtable_end()
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

            local pos = Position(10, 20)
            
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
            
            local grunt = ecs.instantiate("OrcBase")
            local info = grunt:get("OrcInfo")
            
            if info.rank ~= 1 then return -1 end
            
            info.rank = 2
            
            local grunt2 = ecs.instantiate("OrcBase")
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

    SUBCASE("Advanced Features: Hierarchy, Prefabs, Observers") {
        ks_script_begin_scope(ctx);
        const char* script = R"(            
            local my_prefab = ecs.Prefab("EnemyPrefab", {
                Position(10, 20)
            })
            
            local instance = ecs.instantiate(my_prefab)
            local pos = instance:get("Position")

            if (pos.x ~= 10 or pos.y ~= 20) then 
                error('Prefab instantiation failed. Expected (10, 20), Got (' .. tostring(pos.x) .. ', ' .. tostring(pos.y) .. ')')
            end
            
            local child = ecs.Entity("Child")
            instance:add_child(child)
            
            local parent = child:parent()
            if not parent then error("Hierarchy parent failed") end

            local obs_triggered = false
            
            ecs.Observer("OnSet", "Position", function(e)
                obs_triggered = true
            end)
            
            instance:add(Position(50))
            
            return obs_triggered
        )";

        Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);

        if (!ks_script_call_succeded(ctx, res)) {
            FAIL(ks_script_get_last_error_str(ctx));
        }

        CHECK(ks_script_obj_as_boolean(ctx, ks_script_call_get_return(ctx, res)) == true);

        ks_script_end_scope(ctx);
    }

    SUBCASE("Systems, Phases & Queries") {
        ks_script_begin_scope(ctx);

        const char* setup_script = R"(
            TagA = ecs.Component("TagA")
            TagB = ecs.Component("TagB")
            Val  = ecs.Component("Val", { v = 0 })

            execution_log = ""

            ecs.System("SysPre", "PreUpdate", "Val", function(e)
                execution_log = execution_log .. "PRE|"
            end)

            ecs.System("SysUpdate", "OnUpdate", "Val", function(e)
                execution_log = execution_log .. "UPD|"
                local data = e:get("Val")
                data.v = data.v + 1
            end)

            ecs.System("SysPost", "PostUpdate", "Val", function(e)
                execution_log = execution_log .. "PST"
            end)

            function run_query_checks()
                local count_a = 0
                local count_ab = 0
                
                ecs.Query("TagA", function(e) 
                    count_a = count_a + 1 
                end)

                ecs.Query("TagA, TagB", function(e) 
                    count_ab = count_ab + 1 
                end)

                return count_a, count_ab
            end

            function get_log() return execution_log end
        )";

        ks_script_do_cstring(ctx, setup_script);

        ks_script_do_cstring(ctx, "sys_ent = ecs.Entity('SysEnt', { Val{v=0} })");

        ks_script_do_cstring(ctx, "e1 = ecs.Entity('E1', { TagA{} })");
        ks_script_do_cstring(ctx, "e2 = ecs.Entity('E2', { TagA{}, TagB{} })");
        ks_script_do_cstring(ctx, "e3 = ecs.Entity('E3', { TagB{} })");

        ks_ecs_progress(world, 0.016f);

        Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, "return get_log()");
        Ks_Script_Object log_res = ks_script_call_get_return(ctx, res);
        const char* log_str = ks_script_obj_as_cstring(ctx, log_res);

        CHECK(strcmp(log_str, "PRE|UPD|PST") == 0);

        res = ks_script_do_cstring(ctx, "return sys_ent:get('Val').v");
        int val = (int)ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res));
        CHECK(val == 1);

        ks_script_do_cstring(ctx, "return run_query_checks()");

        res = ks_script_do_cstring(ctx, "c_a, c_ab = run_query_checks(); return c_a");
        int count_a = (int)ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res));
        CHECK(count_a == 2);

        res = ks_script_do_cstring(ctx, "c_a, c_ab = run_query_checks(); return c_ab");
        int count_ab = (int)ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res));
        CHECK(count_ab == 1);

        ks_script_end_scope(ctx);
    }

    ks_ecs_lua_shutdown(world);
    ks_ecs_destroy_world(world);
    ks_script_destroy_ctx(ctx);
    ks_reflection_shutdown();
    ks_memory_shutdown();
}