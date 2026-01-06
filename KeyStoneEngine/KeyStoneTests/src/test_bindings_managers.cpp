#include <doctest/doctest.h>
#include <keystone.h>
#include "../include/common.h"
#include <thread>
#include <fstream>

static int g_res_int = 0;
static float g_res_float = 0.0f;

void reset_static_vars() {
    g_res_int = 0;
    g_res_float = 0.0f;
}

struct NativeTestEvent {
    int id;
    float value;
};

struct SimpleAsset {
    int id;
};

struct GameConfig {
    int difficulty;
    float volume;
};

Ks_AssetData simple_load(ks_str p) {
    SimpleAsset* a = (SimpleAsset*)ks_alloc(sizeof(SimpleAsset), KS_LT_USER_MANAGED, KS_TAG_RESOURCE);
    a->id = 123;
    return (Ks_AssetData)a;
}

void construct_GameConfig(GameConfig* self) {
    new (self) GameConfig();
    self->difficulty = 1;
    self->volume = 0.5f;
}

void simple_free(Ks_AssetData d) {
    ks_dealloc(d);
}

void register_structs_test_reflection() {
    if (!ks_reflection_get_type("SimpleAsset")) {
        ks_reflect_struct(SimpleAsset,
            ks_reflect_field(int, id)
        );
    }

    if (!ks_reflection_get_type("GameConfig")) {
        ks_reflect_struct(GameConfig,
            ks_reflect_field(int, difficulty),
            ks_reflect_field(float, volume),
            ks_reflect_vtable_begin(GameConfig),
            ks_reflect_constructor(construct_GameConfig, ks_no_args()),
            ks_reflect_vtable_end()
        );
    }

    if (!ks_reflection_get_type("NativeTestEvent")) {
        ks_reflect_struct(NativeTestEvent,
            ks_reflect_field(int, id),
            ks_reflect_field(float, value)
        );
    }
}

void register_native_event_lua(Ks_Script_Ctx ctx) {
    auto b = ks_script_usertype_begin(ctx, "NativeTestEvent", sizeof(NativeTestEvent));
    ks_script_usertype_add_field(b, "id", KS_TYPE_INT, offsetof(NativeTestEvent, id), nullptr);
    ks_script_usertype_add_field(b, "value", KS_TYPE_FLOAT, offsetof(NativeTestEvent, value), nullptr);
    ks_script_usertype_end(b);
}

TEST_CASE("Bindings: Managers Integration") {
    ks_memory_init();
    ks_reflection_init();
    reset_static_vars();

    register_structs_test_reflection();

    Ks_Script_Ctx ctx = ks_script_create_ctx();
    ks_types_lua_bind(ctx);

    register_native_event_lua(ctx);

    ks_script_set_global(ctx, "verify_results",
        ks_script_create_cfunc(ctx, KS_SCRIPT_FUNC_VOID([](Ks_Script_Ctx c) {
            g_res_int = (int)ks_script_obj_as_integer(c, ks_script_get_arg(c, 1));
            g_res_float = (float)ks_script_obj_as_number(c, ks_script_get_arg(c, 2));
        return 0;
        }))
    );

    SUBCASE("Event Manager: C++ Native Publish -> Lua Subscribe") {
        Ks_EventManager em = ks_event_manager_create();
        ks_event_manager_lua_bind(em, ctx);

        Ks_Handle native_test_e = ks_event_manager_register_type(em, "NativeTestEvent");

        const char* script = R"(
            local e_h = events.get_handle("NativeTestEvent")
            events.subscribe(e_h, function(evt)
                verify_results(evt.id, evt.value)
            end)
        )";

        Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));

        NativeTestEvent evt_data;
        evt_data.id = 1234;
        evt_data.value = 56.78f;

        ks_event_manager_publish(em, native_test_e, &evt_data);

        CHECK(g_res_int == 1234);
        CHECK(g_res_float == doctest::Approx(56.78f));

        ks_event_manager_destroy(em);
    }

    SUBCASE("Assets Manager Binding") {
        Ks_JobManager jm = ks_job_manager_create();
        Ks_AssetsManager am = ks_assets_manager_create();
        ks_assets_manager_lua_bind(ctx, am, jm);

        Ks_IAsset iface = { simple_load, nullptr, simple_free };
        ks_assets_manager_register_asset_type(am, "SimpleAsset", iface);

        auto b = ks_script_usertype_begin_from_ref(ctx, "SimpleAsset");
        REQUIRE(b != nullptr);
        ks_script_usertype_end(b);

        const char* script = R"(
            local h = assets.load("SimpleAsset", "item1", "path")
            if assets.valid(h) == 0 then return -1 end
            return h.id
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res)) == 123);

        ks_assets_manager_destroy(am);
        ks_job_manager_destroy(jm);
    }

    SUBCASE("State Manager Binding") {
        Ks_StateManager sm = ks_state_manager_create();
        ks_state_manager_lua_bind(sm, ctx);

        const char* script = R"(
            local s = state("player_hp", 100)
            s:set(50)
            return s:get()
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res)) == 50);

        Ks_Handle h = ks_state_manager_get_handle(sm, "player_hp");
        CHECK(ks_state_get_int(sm, h) == 50);

        ks_state_manager_destroy(sm);
    }

    SUBCASE("State Manager: Reflection Binding") {
        Ks_StateManager sm = ks_state_manager_create();
        ks_state_manager_lua_bind(sm, ctx);

        auto b = ks_script_usertype_begin_from_ref(ctx, "GameConfig");
        REQUIRE(b != nullptr);
        ks_script_usertype_end(b);

        const char* script = R"(
            local cfg = GameConfig()
            
            if cfg.difficulty ~= 1 then return -1 end
            
            cfg.difficulty = 3
            local s_cfg = state("config", cfg)
            
            local saved = s_cfg:get()
            saved.difficulty = 5 
            
            return saved.difficulty
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res)) == 5);

        Ks_Handle h_cfg = ks_state_manager_get_handle(sm, "config");
        const char* t_name;
        void* ptr = ks_state_get_usertype_info(sm, h_cfg, &t_name, nullptr);

        CHECK(strcmp(t_name, "GameConfig") == 0);
        CHECK(((GameConfig*)ptr)->difficulty == 5);

        ks_state_manager_destroy(sm);
    }

    ks_script_destroy_ctx(ctx);
    ks_reflection_shutdown();
    ks_memory_shutdown();
}