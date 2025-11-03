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

    Ks_Script_Object func = ks_script_create_cfunc(ctx, [](Ks_Script_Ctx ctx) -> int {

        Ks_Script_Object o1 = ks_script_stack_pop_obj(ctx);
        Ks_Script_Object o2 = ks_script_stack_pop_obj(ctx);

        ks_double res = ks_script_obj_as_number(o1) + ks_script_obj_as_number(o2);

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

    ks::log::info("res as number: {}", ks_script_obj_as_number(res));

    res = ks_script_do_string(ctx, "print(\"Hello from lua\")");
    
    Ks_Script_Table tbl = ks_script_create_named_table(ctx, "test");
    ks_script_table_set(ctx, tbl,
        ks_script_create_cstring(ctx, "a"),
        ks_script_create_number(ctx, 10)
    );

    ks_script_table_set(ctx, tbl,
        ks_script_create_cstring(ctx, "b"),
        ks_script_create_number(ctx, 20)
    );

    ks_script_table_set(ctx, tbl,
        ks_script_create_cstring(ctx, "c"),
        ks_script_create_number(ctx, 30)
    );

    ks_bool has = ks_script_table_has(
        ctx, tbl,
        ks_script_create_cstring(ctx, "a")
    );

    ks::log::info("test has a:= {}", has);

    if (has) {
        Ks_Script_Object obj = ks_script_table_get(
            ctx, tbl,
            ks_script_create_cstring(ctx, "a")
        );

        ks::log::info("test.a = {}", ks_script_obj_as_number(obj));
    }

    Ks_Script_Table_Iterator it = ks_script_table_iterate(ctx, tbl);

    Ks_Script_Object key, value;
    while (ks_script_iterator_next(ctx, &it, &key, &value)) {
        ks::log::debug("Key: {} -> Value: {}",
            ks_script_obj_as_str(key),
            ks_script_obj_as_number(value)
        );
    }

    ks_script_iterator_destroy(ctx, &it);

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

    /*
    if(argc < 2){
        LOG_FATAL("Project path was not given to the engine.\n\tUsage: ./keystone-cli <project_path>");
        return 1;
    }

    engine e;

    Result<void> res = e.init();

    if(!res.ok()){
        LOG_FATAL("Could not init the engine:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.set_engine_path(argv[0]);

    if(!res.ok()){
        LOG_FATAL("Could not set the engine path:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.set_project_path(argv[1]);

    if(!res.ok()){
        LOG_FATAL("Could not set the project path:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.load_configs();

    if(!res.ok()){
        LOG_FATAL("Could not load configs:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.load_systems();

    if(!res.ok()){
        LOG_FATAL("Could not load systems:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.load_project();
    
    if(!res.ok()){
        LOG_FATAL("Could not load project:\n\t{}",
            res.what());
        return 1;
    } 

    res = e.run();

    if(!res.ok()){
        LOG_FATAL("Could not run the project:\n\t{}", res.what());
        return 1;
    }  
    */

    return 0;
}
