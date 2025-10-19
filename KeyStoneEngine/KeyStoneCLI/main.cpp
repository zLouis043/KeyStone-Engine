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

    ks::script::ScriptManager sm;
    sm.init();

    sm.script(
        R"(print("Hello World from script"))"
    );

    sm.shutdown();

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

    ks_assets_manager_register_asset_type(cam, "MyAsset", (Ks_IAsset){
        .load_from_file_fn = create_my_asset,
        .destroy_fn = destroy_my_asset
    });

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
