#pragma once

#include <keystone.h>
#include <string.h>
#include <string>
#include <cstddef>

typedef struct {
    int id;
    float value;
    char name[32];
} MyCAsset;

inline Ks_AssetData my_asset_load_file(ks_str file_path) {
    MyCAsset* asset = (MyCAsset*)ks_alloc(sizeof(MyCAsset), KS_LT_USER_MANAGED, KS_TAG_RESOURCE);
    if (!asset) return KS_INVALID_ASSET_DATA;

    asset->id = 100;
    asset->value = 3.14f;
    if (file_path) {
        strncpy(asset->name, file_path, 31);
        asset->name[31] = '\0';
    }
    else {
        strcpy(asset->name, "Unknown");
    }

    return (Ks_AssetData)asset;
}

inline Ks_AssetData my_asset_load_data(const Ks_UserData data) {
    MyCAsset* asset = (MyCAsset*)ks_alloc(sizeof(MyCAsset), KS_LT_USER_MANAGED, KS_TAG_RESOURCE);
    if (!asset) return KS_INVALID_ASSET_DATA;
    asset->id = 200;
    return (Ks_AssetData)asset;
}

inline ks_no_ret my_asset_destroy(Ks_AssetData data) {
    MyCAsset* asset = (MyCAsset*)data;
    ks_dealloc(asset);
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

struct Vec3 {
    float x, y, z;
};

struct Transform {
    Vec3 position;
    Vec3 scale;
    int id;
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