#include <doctest/doctest.h>
#include <keystone.h>
#include "../include/common.h"
#include <thread>
#include <fstream>

struct SimpleAsset { int id; };
Ks_AssetData simple_load(ks_str p) {
    SimpleAsset* a = (SimpleAsset*)ks_alloc(sizeof(SimpleAsset), KS_LT_USER_MANAGED, KS_TAG_RESOURCE);
    a->id = 123;
    return (Ks_AssetData)a;
}
void simple_free(Ks_AssetData d) { ks_dealloc((void*)d); }

TEST_CASE("Bindings: Managers Integration") {
    ks_memory_init();
    ResetTestGlobals();

    Ks_Script_Ctx ctx = ks_script_create_ctx();
    ks_types_lua_bind(ctx);

    SUBCASE("Event Manager Binding") {
        Ks_EventManager em = ks_event_manager_create();
        ks_event_manager_lua_bind(em, ctx);

        Ks_Handle h = ks_event_manager_register(em, "TestEvent", KS_TYPE_INT);

        ks_script_set_global(ctx, "record_event",
            ks_script_create_cfunc_with_upvalues(ctx, KS_SCRIPT_FUNC_VOID([](Ks_Script_Ctx c) {
                g_event_int = (int)ks_script_obj_as_integer(c, ks_script_get_arg(c, 1));
                return 0;
                }), 0));

        const char* script = R"(
            local h = events.get("TestEvent")
            events.subscribe(h, function(val)
                record_event(val)
            end)
        )";
        ks_script_do_cstring(ctx, script);

        ks_event_manager_publish(em, h, 999);
        CHECK(g_event_int == 999);

        ks_event_manager_destroy(em);
    }

    SUBCASE("Assets Manager Binding") {
        Ks_JobManager jm = ks_job_manager_create();
        Ks_AssetsManager am = ks_assets_manager_create();
        ks_assets_manager_lua_bind(ctx, am, jm);

        Ks_IAsset iface = { simple_load, nullptr, simple_free };
        ks_assets_manager_register_asset_type(am, "Simple", iface);

        auto b = ks_script_usertype_begin(ctx, "Simple", sizeof(SimpleAsset));
        ks_script_usertype_add_field(b, "id", KS_TYPE_INT, offsetof(SimpleAsset, id), nullptr);
        ks_script_usertype_end(b);

        const char* script = R"(
            local h = assets.load("Simple", "item1", "path")
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

    ks_script_destroy_ctx(ctx);
    ks_memory_shutdown();
}