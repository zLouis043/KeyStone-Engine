#pragma once

#include <keystone.h>
#include <string.h>
#include <string>
#include <vector>
#include <new>
#include <cstddef>

struct Vec3 { float x, y, z; };

struct Transform {
    Vec3 position;
    Vec3 scale;
    int id;
};

typedef struct {
    int id;
    float value;
    char name[128];
} MyCAsset;

inline Ks_AssetData my_asset_load_file(ks_str file_path) {
    MyCAsset* asset = (MyCAsset*)ks_alloc(sizeof(MyCAsset), KS_LT_USER_MANAGED, KS_TAG_RESOURCE);
    if (!asset) return KS_INVALID_ASSET_DATA;
    asset->id = 100;
    asset->value = 3.14f;
    if (file_path) {
        strncpy(asset->name, file_path, 127);
        asset->name[127] = '\0';
    }
    else {
        strcpy(asset->name, "Unknown");
    }
    return (Ks_AssetData)asset;
}

inline ks_no_ret my_asset_destroy(Ks_AssetData data) {
    ks_dealloc((void*)data);
}

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

inline ks_returns_count vec3_new(Ks_Script_Ctx ctx) {
    Vec3* self = (Vec3*)ks_script_get_self(ctx);
    self->x = 0; self->y = 0; self->z = 0;
    return 0;
}

inline ks_returns_count transform_new(Ks_Script_Ctx ctx) {
    Transform* self = (Transform*)ks_script_get_self(ctx);
    self->id = 0;
    self->position = { 0, 0, 0 };
    self->scale = { 1, 1, 1 };
    return 0;
}

inline ks_returns_count entity_get_id(Ks_Script_Ctx ctx) {
    Entity* self = (Entity*)ks_script_get_self(ctx);
    ks_script_stack_push_integer(ctx, self ? self->id : 0);
    return 1;
}

inline ks_returns_count entity_exist(Ks_Script_Ctx ctx) {
    Entity* self = (Entity*)ks_script_get_self(ctx);
    if (self) self->exist();
    return 0;
}

inline ks_returns_count hero_new_void(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    new(self) Hero("Unknown", 100);
    return 0;
}

inline ks_returns_count hero_new_name(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    new(self) Hero(name ? name : "Unknown", 100);
    return 0;
}

inline ks_returns_count hero_new_full(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    int hp = (int)ks_script_obj_as_integer(ctx, ks_script_get_arg(ctx, 2));
    new(self) Hero(name ? name : "Unknown", hp);
    return 0;
}

inline void hero_delete(ks_ptr data, ks_size size) {
    static_cast<Hero*>(data)->~Hero();
}

inline ks_returns_count hero_get_hp(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    ks_script_stack_push_integer(ctx, self->hp);
    return 1;
}

inline ks_returns_count hero_set_hp(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    int val = (int)ks_script_obj_as_integer(ctx, ks_script_get_arg(ctx, 1));
    if (val < 0) val = 0;
    self->hp = val;
    return 0;
}

inline ks_returns_count hero_heal(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    Ks_Script_Object amt = ks_script_get_arg(ctx, 1);
    self->heal((int)ks_script_obj_as_integer(ctx, amt));
    return 0;
}

inline ks_returns_count hero_attack_basic(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    int dmg = self->attack();
    ks_script_stack_push_integer(ctx, dmg);
    return 1;
}

inline ks_returns_count hero_attack_strong(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    int input_dmg = (int)ks_script_obj_as_integer(ctx, ks_script_get_arg(ctx, 1));
    int dmg = self->attack_strong(input_dmg);
    ks_script_stack_push_integer(ctx, dmg);
    return 1;
}

struct TestEntity {
    int id;
    bool active;
};

struct TestHero {
    int x, y;
    int hp;
    bool is_active;
    float mana;
};

inline void TestHero_Init(TestHero* self, int x, int y) {
    self->x = x; self->y = y; self->hp = 100; self->is_active = true; self->mana = 50.0f;
}
inline void TestHero_Move(TestHero* self, int dx, int dy) {
    self->x += dx; self->y += dy;
}
inline int TestHero_GetHP(TestHero* self) { return self->hp; }
inline void TestHero_SetActive(TestHero* self, bool active) { self->is_active = active; }
inline int TestHero_GetTotalHeroes() { return 999; }

inline int __tostring_TestHero(Ks_Script_Ctx ctx) {
    TestHero* self = (TestHero*)ks_script_get_self(ctx);
    ks_script_stack_push_cstring(ctx, "Hero");
    return 1;
}

struct OverloadTester {
    int last_result;
};

inline void Ov_Reset(OverloadTester* self) {
    self->last_result = 0;
}

inline void Ov_Set(OverloadTester* self, int val) {
    self->last_result = val;
}

inline void Ov_Add(OverloadTester* self, int a, int b) {
    self->last_result = a + b;
}

inline void Ov_Hash(OverloadTester* self, const char* str) {
    self->last_result = (int)strlen(str);
}

inline void OverloadTester_Init(OverloadTester* self) {
    self->last_result = -1;
}

inline void RegisterCommonReflection() {
    ks_reflection_shutdown();
    ks_reflection_init();

    ks_reflect_struct(Vec3,
        ks_reflect_field(float, x),
        ks_reflect_field(float, y),
        ks_reflect_field(float, z)
    );

    ks_reflect_struct(TestEntity,
        ks_reflect_field(int, id),
        ks_reflect_field(bool, active)
    );

    ks_reflect_struct(TestHero,
        ks_reflect_field(int, x), ks_reflect_field(int, y),
        ks_reflect_field(int, hp), ks_reflect_field(bool, is_active),
        ks_reflect_field(float, mana),

        ks_reflect_vtable_begin(TestHero),
        ks_reflect_constructor(TestHero_Init, ks_args(ks_arg(int, x), ks_arg(int, y))),
        ks_reflect_method(TestHero_Move, void, ks_args(ks_arg(int, dx), ks_arg(int, dy))),
        ks_reflect_method(TestHero_GetHP, int, ks_no_args()),
        ks_reflect_method(TestHero_SetActive, void, ks_args(ks_arg(bool, active))),
        ks_reflect_static_method(__tostring_TestHero, int, ks_args(ks_arg(Ks_Script_Ctx, ctx))),
        ks_reflect_static_method(TestHero_GetTotalHeroes, int, ks_no_args()),
        ks_reflect_vtable_end()
    );

    ks_reflect_struct(OverloadTester,
        ks_reflect_field(int, last_result),

        ks_reflect_vtable_begin(OverloadTester),
        ks_reflect_constructor(OverloadTester_Init, ks_no_args()),

        ks_reflect_method_named("exec", Ov_Reset, void, ks_no_args()),
        ks_reflect_method_named("exec", Ov_Set, void, ks_args(ks_arg(int, val))),
        ks_reflect_method_named("exec", Ov_Add, void, ks_args(ks_arg(int, a), ks_arg(int, b))),
        ks_reflect_method_named("exec", Ov_Hash, void, ks_args(ks_arg(const char*, s))),

        ks_reflect_vtable_end()
    );
}