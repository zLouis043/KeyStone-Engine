#include <doctest/doctest.h>
#include <keystone.h>
#include "../include/common.h"

void register_common_lua_usertypes(Ks_Script_Ctx ctx) {
    auto b = ks_script_usertype_begin_from_ref(ctx, "Vec4");
    ks_script_usertype_add_constructor(b, KS_SCRIPT_FUNC([](Ks_Script_Ctx c) {
        float v = (float)ks_script_obj_as_number(c, ks_script_get_arg(c, 1));
        Vec4* v4 = new(ks_script_get_self(c)) Vec4();
        v4->x = v;
        v4->y = v;
        v4->z = v;
        v4->w = v;
        return 0;
        }, KS_TYPE_FLOAT));
    ks_script_usertype_end(b);
    b = ks_script_usertype_begin_from_ref(ctx, "MixedData");
    ks_script_usertype_end(b);
}

TEST_CASE("Bindings: Reflection VTable & FFI") {
    ks_memory_init();
    Ks_Script_Ctx ctx = ks_script_create_ctx();

    RegisterCommonReflection();
    register_common_lua_usertypes(ctx);

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
            FAIL(ks_error_get_last_error().message);
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
            FAIL(ks_error_get_last_error().message);
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
            FAIL(ks_error_get_last_error().message);
        }
        CHECK(strcmp(ks_script_obj_as_cstring(ctx, ks_script_call_get_return(ctx, res)), "OK") == 0);
    }

    SUBCASE("Mixed Sources: Reflection Overloads + Manual Binding") {
        auto b = ks_script_usertype_begin_from_ref(ctx, "PowerValue");

        ks_script_usertype_add_constructor(b, KS_SCRIPT_FUNC([](Ks_Script_Ctx c) {
            int v = (int)ks_script_obj_as_number(c, ks_script_get_arg(c, 1));
            PowerValue *self = new(ks_script_get_self(c)) PowerValue();

            self->generation = 0;
            self->value = v;

            return 0;
        }, KS_TYPE_INT));

        ks_script_usertype_add_metamethod(b, KS_SCRIPT_MT_SUB,
            KS_SCRIPT_FUNC(l_powervalue_sub, KS_TYPE_USERDATA, KS_TYPE_INT)
        );

        ks_script_usertype_end(b);

        const char* script = R"(
            local p1 = PowerValue(100)
            local p2 = PowerValue(50)

            local s = tostring(p1)

            local p_sum_struct = p1 + p2

            local p_sum_int = p1 + 10

            local p_sub = p1 - 20

            return s, p_sum_struct.value, p_sum_int.value, p_sub.value
        )";

        auto res = ks_script_do_cstring(ctx, script);

        if (!ks_script_call_succeded(ctx, res)) {
            FAIL(ks_error_get_last_error().message);
        }

        CHECK(strcmp(ks_script_obj_as_cstring(ctx, ks_script_call_get_return_at(ctx, res, 1)), "PowerValue(69)") == 0);
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return_at(ctx, res, 2)) == 150);
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return_at(ctx, res, 3)) == 110);
        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return_at(ctx, res, 4)) == 80);
    }

    SUBCASE("FFI: Float Structs (Register Mapping)") {
        const char* script = R"(
            local v1 = Vec4(1.5)
            local v2 = Vec4(0.5)

            local v3 = v1 + v2 

            return v3.x, v3.y, v3.z, v3.w
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));

        CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 1)) == doctest::Approx(2.0f));
        CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 2)) == doctest::Approx(2.0f));
        CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 3)) == doctest::Approx(2.0f));
        CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 4)) == doctest::Approx(2.0f));
    }

    SUBCASE("FFI: Mixed Alignment (Padding)") {
        const char* script = R"(
            local m = MixedData.Mixed_Make(42, 123.456)
            return m.count, m.val
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));

        CHECK(ks_script_obj_as_integer(ctx, ks_script_call_get_return_at(ctx, res, 1)) == 42);
        CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 2)) == doctest::Approx(123.456));
    }

    SUBCASE("Metamethods: Equality (__eq)") {
        const char* script = R"(
            local v1 = Vec4(10.0)
            local v2 = Vec4(10.0)
            local v3 = Vec4(20.0)

            local eq1 = (v1 == v2)
            local eq2 = (v1 == v3)

            return eq1, eq2
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));

        CHECK(ks_script_obj_as_boolean(ctx, ks_script_call_get_return_at(ctx, res, 1)) == true);
        CHECK(ks_script_obj_as_boolean(ctx, ks_script_call_get_return_at(ctx, res, 2)) == false);
    }

    SUBCASE("GC Stress Test: Header Integrity") {
        const char* script = R"(
            local acc = Vec4(0.0)
            local inc = Vec4(1.0)
 
            for i = 1, 1000 do
                acc = acc + inc
            end
            
            collectgarbage()
            
            return acc.x
        )";

        auto res = ks_script_do_cstring(ctx, script);
        CHECK(ks_script_call_succeded(ctx, res));
        CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res)) == doctest::Approx(1000.0f));
    }

    ks_script_destroy_ctx(ctx);
    ks_memory_shutdown();
}