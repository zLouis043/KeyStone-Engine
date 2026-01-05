#include <doctest/doctest.h>
#include <keystone.h>
#include "../include/common.h"

TEST_CASE("Bindings: Reflection VTable & FFI") {
    ks_memory_init();
    Ks_Script_Ctx ctx = ks_script_create_ctx();

    RegisterCommonReflection();

    SUBCASE("Automatic Binding (begin_from_ref)") {
        auto b = ks_script_usertype_begin_from_ref(ctx, "TestHero");
        REQUIRE(b != nullptr);
        ks_script_usertype_end(b);

        const char* script = R"(
            local h = TestHero(10, 20)
            
            if h.x ~= 10 or h.y ~= 20 then return "Init Failed" end
            if h.hp ~= 100 then return "HP Default Failed" end
            
            h:TestHero_Move(5, -5)
            if h.x ~= 15 or h.y ~= 15 then return "Move Failed" end
            
            if h:TestHero_GetHP() ~= 100 then return "GetHP Failed" end
            
            h:TestHero_SetActive(false)
            if h.is_active ~= false then return "SetActive Failed" end
            
            if TestHero.TestHero_GetTotalHeroes then
                if TestHero.TestHero_GetTotalHeroes() ~= 999 then return "Static Call Wrong" end
            else
                return "Static Missing"
            end
            
            return "OK"
        )";

        auto res = ks_script_do_cstring(ctx, script);
        if (!ks_script_call_succeded(ctx, res)) {
            FAIL(ks_script_get_last_error_str(ctx));
        }
        CHECK(strcmp(ks_script_obj_as_cstring(ctx, ks_script_call_get_return(ctx, res)), "OK") == 0);
    }

    SUBCASE("Manual Override Priority") {
        auto b = ks_script_usertype_begin_from_ref(ctx, "TestHero");

        ks_script_usertype_add_method(b, "TestHero_GetHP", KS_SCRIPT_FUNC_VOID([](Ks_Script_Ctx c) {
            ks_script_stack_push_integer(c, 0);
            return 1;
            }));

        ks_script_usertype_end(b);

        const char* script = R"(
            local h = TestHero(10, 10)
            return h:TestHero_GetHP()
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return(ctx, res)) == 0);
    }

    SUBCASE("Reflection VTable: Overload Resolution") {
        auto b = ks_script_usertype_begin_from_ref(ctx, "OverloadTester");
        ks_script_usertype_end(b);

        const char* script = R"(
            local o = OverloadTester()
            if o.last_result ~= -1 then return "Ctor Fail" end
            
            o:exec() 
            if o.last_result ~= 0 then return "Overload Void Failed" end
            
            o:exec(42)
            if o.last_result ~= 42 then return "Overload Int Failed" end
            
            o:exec(10, 20)
            if o.last_result ~= 30 then return "Overload Int,Int Failed" end
            
            o:exec("hello") -- len 5
            if o.last_result ~= 5 then return "Overload String Failed" end
            
            return "OK"
        )";

        auto res = ks_script_do_cstring(ctx, script);
        if (!ks_script_call_succeded(ctx, res)) {
            FAIL(ks_script_get_last_error_str(ctx));
        }
        CHECK(strcmp(ks_script_obj_as_cstring(ctx, ks_script_call_get_return(ctx, res)), "OK") == 0);
    }

    SUBCASE("Automatic Metamethods (__tostring)") {
        auto b = ks_script_usertype_begin_from_ref(ctx, "TestHero");
        ks_script_usertype_end(b);

        const char* script = R"(
            local h = TestHero(10, 20)
            
            local s = tostring(h)
            
            if s ~= 'Hero' then
                return "ToString Failed: " ..tostring(s)
            end

            return "OK"
        )";

        auto res = ks_script_do_cstring(ctx, script);
        if (!ks_script_call_succeded(ctx, res)) {
            FAIL(ks_script_get_last_error_str(ctx));
        }
        CHECK(strcmp(ks_script_obj_as_cstring(ctx, ks_script_call_get_return(ctx, res)), "OK") == 0);
    }

    ks_script_destroy_ctx(ctx);
    ks_memory_shutdown();
}