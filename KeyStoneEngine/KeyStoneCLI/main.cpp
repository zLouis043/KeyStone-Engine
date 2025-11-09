#include <iostream>
#include <keystone.hpp>

struct MyAsset {
    int a = 5;
    std::string string = "string";

    bool f = true;
};

struct MyClass : public ks::asset::Asset<MyClass> {
public:
    bool load_impl(const char* file_path) {
        string = "loaded from file";
        return true;
    }

    bool load_impl(const uint8_t* data) {
        string = "loaded from data";
        return true;
    }

    int a = 5;
    std::string string = "string";

    bool f = true;
};

Ks_AssetData create_my_asset(const char* file_path) {
    MyAsset* asset = ks::mem::alloc_t<MyAsset>(ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::RESOURCE);
    return static_cast<Ks_AssetData>(asset);
}

void destroy_my_asset(Ks_AssetData data) {
    MyAsset* asset = static_cast<MyAsset*>(data);
    asset->~MyAsset();
    ks::mem::dealloc(asset);
}

struct Entity {
    int id;
    Entity(int _id) : id(_id) { std::cout << "[C++] Entity constructed\n"; }
    virtual ~Entity() { std::cout << "[C++] Entity destroyed\n"; }
    void exist() { std::cout << "[C++] Entity " << id << " exists.\n"; }
};

class Hero : public Entity {
public:
    std::string name;
    int hp;
    Hero(const std::string& n, int h) : name(n), hp(h), Entity(2) { 
        std::cout << "[C++] Hero '" << name << "' constructed\n"; 
    }
    ~Hero() { 
        std::cout << "[C++] Hero '" << name << "' destroyed\n"; 
    }
    void heal(int amount) { 
        hp += amount; 
        std::cout << "[C++] " << name << " healed by " << amount << ". HP: " << hp << "\n"; 
    }
    void attack() {
        std::cout << "[C++] " << name << " performs a basic attack!\n";
    }
    void attack(int damage) {
        std::cout << "[C++] " << name << " performs a STRONG attack dealing " << damage << " damage!\n";
    }
};

ks_returns_count entity_exist(Ks_Script_Ctx ctx) {
    Entity* self = (Entity*)ks_script_get_self(ctx);
    if (self) self->exist();
    return 0;
}

ks_returns_count entity_get_id(Ks_Script_Ctx ctx) {
    Entity* self = (Entity*)ks_script_get_self(ctx);
    if (self) ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, self->id));
    else ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
    return 1;
}

void register_entity(Ks_Script_Ctx ctx) {
    auto b = ks_script_usertype_begin(ctx, "Entity", sizeof(Entity));
    ks_script_usertype_add_method(b, "exist", entity_exist);
    ks_script_usertype_add_property(b, "id", entity_get_id, nullptr);
    ks_script_usertype_end(b);
}

ks_returns_count hero_new(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    new(self) Hero("Unknown", 420);
    return 0;
}

ks_returns_count hero_new_1(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);

    Ks_Script_Object name_arg = ks_script_get_arg(ctx, 1);
    const char* name = ks_script_obj_as_str(ctx, name_arg);

    new(self) Hero(name ? name : "Unknown", 69);

    return 0;
}

ks_returns_count hero_new_2(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);

    Ks_Script_Object name_arg = ks_script_get_arg(ctx, 1);
    const char* name = ks_script_obj_as_str(ctx, name_arg);

    Ks_Script_Object hp_arg = ks_script_get_arg(ctx, 2);
    int hp = ks_script_obj_as_number(ctx, hp_arg);

    new(self) Hero(name ? name : "Unknown", hp);

    return 0;
}

void hero_delete(ks_ptr data, ks_size size) {
    static_cast<Hero*>(data)->~Hero();
}

ks_returns_count hero_heal(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    Ks_Script_Object amt = ks_script_get_arg(ctx, 1);
    if (self) self->heal((int)ks_script_obj_as_number(ctx, amt));
    return 0;
}

ks_returns_count hero_get_hp(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    if (self) {
        ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, self->hp));
    } else {
        ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
    }
    return 1;
}

ks_returns_count hero_set_hp(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    Ks_Script_Object val = ks_script_get_arg(ctx, 1);
    if (self) self->hp = (int)ks_script_obj_as_number(ctx, val);
    return 0;
}

ks_returns_count hero_attack_default(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    if (self) self->attack();
    return 0;
}

ks_returns_count hero_attack_strong(Ks_Script_Ctx ctx) {
    Hero* self = (Hero*)ks_script_get_self(ctx);
    Ks_Script_Object dmg = ks_script_get_arg(ctx, 1); 
    if (self) self->attack((int)ks_script_obj_as_number(ctx, dmg));
    return 0;
}

void register_hero_type(Ks_Script_Ctx ctx) {
    auto b = ks_script_usertype_begin(ctx, "Hero", sizeof(Hero));
    ks_script_usertype_inherits_from(b, "Entity");

    Ks_Script_Object_Type args[2] = {KS_SCRIPT_OBJECT_TYPE_STRING, KS_SCRIPT_OBJECT_TYPE_NUMBER};

    ks_script_usertype_add_constructor(b, hero_new);
    ks_script_usertype_add_constructor_overload(b, hero_new_1, args, 1);
    ks_script_usertype_add_constructor_overload(b, hero_new_2, args, 2);
    ks_script_usertype_set_destructor(b, hero_delete);
    ks_script_usertype_add_method(b, "heal", hero_heal);
    ks_script_usertype_add_property(b, "hp", hero_get_hp, hero_set_hp);

    Ks_Script_Object_Type attack_strong_args[] = { KS_SCRIPT_OBJECT_TYPE_NUMBER };

    ks_script_usertype_add_overload(b, "attack", hero_attack_default, NULL, 0);
    ks_script_usertype_add_overload(b, "attack", hero_attack_strong, attack_strong_args, 1);

    ks_script_usertype_end(b);
}

int main(int argc, char** argv){
    ks_memory_init();

    int* pptr= (int*)ks::mem::alloc(5 * sizeof(int), ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::GARBAGE);
    int* ptr = ks::mem::alloc_t<int, 5>(ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::GARBAGE);

    pptr[2] = 60;
    ptr[0] = 5;
    
    ks::log::info("pptr[2] = {}", pptr[2]);
    ks::log::info("ptr[0] = {}", ptr[0]);
    
    ks::mem::dealloc(pptr);
    ks::mem::dealloc(ptr);

    ks::log::info("pptr[2] = {}", pptr[2]);
    ks::log::info("ptr[0] = {}", ptr[0]);

    Ks_Script_Ctx ctx = ks_script_create_ctx();

    ks::log::info("script_ctx = {}", static_cast<void*>(ctx));

    Ks_Script_Object func = ks_script_create_cfunc(ctx, [](Ks_Script_Ctx ctx) -> ks_returns_count {

        Ks_Script_Object o1 = ks_script_get_arg(ctx, 1);
        Ks_Script_Object o2 = ks_script_get_arg(ctx, 2);

        ks_double res = ks_script_obj_as_number(ctx, o1) + ks_script_obj_as_number(ctx, o2);

        ks::log::trace("Hello From CFunc in lua");
        ks::log::trace("Should be 10 + 20:= {}", res);

        Ks_Script_Object o3 = ks_script_create_number(ctx, res);
        ks_script_stack_push_obj(ctx, o3);

        return 1;
    });

    Ks_Script_Function_Call_Result res = ks_script_func_callv(ctx, func,
        ks_script_create_number(ctx, 10),
        ks_script_create_number(ctx, 20)
    );

    ks::log::info("res as number: {}", ks_script_obj_as_number(ctx, res));

    res = ks_script_do_string(ctx, "print(\"Hello from lua\")");
    
    Ks_Script_Table tbl = ks_script_create_named_table(ctx, "test");

    ks_script_begin_scope(ctx);

    ks_script_table_set(ctx, tbl, 
        ks_script_create_cstring(ctx, "a"), ks_script_create_number(ctx, 10));

    ks_script_table_set(ctx, tbl, 
        ks_script_create_cstring(ctx, "b"), ks_script_create_number(ctx, 20));

    ks_script_table_set(ctx, tbl, 
        ks_script_create_cstring(ctx, "c"), ks_script_create_number(ctx, 30));

    ks_str k = "a";
    ks_bool has = ks_script_table_has(ctx, tbl, ks_script_create_cstring(ctx, k));

    ks::log::info("test has a:= {}", has);

    if (has) {
        Ks_Script_Object obj = ks_script_table_get(ctx, tbl, ks_script_create_cstring(ctx, k));

        ks::log::info("test.{} = {}", k, ks_script_obj_as_number(ctx, obj));
    }

    ks_script_end_scope(ctx);

    Ks_Script_Table_Iterator it = ks_script_table_iterate(ctx, tbl);

    ks_script_begin_scope(ctx);

    Ks_Script_Object key, val;
    while (ks_script_iterator_next(ctx, &it, &key, &val)) {
        ks::log::debug("Key: {} -> Value: {}",
            ks_script_obj_as_str(ctx, key),
            ks_script_obj_as_number(ctx, val)
        );
    }

    ks_script_end_scope(ctx);

    ks_script_iterator_destroy(ctx, &it);

    register_entity(ctx);
    register_hero_type(ctx);

    ks_str test_script = R"(
        print("--- Lua Start ---")
        
        -- 1. Test Costruttore
        local h = Hero()
        local h2 = Hero("Viktor")
        local h1 = Hero("Arthur", 100)
        print("Hero created via Lua. Type:", type(h1))
    
        print("h.hp = ", h.hp)
        print("h2.hp = ", h2.hp)    
    
        -- 2. Test Property Get
        print("Initial HP:", h1.hp)

        -- 3. Test Method Call
        h1:heal(50)

        -- 4. Test Property Set & Get
        h1.hp = 10
        print("HP after direct set:", h1.hp)
        
        print("1. Testing attack() with NO arguments:")
        h1:attack() -- Dovrebbe chiamare hero_attack_default

        print("\n2. Testing attack(1000) with ONE NUMBER argument:")
        h1:attack(9999) -- Dovrebbe chiamare hero_attack_strong
    
        h1:exist()

        print(h1.id)

        -- 5. Test Garbage Collection
        print("Releasing hero to GC...")
        h1 = nil
        collectgarbage() -- Forza il GC per vedere il distruttore C++
        
        print("--- Lua End ---")
    )";

    Ks_Script_Function_Call_Result script_res = ks_script_do_string(ctx, test_script);

    if (!ks_script_call_succeded(ctx, script_res)) {
        Ks_Script_Error err = ks_script_get_last_error(ctx);
        if (err != KS_SCRIPT_ERROR_NONE) {
            ks::log::error("Lua Error: {}", ks_script_get_last_error_str(ctx));
        }
    }

    ks::log::info("Lua mem used {} kb", ks_script_get_mem_used(ctx));

    ks_script_destroy_ctx(ctx);

    ks::asset::AssetsManager am;

    am.register_asset_type<MyClass>("MyClass");

    ks::asset::handle handle = am.load_asset<MyClass>("klass", "");

    if (handle == ks::asset::invalid_handle) {
        ks::log::error("Invalid handle error for asset 'klass'");
        return 1;
    }

    uint32_t ref_count = am.get_asset_ref_count(handle);

    ks::log::info("ref_count = {}", ref_count);

    MyClass* klass = am.get_asset_data<MyClass>(handle);

    if (klass == ks::asset::invalid_data) {
        ks::log::error("Invalid data error for asset 'klass'");
        return 1;
    }
    
    ks::log::info("klass->a = {}", klass->a);
    ks::log::info("klass->f = {}", klass->f);
    ks::log::info("klass->string = {}", klass->string);

    ks::asset::handle h2 = am.get_asset("klass");

    ks::log::info("handle = {}, h2 = {}", handle, h2);

    ref_count = am.get_asset_ref_count(h2);

    ks::log::info("ref_count = {}", ref_count);

    am.asset_release(h2);

    am.asset_release(handle);

    Ks_AssetsManager cam = ks_assets_manager_create();

    Ks_IAsset my_asset;
    my_asset.load_from_file_fn = create_my_asset;
    my_asset.destroy_fn = destroy_my_asset;

    ks_assets_manager_register_asset_type(cam, "MyAsset", my_asset);

    Ks_AssetHandle chandle = ks_assets_manager_load_asset_from_file(
        cam, "MyAsset", "asset", nullptr
    );

    Ks_AssetData cdata = ks_assets_manager_get_data(cam, chandle);

    MyAsset* asset = reinterpret_cast<MyAsset*>(cdata);
    ks::log::info("asset.a = {}", asset->a);
    ks::log::info("asset.f = {}", asset->f);
    ks::log::info("asset.string = {}", asset->string);

    ks_assets_manager_asset_release(cam, chandle);

    ks_assets_manager_destroy(cam);

    ks_memory_shutdown();
    return 0;
}
