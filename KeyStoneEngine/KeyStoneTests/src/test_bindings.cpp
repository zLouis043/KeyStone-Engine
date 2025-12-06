#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>

#include "../include/common.h"

static int g_c_int_val = 0;
static std::string g_c_str_val = "";
static bool g_c_bool_val = false;
static int g_c_call_count = 0;

void reset_c_globals() {
    g_c_int_val = 0;
    g_c_str_val = "";
    g_c_bool_val = false;
    g_c_call_count = 0;
}

ks_bool c_subscriber_cb(Ks_Event_Payload data, void* user_data) {
    g_c_call_count++;

    if (ks_event_get_args_count(data) >= 1)
        g_c_int_val = ks_event_get_int(data, 0);

    if (ks_event_get_args_count(data) >= 2)
        g_c_str_val = ks_event_get_cstring(data, 1);

    return ks_true;
}

struct GameConfig {
    int difficulty;
    float volume;
};

ks_returns_count test_set_received(Ks_Script_Ctx ctx) {
    g_c_int_val = (int)ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    const char* s = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 2));
    if (s) g_c_str_val = s;
    g_c_call_count++;
    return 0;
}

TEST_CASE("Managers-Lua bindings Tests") {
	ks_memory_init();

    reset_c_globals();

	Ks_Script_Ctx ctx = ks_script_create_ctx();

    ks_types_lua_bind(ctx);

    Ks_AssetsManager am = ks_assets_manager_create();

    ks_assets_manager_lua_bind(ctx, am);

    Ks_EventManager em = ks_event_manager_create();

    ks_event_manager_lua_bind(em, ctx);

    ks_script_set_global(ctx, "test_set_received", ks_script_create_cfunc(ctx, KS_SCRIPT_FUNC_VOID(test_set_received)));

    Ks_StateManager sm = ks_state_manager_create();
    ks_state_manager_lua_bind(sm, ctx);

	SUBCASE("Assets Manager Lua Bindings") {
		Ks_IAsset interface;
		interface.load_from_file_fn = my_asset_load_file;
		interface.destroy_fn = my_asset_destroy;

        ks_assets_manager_register_asset_type(am, "MyAsset", interface);

        auto b = ks_script_usertype_begin(ctx, "MyAsset", sizeof(MyCAsset));
        ks_script_usertype_add_field(b, "id", KS_TYPE_INT, offsetof(MyCAsset, id), nullptr);
        ks_script_usertype_end(b);

        const char* script = R"(
            local h = assets.load("MyAsset", "hero_tex", "path/to/tex.png")
            
            if assets.valid(h) == 0 then
                return "Invalid Handle"
            end
            
            local data = assets.get_data(h)

            local h_ref = assets.get("hero_tex")
        
            if h == h_ref then
                return data.id, "Success"
            else
                return "Handle Mismatch"
            end
        )";

        Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);

        if (!ks_script_call_succeded(ctx, res)) {
            FAIL(ks_script_get_last_error_str(ctx));
        }

        const char* ret_str = ks_script_obj_as_cstring(ctx, ks_script_call_get_return_at(ctx, res, 2));
        REQUIRE(ret_str != nullptr);
        CHECK(strcmp(ret_str, "Success") == 0);
	}

    SUBCASE("Event Manager: C++ Register -> Lua Subscribe -> C++ Publish") {
        Ks_Handle evt = ks_event_manager_register(em, "C_Event", KS_TYPE_INT, KS_TYPE_CSTRING);

        const char* script = R"(
            local h = events.get("C_Event")
            events.subscribe(h, function(num, str)
                test_set_received(num, str)
            end)
        )";

        Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));

        ks_event_manager_publish(em, evt, 42, "Hello Lua");

        CHECK(g_c_call_count == 1);
        CHECK(g_c_int_val == 42);
        CHECK(g_c_str_val == "Hello Lua");
    }

    SUBCASE("Event Manager: Lua Register -> C++ Subscribe -> Lua Publish") {
        const char* setup_script = R"(
            local h = events.register("Lua_Event", {type.INT, type.CSTRING})
            return h
        )";

        Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, setup_script);
        CHECK(ks_script_call_succeded(ctx, res));

        double h_val = ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res));
        Ks_Handle evt = (Ks_Handle)h_val;
        CHECK(evt != KS_INVALID_HANDLE);

        ks_event_manager_subscribe(em, evt, c_subscriber_cb, nullptr);

        const char* pub_script = R"(
            local h = events.get("Lua_Event")
            events.publish(h, 99, "From Lua")
        )";
        ks_script_do_cstring(ctx, pub_script);

        CHECK(g_c_call_count == 1);
        CHECK(g_c_int_val == 99);
        CHECK(g_c_str_val == "From Lua");
    }

    SUBCASE("Event Manager: Lua Register -> Lua Subscribe -> Lua Publish (Loopback)") {
        const char* script = R"(
            local h = events.register("Loop_Event", {type.BOOL})
            local received_val = false
            
            events.subscribe(h, function(b)
                received_val = b
            end)
            
            events.publish(h, true)
            
            if received_val == true then
                test_set_received(1, "OK")
            end
        )";

        ks_script_do_cstring(ctx, script);

        CHECK(g_c_call_count == 1);
        CHECK(g_c_int_val == 1);
        CHECK(g_c_str_val == "OK");
    }

    SUBCASE("Event Manager: Table Passing (No Unwrapping)") {
        Ks_Handle evt = ks_event_manager_register(em, "Table_Event", KS_TYPE_SCRIPT_TABLE);

        static bool table_verified = false;
        auto verify_table_cb = [](Ks_Event_Payload data, void*) -> ks_bool {
            Ks_Script_Ctx ctx = ks_event_get_script_ctx(data);
            Ks_Script_Table t = ks_event_get_script_table(data, 0);

            ks_script_begin_scope(ctx);

            Ks_Script_Object key = ks_script_create_cstring(ctx, "val");

            if (!ks_script_table_has(ctx, t, key)) {
                ks_script_end_scope(ctx);
                return false;
            }

            Ks_Script_Object val = ks_script_table_get(ctx, t, key);

            if (ks_script_obj_as_number(ctx, val) == 123.0) {
                table_verified = true;
            }

            ks_script_end_scope(ctx);

            return ks_true;
        };

        ks_event_manager_subscribe(em, evt, verify_table_cb, nullptr);

        const char* script = R"(
            local h = events.get("Table_Event")
            events.publish(h, { val = 123 })
        )";
        ks_script_do_cstring(ctx, script);

        CHECK(table_verified == true);
    }

    SUBCASE("State Manager Integration") {
        ks_script_begin_scope(ctx);

        const char* script_create = R"(
            local hp = state("hp", 100)
            local sp = state("speed", 5.5)
            return hp:get(), sp:get()
        )";

        auto res1 = ks_script_do_cstring(ctx, script_create);

        CHECK(ks_script_call_succeded(ctx, res1));
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return_at(ctx, res1, 1)) == 100);

        Ks_Handle h_hp = ks_state_manager_get_handle(sm, "hp");
        CHECK(ks_state_get_int(sm, h_hp) == 100);

        const char* script_update = R"(
            local hp = state("hp")
            hp:set(80)
            return hp:get()
        )";
        auto res2 = ks_script_do_cstring(ctx, script_update);
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res2)) == 80);
        CHECK(ks_state_get_int(sm, h_hp) == 80);

        const char* script_fail = R"(
            local hp = state("hp")
            hp:set("stringa")
            return hp:get()
        )";
        auto res3 = ks_script_do_cstring(ctx, script_fail);
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res3)) == 80);

        auto b = ks_script_usertype_begin(ctx, "GameConfig", sizeof(GameConfig));
        ks_script_usertype_add_constructor(b, KS_SCRIPT_FUNC_VOID([](Ks_Script_Ctx c) {
            auto* p = (GameConfig*)ks_script_get_self(c); p->difficulty = 1; p->volume = 0.5f; return 0;
            }));
        ks_script_usertype_add_field(b, "difficulty", KS_TYPE_INT, offsetof(GameConfig, difficulty), nullptr);
        ks_script_usertype_end(b);

        const char* script_obj = R"(
            local cfg = GameConfig()
            cfg.difficulty = 3
            
            local s_cfg = state("config", cfg)
            
            local saved = s_cfg:get()
            saved.difficulty = 5 
            
            return saved.difficulty
        )";

        auto res4 = ks_script_do_cstring(ctx, script_obj);
        CHECK(ks_script_call_succeded(ctx, res4));

        Ks_Handle h_cfg = ks_state_manager_get_handle(sm, "config");
        CHECK(h_cfg != KS_INVALID_HANDLE);

        const char* t_name;
        void* ptr = ks_state_get_usertype_info(sm, h_cfg, &t_name, nullptr);
        REQUIRE(ptr != nullptr);
        CHECK(strcmp(t_name, "GameConfig") == 0);

        CHECK(((GameConfig*)ptr)->difficulty == 5);

        ks_script_end_scope(ctx);
    }

    ks_state_manager_destroy(sm);

    ks_assets_manager_destroy(am);

    ks_event_manager_destroy(em);

	ks_script_destroy_ctx(ctx);

	ks_memory_shutdown();
}