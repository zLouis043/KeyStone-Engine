#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>
#include <string>
#include <new>

struct Entity {
    int id;
    Entity(int _id) : id(_id) {}
    virtual ~Entity() {}
    void exist() {}
};

class Hero : public Entity {
public:
    std::string name;
    int hp;
    Hero(const std::string& n, int h) : name(n), hp(h), Entity(2) {}
    void heal(int amount) { hp += amount; }
    int attack() { return 10; }
    int attack_strong(int dmg) { return dmg * 2; }
};

ks_returns_count entity_exist(Ks_Script_Ctx ctx) {
    Entity* self = (Entity*)ks_script_get_self(ctx);
    if (self) self->exist();
    return 0;
}

ks_returns_count entity_get_id(Ks_Script_Ctx ctx) {
    Entity* self = (Entity*)ks_script_get_self(ctx);
    ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, self ? self->id : 0));
    return 1;
}

ks_returns_count hero_new_void(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    new(self) Hero("Unknown", 100);
    return 0;
}

ks_returns_count hero_new_name(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    const char* name = ks_script_obj_as_str(ctx, ks_script_get_arg(ctx, 1));
    new(self) Hero(name ? name : "Unknown", 100);
    return 0;
}

ks_returns_count hero_new_full(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    const char* name = ks_script_obj_as_str(ctx, ks_script_get_arg(ctx, 1));
    int hp = (int)ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 2));
    new(self) Hero(name ? name : "Unknown", hp);
    return 0;
}

void hero_delete(ks_ptr data, ks_size size) {
    static_cast<Hero*>(data)->~Hero();
}

ks_returns_count hero_heal(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    Ks_Script_Object amt = ks_script_get_arg(ctx, 1);
    self->heal((int)ks_script_obj_as_number(ctx, amt));
    return 0;
}

ks_returns_count hero_get_hp(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, self->hp));
    return 1;
}

ks_returns_count hero_set_hp(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    Ks_Script_Object val = ks_script_get_arg(ctx, 1);
    if (self) self->hp = (int)ks_script_obj_as_number(ctx, val);
    return 0;
}

ks_returns_count hero_attack_basic(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    int dmg = self->attack();
    ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, dmg));
    return 1;
}

ks_returns_count hero_attack_strong(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    int input_dmg = (int)ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    int dmg = self->attack_strong(input_dmg);
    ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, dmg));
    return 1;
}

void register_test_types(Ks_Script_Ctx ctx) {
}

TEST_CASE("C API: Script Engine Suite") {
    ks_memory_init();
    Ks_Script_Ctx ctx = ks_script_create_ctx();

    REQUIRE(ctx != nullptr);

    SUBCASE("Basics: Primitives & Execution") {
        ks_script_begin_scope(ctx); {

            Ks_Script_Function_Call_Result res = ks_script_do_string(ctx, "return 42");
            CHECK(ks_script_call_succeded(ctx, res));
            CHECK(ks_script_obj_as_number(ctx, ks_script_call_get_return(ctx, res)) == 42.0);

            Ks_Script_Object num = ks_script_create_number(ctx, 123.456);
            CHECK(ks_script_obj_type(ctx, num) == KS_SCRIPT_OBJECT_TYPE_NUMBER);
            CHECK(ks_script_obj_as_number(ctx, num) == 123.456);

            Ks_Script_Object str = ks_script_create_cstring(ctx, "KeyStone");
            CHECK(ks_script_obj_type(ctx, str) == KS_SCRIPT_OBJECT_TYPE_STRING);
            CHECK(strcmp(ks_script_obj_as_str(ctx, str), "KeyStone") == 0);

            Ks_Script_Object boolean = ks_script_create_boolean(ctx, ks_true);
            CHECK(ks_script_obj_type(ctx, boolean) == KS_SCRIPT_OBJECT_TYPE_BOOLEAN);
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
        CHECK(ks_script_obj_type(ctx, promoted_obj) == KS_SCRIPT_OBJECT_TYPE_TABLE);

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

            Ks_Script_Function func_obj = ks_script_create_cfunc(ctx, add_func);
            CHECK(ks_script_obj_type(ctx, func_obj) == KS_SCRIPT_OBJECT_TYPE_FUNCTION);

            Ks_Script_Function_Call_Result res_c = ks_script_func_callv(ctx, func_obj,
                ks_script_create_number(ctx, 10),
                ks_script_create_number(ctx, 20)
            );

            CHECK(ks_script_call_succeded(ctx, res_c));

            Ks_Script_Object ret_val = ks_script_call_get_return(ctx, res_c);
            CHECK(ks_script_obj_as_number(ctx, ret_val) == 30.0);

            ks_script_set_global(ctx, "my_add", func_obj);

            const char* script = "return my_add(5, 7)";
            Ks_Script_Function_Call_Result res_lua = ks_script_do_string(ctx, script);

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

            Ks_Script_Function closure = ks_script_create_cfunc_with_upvalues(ctx, counter_func, 1);

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
            CHECK(ks_script_obj_type(ctx, tbl) == KS_SCRIPT_OBJECT_TYPE_TABLE);

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
            CHECK(ks_script_obj_type(ctx, got_mt) == KS_SCRIPT_OBJECT_TYPE_TABLE);

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
            CHECK(ks_script_obj_type(ctx, lud) == KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA);

            void* got_ptr = ks_script_lightuserdata_get_ptr(ctx, lud);
            CHECK(got_ptr == ptr);

            struct MyData { int x; float y; };
            Ks_Script_Userdata ud = ks_script_create_userdata(ctx, sizeof(MyData));
            CHECK(ks_script_obj_type(ctx, ud) == KS_SCRIPT_OBJECT_TYPE_USERDATA);

            MyData* data_ptr = (MyData*)ks_script_userdata_get_ptr(ctx, ud);
            REQUIRE(data_ptr != nullptr);
            data_ptr->x = 100;
            data_ptr->y = 3.14f;

            MyData* read_ptr = (MyData*)ks_script_userdata_get_ptr(ctx, ud);
            CHECK(read_ptr->x == 100);
            CHECK(read_ptr->y == 3.14f);

        } ks_script_end_scope(ctx);
    }
    
    SUBCASE("Usertypes: Registration & Lifecycle") {
        ks_script_begin_scope(ctx);
        {
            auto b = ks_script_usertype_begin(ctx, "Hero", sizeof(Hero));
            ks_script_usertype_add_constructor(b, hero_new_void);
            ks_script_usertype_set_destructor(b, hero_delete);

            ks_script_usertype_add_method(b, "heal", hero_heal);
            ks_script_usertype_add_property(b, "hp", hero_get_hp, hero_set_hp);

            ks_script_usertype_end(b);

            const char* script = R"(
                local h = Hero()
                h.hp = 50
                h:heal(25)
                return h.hp
            )";

            Ks_Script_Function_Call_Result res = ks_script_do_string(ctx, script);
            CHECK(ks_script_call_succeded(ctx, res));

            Ks_Script_Object ret = ks_script_call_get_return(ctx, res);
            CHECK(ks_script_obj_as_number(ctx, ret) == 75.0);

        }
        ks_script_end_scope(ctx);
    }

    SUBCASE("Usertypes: Inheritance & Overloading") {
        ks_script_begin_scope(ctx); {
            const char* T_ENT = "TestEntity";
            const char* T_HERO = "TestHero";

            auto b_ent = ks_script_usertype_begin(ctx, T_ENT, sizeof(Entity));
            ks_script_usertype_add_method(b_ent, "exist", entity_exist);
            ks_script_usertype_add_property(b_ent, "id", entity_get_id, nullptr);
            ks_script_usertype_end(b_ent);

            auto b_hero = ks_script_usertype_begin(ctx, T_HERO, sizeof(Hero));
            ks_script_usertype_inherits_from(b_hero, T_ENT);

            Ks_Script_Object_Type args_name[] = { KS_SCRIPT_OBJECT_TYPE_STRING };
            Ks_Script_Object_Type args_full[] = { KS_SCRIPT_OBJECT_TYPE_STRING, KS_SCRIPT_OBJECT_TYPE_NUMBER };
            ks_script_usertype_add_constructor(b_hero, hero_new_void);
            ks_script_usertype_add_constructor_overload(b_hero, hero_new_name, args_name, 1);
            ks_script_usertype_add_constructor_overload(b_hero, hero_new_full, args_full, 2);
            ks_script_usertype_set_destructor(b_hero, hero_delete);

            Ks_Script_Object_Type args_atk[] = { KS_SCRIPT_OBJECT_TYPE_NUMBER };
            ks_script_usertype_add_overload(b_hero, "attack", hero_attack_basic, nullptr, 0);
            ks_script_usertype_add_overload(b_hero, "attack", hero_attack_strong, args_atk, 1);

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

            Ks_Script_Function_Call_Result res = ks_script_do_string(ctx, script);

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
            ks_script_do_string(ctx, script);

            Ks_Script_Object func_obj = ks_script_get_global(ctx, "co_gen");
            Ks_Script_Coroutine co = ks_script_create_coroutine(ctx, ks_script_obj_as_function(ctx, func_obj));

            CHECK(ks_script_obj_type(ctx, co) == KS_SCRIPT_OBJECT_TYPE_COROUTINE);

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
            const char* str = ks_script_obj_as_str(ctx, ks_script_call_get_return(ctx, res3));
            CHECK(strcmp(str, "Done") == 0);

            CHECK(ks_script_coroutine_status(ctx, co) == KS_SCRIPT_COROUTINE_DEAD);

        } ks_script_end_scope(ctx);
    }


    ks_script_destroy_ctx(ctx);
    ks_memory_shutdown();
}