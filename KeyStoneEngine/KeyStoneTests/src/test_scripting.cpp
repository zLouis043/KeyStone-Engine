#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>
#include <string>
#include <new>
#include "../include/common.h"

TEST_CASE("C API: Script Engine Suite") {
    ks_memory_init();
    Ks_Script_Ctx ctx = ks_script_create_ctx();

    REQUIRE(ctx != nullptr);

    SUBCASE("Basics: Primitives & Execution") {
        ks_script_begin_scope(ctx); {

            Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, "return 42");
            CHECK(ks_script_call_succeded(ctx, res));
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res)) == 42.0);

            Ks_Script_Object num = ks_script_create_number(ctx, 123.456);
            CHECK(ks_script_obj_type(ctx, num) == KS_TYPE_DOUBLE);
            CHECK(ks_script_obj_as_number(ctx, num) == 123.456);

            Ks_Script_Object integer = ks_script_create_integer(ctx, 9223372036854775807LL);
            CHECK(ks_script_obj_type(ctx, integer) == KS_TYPE_INT);
            ks_int64 val = ks_script_obj_as_integer(ctx, integer);
            CHECK(val == 9223372036854775807LL);
            ks_script_stack_push_integer(ctx, 12345);
            ks_int64 popped = ks_script_stack_pop_integer(ctx);
            CHECK(popped == 12345);

            Ks_Script_Object str = ks_script_create_cstring(ctx, "KeyStone");
            CHECK(ks_script_obj_type(ctx, str) == KS_TYPE_CSTRING);
            CHECK(strcmp(ks_script_obj_as_cstring(ctx, str), "KeyStone") == 0);

            Ks_Script_Object boolean = ks_script_create_boolean(ctx, ks_true);
            CHECK(ks_script_obj_type(ctx, boolean) == KS_TYPE_BOOL);
            CHECK(ks_script_obj_as_boolean(ctx, boolean) == ks_true);



        } ks_script_end_scope(ctx);
    }

    SUBCASE("Stack Manipulation") {
        ks_script_begin_scope(ctx); {
            ks_script_stack_clear(ctx);
            CHECK(ks_script_stack_size(ctx) == 0);

            ks_script_stack_push_number(ctx, 10.0);
            ks_script_stack_push_number(ctx, 20.0);
            ks_script_stack_push_number(ctx, 30.0);
            CHECK(ks_script_stack_size(ctx) == 3);

            CHECK(ks_script_obj_as_number(ctx, ks_script_stack_peek(ctx, -1)) == 30.0);
            CHECK(ks_script_obj_as_number(ctx, ks_script_stack_peek(ctx, 1)) == 10.0);

            Ks_Script_Object popped = ks_script_stack_pop_obj(ctx);
            CHECK(ks_script_obj_as_number(ctx, popped) == 30.0);
            CHECK(ks_script_stack_size(ctx) == 2);

            ks_script_stack_push_number(ctx, 99.0);
            ks_script_stack_insert(ctx, 1);
            CHECK(ks_script_obj_as_number(ctx, ks_script_stack_peek(ctx, 1)) == 99.0);
            CHECK(ks_script_obj_as_number(ctx, ks_script_stack_peek(ctx, 2)) == 10.0);

            ks_script_stack_remove(ctx, 2);
            CHECK(ks_script_stack_size(ctx) == 2);
            CHECK(ks_script_obj_as_number(ctx, ks_script_stack_peek(ctx, 2)) == 20.0);

            ks_script_stack_push_number(ctx, 55.0);
            ks_script_stack_replace(ctx, 1);
            CHECK(ks_script_obj_as_number(ctx, ks_script_stack_peek(ctx, 1)) == 55.0);

            ks_script_stack_copy(ctx, 1, 2);
            CHECK(ks_script_obj_as_number(ctx, ks_script_stack_peek(ctx, 2)) == 55.0);

            ks_script_stack_clear(ctx);
            CHECK(ks_script_stack_size(ctx) == 0);

        } ks_script_end_scope(ctx);
    }

    SUBCASE("Lifetimes: Scopes, Promote & Free") {
        Ks_Script_Object promoted_obj;

        ks_script_begin_scope(ctx); {
            Ks_Script_Table t1 = ks_script_create_table(ctx);
            ks_script_table_set(ctx, t1, ks_script_create_cstring(ctx, "k"), ks_script_create_number(ctx, 100));

            promoted_obj = t1;
            ks_script_promote(ctx, promoted_obj);

            Ks_Script_Table t_garbage = ks_script_create_table(ctx);
        } ks_script_end_scope(ctx);

        CHECK(ks_script_obj_is_valid(ctx, promoted_obj));
        CHECK(ks_script_obj_type(ctx, promoted_obj) == KS_TYPE_SCRIPT_TABLE);

        Ks_Script_Object val = ks_script_table_get(ctx, static_cast<Ks_Script_Table>(promoted_obj), ks_script_create_cstring(ctx, "k"));
        CHECK(ks_script_obj_as_number(ctx, val) == 100.0);

        ks_script_free_obj(ctx, promoted_obj);
    }

    SUBCASE("Functions: C calling Lua & Lua calling C") {
        ks_script_begin_scope(ctx); {
            ks_script_cfunc add_func = [](Ks_Script_Ctx c) -> ks_returns_count {
                double a = ks_script_obj_as_number(c, ks_script_get_arg(c, 1));
                double b = ks_script_obj_as_number(c, ks_script_get_arg(c, 2));

                ks_script_stack_push_obj(c, ks_script_create_number(c, a + b));
                return 1;
                };

            Ks_Script_Function func_obj = ks_script_create_cfunc(ctx,
                KS_SCRIPT_FUNC(add_func, KS_TYPE_DOUBLE, KS_TYPE_DOUBLE)
            );

            CHECK(ks_script_obj_type(ctx, func_obj) == KS_TYPE_SCRIPT_FUNCTION);

            Ks_Script_Function_Call_Result res_c = ks_script_func_callv(ctx, func_obj,
                ks_script_create_number(ctx, 10),
                ks_script_create_number(ctx, 20)
            );

            CHECK(ks_script_call_succeded(ctx, res_c));

            Ks_Script_Object ret_val = ks_script_call_get_return(ctx, res_c);
            CHECK(ks_script_obj_as_number(ctx, ret_val) == 30.0);

            ks_script_set_global(ctx, "my_add", func_obj);

            const char* script = "return my_add(5, 7)";
            Ks_Script_Function_Call_Result res_lua = ks_script_do_cstring(ctx, script);

            CHECK(ks_script_call_succeded(ctx, res_lua));
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res_lua)) == 12.0);

        } ks_script_end_scope(ctx);
    }

    SUBCASE("Upvalues (C Closures)") {
        ks_script_begin_scope(ctx); {

            ks_script_cfunc counter_func = [](Ks_Script_Ctx c) -> ks_returns_count {
                Ks_Script_Object up_tbl = ks_script_func_get_upvalue(c, 1);
                Ks_Script_Object key = ks_script_create_cstring(c, "val");

                double val = ks_script_obj_as_number(c, ks_script_table_get(c, static_cast<Ks_Script_Table>(up_tbl), key));

                val += 1.0;
                ks_script_table_set(c, static_cast<Ks_Script_Table>(up_tbl), key, ks_script_create_number(c, val));

                ks_script_stack_push_obj(c, ks_script_create_number(c, val));
                return 1;
            };

            Ks_Script_Table state_tbl = ks_script_create_table(ctx);
            ks_script_table_set(ctx, state_tbl, ks_script_create_cstring(ctx, "val"), ks_script_create_number(ctx, 0));

            ks_script_stack_push_obj(ctx, state_tbl);

            Ks_Script_Function closure = ks_script_create_cfunc_with_upvalues(ctx, KS_SCRIPT_FUNC_VOID(counter_func), 1);

            Ks_Script_Function_Call_Result res1 = ks_script_func_callv(ctx, closure);
            CHECK(ks_script_call_succeded(ctx, res1));
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res1)) == 1.0);

            Ks_Script_Function_Call_Result res2 = ks_script_func_callv(ctx, closure);
            CHECK(ks_script_call_succeded(ctx, res2));
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res2)) == 2.0);

        } ks_script_end_scope(ctx);
    }

    SUBCASE("Tables: Set, Get, Iterate") {
        ks_script_begin_scope(ctx); {

            Ks_Script_Table tbl = ks_script_create_named_table(ctx, "config");
            CHECK(ks_script_obj_type(ctx, tbl) == KS_TYPE_SCRIPT_TABLE);

            ks_script_table_set(ctx, tbl,
                ks_script_create_cstring(ctx, "width"),
                ks_script_create_number(ctx, 1920));

            ks_script_table_set(ctx, tbl,
                ks_script_create_cstring(ctx, "fullscreen"),
                ks_script_create_boolean(ctx, ks_true));

            CHECK(ks_script_table_has(ctx, tbl, ks_script_create_cstring(ctx, "width")));

            Ks_Script_Object val_w = ks_script_table_get(ctx, tbl, ks_script_create_cstring(ctx, "width"));
            CHECK(ks_script_obj_as_number(ctx, val_w) == 1920.0);

            Ks_Script_Object val_fs = ks_script_table_get(ctx, tbl, ks_script_create_cstring(ctx, "fullscreen"));
            CHECK(ks_script_obj_as_boolean(ctx, val_fs) == ks_true);

            ks_script_table_set(ctx, tbl, ks_script_create_cstring(ctx, "version"), ks_script_create_number(ctx, 1.0));

            Ks_Script_Table_Iterator it = ks_script_table_iterate(ctx, tbl);
            int count = 0;
            Ks_Script_Object key, val;

            while (ks_script_iterator_next(ctx, &it, &key, &val)) {
                count++;
                CHECK(ks_script_obj_is_valid(ctx, key));
                CHECK(ks_script_obj_is_valid(ctx, val));
            }

            ks_script_iterator_destroy(ctx, &it);

            CHECK(count == 3);

        } ks_script_end_scope(ctx);
    }

    SUBCASE("Metatables") {
        ks_script_begin_scope(ctx); {

            Ks_Script_Table obj = ks_script_create_table(ctx);
            Ks_Script_Table mt = ks_script_create_table(ctx);

            ks_script_obj_set_metatable(ctx, obj, mt);

            CHECK(ks_script_obj_has_metatable(ctx, obj) == ks_true);

            Ks_Script_Table got_mt = ks_script_obj_get_metatable(ctx, obj);
            CHECK(ks_script_obj_type(ctx, got_mt) == KS_TYPE_SCRIPT_TABLE);

            ks_script_table_set(ctx, mt, ks_script_create_cstring(ctx, "flag"), ks_script_create_boolean(ctx, ks_true));

            Ks_Script_Object flag = ks_script_table_get(ctx, got_mt, ks_script_create_cstring(ctx, "flag"));
            CHECK(ks_script_obj_as_boolean(ctx, flag) == ks_true);

        } ks_script_end_scope(ctx);
    }

    SUBCASE("Userdata & LightUserdata") {
        ks_script_begin_scope(ctx); {

            int dummy_int = 42;
            void* ptr = &dummy_int;

            Ks_Script_LightUserdata lud = ks_script_create_lightuserdata(ctx, ptr);
            CHECK(ks_script_obj_type(ctx, lud) == KS_TYPE_LIGHTUSERDATA);

            void* got_ptr = ks_script_lightuserdata_get_ptr(ctx, lud);
            CHECK(got_ptr == ptr);

            struct MyData { int x; float y; };
            Ks_Script_Userdata ud = ks_script_create_userdata(ctx, sizeof(MyData));
            CHECK(ks_script_obj_type(ctx, ud) == KS_TYPE_USERDATA);

            Ks_UserData info = ks_script_obj_as_userdata(ctx, ud);
            REQUIRE(info.data != nullptr);
            CHECK(info.size == sizeof(MyData));

            MyData* data_ptr = (MyData*)info.data;
            REQUIRE(data_ptr != nullptr);
            data_ptr->x = 100;
            data_ptr->y = 3.14f;

            Ks_UserData info_read = ks_script_obj_as_userdata(ctx, ud);
            MyData* read_ptr = (MyData*)info_read.data;
            CHECK(read_ptr->x == 100);
            CHECK(read_ptr->y == 3.14f);

        } ks_script_end_scope(ctx);
    }
    
    SUBCASE("Usertypes: Registration & Lifecycle") {
        ks_script_begin_scope(ctx);
        {
            auto b = ks_script_usertype_begin(ctx, "Hero", sizeof(Hero));
            ks_script_usertype_add_constructor(b, KS_SCRIPT_FUNC_VOID(hero_new_void));
            ks_script_usertype_set_destructor(b, hero_delete);

            ks_script_usertype_add_method(b, "heal",
                KS_SCRIPT_FUNC(hero_heal, KS_TYPE_DOUBLE)
            );
            ks_script_usertype_add_property(b, "hp", hero_get_hp, hero_set_hp);

            ks_script_usertype_end(b);

            const char* script = R"(
                local h = Hero()
                h.hp = 50
                h:heal(25)
                return h.hp
            )";

            Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);
            CHECK(ks_script_call_succeded(ctx, res));

            Ks_Script_Object ret = ks_script_call_get_return(ctx, res);
            CHECK(ks_script_obj_as_number(ctx, ret) == 75.0);

        }
        ks_script_end_scope(ctx);
    }

    SUBCASE("Usertypes: Fields (Direct Access) & Nested Types") {
        ks_script_begin_scope(ctx); {
            auto b_vec = ks_script_usertype_begin(ctx, "Vec3", sizeof(Vec3));
            ks_script_usertype_add_constructor(b_vec, KS_SCRIPT_FUNC_VOID(vec3_new));

            ks_script_usertype_add_field(b_vec, "x", KS_TYPE_FLOAT, offsetof(Vec3, x), nullptr);
            ks_script_usertype_add_field(b_vec, "y", KS_TYPE_FLOAT, offsetof(Vec3, y), nullptr);
            ks_script_usertype_add_field(b_vec, "z", KS_TYPE_FLOAT, offsetof(Vec3, z), nullptr);
            ks_script_usertype_end(b_vec);

            auto b_trans = ks_script_usertype_begin(ctx, "Transform", sizeof(Transform));
            ks_script_usertype_add_constructor(b_trans, KS_SCRIPT_FUNC_VOID(transform_new));

            ks_script_usertype_add_field(b_trans, "id", KS_TYPE_INT, offsetof(Transform, id), nullptr);

            ks_script_usertype_add_field(b_trans, "position", KS_TYPE_USERDATA, offsetof(Transform, position), "Vec3");
            ks_script_usertype_add_field(b_trans, "scale", KS_TYPE_USERDATA, offsetof(Transform, scale), "Vec3");

            ks_script_usertype_end(b_trans);

            const char* script = R"(
                local t = Transform()
                
                t.id = 99
                
                t.position.x = 10.5
                t.position.y = -5.0
                t.position.z = 33.0

                local new_scale = Vec3()
                new_scale.x = 2.0
                new_scale.y = 2.0
                new_scale.z = 2.0
                
                t.scale = new_scale 

                return t.id, t.position.x, t.scale.y
            )";

            Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);

            if (!ks_script_call_succeded(ctx, res)) {
                FAIL(ks_script_get_last_error_str(ctx));
            }

            CHECK(ks_script_call_get_returns_count(ctx, res) == 3);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 1)) == 99.0);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 2)) == 10.5);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 3)) == 2.0);

        }
        ks_script_end_scope(ctx);
    }

    SUBCASE("Usertypes: Properties (Getters/Setters Logic)") {
        ks_script_begin_scope(ctx);
        {
            struct PropTest { int val; };

            auto get_double = [](Ks_Script_Ctx c) -> ks_returns_count {
                auto* self = (PropTest*)ks_script_get_self(c);
                ks_script_stack_push_number(c, self->val * 2.0);
                return 1;
            };

            auto set_clamped = [](Ks_Script_Ctx c) -> ks_returns_count {
                auto* self = (PropTest*)ks_script_get_self(c);
                int v = (int)ks_script_obj_as_number(c, ks_script_get_arg(c, 1));
                if (v < 0) v = 0;
                self->val = v;
                return 0;
            };

            auto b = ks_script_usertype_begin(ctx, "PropTest", sizeof(PropTest));
            ks_script_usertype_add_constructor(b, KS_SCRIPT_FUNC_VOID([](Ks_Script_Ctx c) { new(ks_script_get_self(c)) PropTest{ 10 }; return 0; }));
            ks_script_usertype_add_property(b, "value", get_double, set_clamped);
            ks_script_usertype_end(b);

            const char* script = R"(
                local p = PropTest()
                p.value = -50
                local v1 = p.value
                
                p.value = 5
                local v2 = p.value
                return v1, v2
            )";

            Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 1)) == 0.0);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 2)) == 10.0);
        }
        ks_script_end_scope(ctx);
    }

    SUBCASE("Usertypes: Inheritance & Overloading") {
        ks_script_begin_scope(ctx); {
            const char* T_ENT = "TestEntity";
            const char* T_HERO = "TestHero";

            auto b_ent = ks_script_usertype_begin(ctx, T_ENT, sizeof(Entity));
            ks_script_usertype_add_method(b_ent, "exist", KS_SCRIPT_FUNC_VOID(entity_exist));
            ks_script_usertype_add_property(b_ent, "id", entity_get_id, nullptr);
            ks_script_usertype_end(b_ent);

            auto b_hero = ks_script_usertype_begin(ctx, T_HERO, sizeof(Hero));
            ks_script_usertype_inherits_from(b_hero, T_ENT);

            ks_script_usertype_add_constructor(b_hero, KS_SCRIPT_OVERLOAD(
                KS_SCRIPT_SIG_DEF_VOID(hero_new_void), 
                KS_SCRIPT_SIG_DEF(hero_new_name, KS_TYPE_CSTRING), 
                KS_SCRIPT_SIG_DEF(hero_new_full, KS_TYPE_CSTRING, KS_TYPE_DOUBLE)
            ));
            ks_script_usertype_set_destructor(b_hero, hero_delete);


            ks_script_usertype_add_method(b_hero, "attack", KS_SCRIPT_OVERLOAD(
                KS_SCRIPT_SIG_DEF_VOID(hero_attack_basic),
                KS_SCRIPT_SIG_DEF(hero_attack_strong, KS_TYPE_DOUBLE)
            ));

            ks_script_usertype_add_property(b_hero, "hp", hero_get_hp, hero_set_hp);

            ks_script_usertype_end(b_hero);

            const char* script = R"(
                local h1 = TestHero()             
                local h2 = TestHero("Thrall")     
                local h3 = TestHero("Jaina", 200) 

                local id_val = h3.id 
                local dmg1 = h3:attack()       
                local dmg2 = h3:attack(50)     

                return h1.hp, h3.hp, id_val, dmg1, dmg2
            )";

            Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);

            if (!ks_script_call_succeded(ctx, res)) {
                FAIL(ks_script_get_last_error_str(ctx));
            }

            CHECK(ks_script_call_get_returns_count(ctx, res) == 5);

            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 1)) == 100.0);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 2)) == 200.0);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 3)) == 2.0);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 4)) == 10.0);
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return_at(ctx, res, 5)) == 100.0);

        } ks_script_end_scope(ctx);
    }

    SUBCASE("Coroutines: Yield & Resume") {
        ks_script_begin_scope(ctx); {

            const char* script = R"(
                function co_gen(start_val)
                    coroutine.yield(start_val * 2)                   
                    coroutine.yield(start_val * 3)                  
                    return "Done"
                end
            )";
            ks_script_do_cstring(ctx, script);

            Ks_Script_Object func_obj = ks_script_get_global(ctx, "co_gen");
            Ks_Script_Coroutine co = ks_script_create_coroutine(ctx, ks_script_obj_as_function(ctx, func_obj));

            CHECK(ks_script_obj_type(ctx, co) == KS_TYPE_SCRIPT_COROUTINE);

            CHECK(ks_script_coroutine_status(ctx, co) == KS_SCRIPT_COROUTINE_SUSPENDED);

            ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, 10));

            Ks_Script_Function_Call_Result res1 = ks_script_coroutine_resume(ctx, co, 1);

            CHECK(ks_script_call_succeded(ctx, res1));
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res1)) == 20.0);
            CHECK(ks_script_coroutine_status(ctx, co) == KS_SCRIPT_COROUTINE_SUSPENDED);

            Ks_Script_Function_Call_Result res2 = ks_script_coroutine_resume(ctx, co, 0);

            CHECK(ks_script_call_succeded(ctx, res2));
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res2)) == 30.0);
            CHECK(ks_script_coroutine_status(ctx, co) == KS_SCRIPT_COROUTINE_SUSPENDED);

  
            Ks_Script_Function_Call_Result res3 = ks_script_coroutine_resume(ctx, co, 0);

            CHECK(ks_script_call_succeded(ctx, res3));
            const char* str = ks_script_obj_as_cstring(ctx, ks_script_call_get_return(ctx, res3));
            CHECK(strcmp(str, "Done") == 0);

            CHECK(ks_script_coroutine_status(ctx, co) == KS_SCRIPT_COROUTINE_DEAD);

        } ks_script_end_scope(ctx);
    }

    SUBCASE("Error Reporting: Overload Mismatch with Typename") {
        ks_script_begin_scope(ctx); {
            struct Dummy { int x; };
            auto b1 = ks_script_usertype_begin(ctx, "DummyType", sizeof(Dummy));
            ks_script_usertype_add_constructor(b1, KS_SCRIPT_FUNC_VOID([](Ks_Script_Ctx c) {
                new(ks_script_get_self(c)) Dummy{ 0 }; return 0;
            }));
            ks_script_usertype_end(b1);

            struct Target { int v; };
            auto func_accept_int = [](Ks_Script_Ctx c) -> ks_returns_count { return 0; };

            auto b2 = ks_script_usertype_begin(ctx, "TargetType", sizeof(Target));
            ks_script_usertype_add_constructor(b2, KS_SCRIPT_FUNC_VOID([](Ks_Script_Ctx c) {
                new(ks_script_get_self(c)) Target{ 0 }; return 0;
            }));
            ks_script_usertype_add_method(b2, "process", KS_SCRIPT_FUNC(func_accept_int, KS_TYPE_INT));
            ks_script_usertype_end(b2);

            const char* script = R"(
                local d = DummyType()
                local t = TargetType()
                t:process(d)
            )";

            Ks_Script_Function_Call_Result res = ks_script_do_cstring(ctx, script);

            CHECK(ks_script_call_succeded(ctx, res) == ks_false);

            const char* err = ks_script_get_last_error_str(ctx);
            if (err) {
                KS_LOG_INFO("Expected Error: %s", err);
                CHECK(strstr(err, "[1] userdata (DummyType)") != nullptr);
                CHECK(strstr(err, "Candidate 1: (integer)") != nullptr);
            }
            else {
                CHECK_MESSAGE(false, "No error message returned!");
            }

        } ks_script_end_scope(ctx);
    }

    ks_script_destroy_ctx(ctx);
    ks_memory_shutdown();
}