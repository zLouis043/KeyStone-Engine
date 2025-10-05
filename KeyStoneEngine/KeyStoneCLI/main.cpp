#include <iostream>
#include <keystone.hpp>

struct MyAsset {
    int a = 5;
    std::string mammt = "mammt";

    bool f = true;
};

struct MyClass {
public:

    static Ks_AssetData load_from_file(const char* file_path) {

        MyClass* klass = ks::mem::alloc_t<MyClass>(
            ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::RESOURCE
        );

        return static_cast<Ks_AssetData>(klass);
    }

    static Ks_AssetData load_from_data(const uint8_t* data) {
        MyClass* klass = ks::mem::alloc_t<MyClass>(
            ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::RESOURCE
        );

        return static_cast<Ks_AssetData>(klass);
    }

    static void   destroy_asset(Ks_AssetData data) {
        MyClass* klass = static_cast<MyClass*>(data);

        klass->~MyClass();

        ks::mem::dealloc(klass);
    }

    int a = 5;
    std::string mammt = "mammt";

    bool f = true;
};


Ks_AssetData create_my_asset(const char* file_path) {
    MyAsset* asset = ks::mem::alloc_t<MyAsset>(ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::RESOURCE);
    return static_cast<Ks_AssetData>(asset);
}

void destroy_my_asset(Ks_AssetData data) {
    MyAsset* asset = reinterpret_cast<MyAsset*>(data);
    asset->~MyAsset();
    ks::mem::dealloc(asset);
}

int main(int argc, char** argv){

    ks_memory_init();

    ks::mem::alloc(5 * sizeof(int), ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::GARBAGE);
    int* ptr = ks::mem::alloc_t<int, 5>(ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::GARBAGE);

    ptr[0] = 5;

    ks::log::info("ptr = {}", ptr[0]);

    ks::mem::dealloc(ptr);

    ks::log::info("ptr = {}", ptr[0]);

    ks::script::ScriptManager sm;
    sm.init();

    sm.script(
        R"(print("Hello World from script"))"
    );

    sm.shutdown();

    ks::asset::AssetsManager am;

    am.register_type<MyClass>("MyClass");

    ks::asset::handle handle = am.load_asset_from_file<MyClass>("klass", "");

    MyClass* klass = am.get_asset_data<MyClass>(handle);

    ks::log::info("klass->a = {}", klass->a);
    ks::log::info("klass->f = {}", klass->f);
    ks::log::info("klass->mammt = {}", klass->mammt);

    am.asset_unload(handle);

    Ks_AssetsManager cam = ks_assets_manager_create();

    ks_assets_manager_register_asset_type(cam, "MyAsset", {
        .load_from_file_fn = create_my_asset,
        .asset_destroy_fn = destroy_my_asset
    });


    Ks_AssetHandle chandle = ks_assets_manager_load_asset_from_file(
        cam, "MyAsset", "mammt", nullptr
    );

    Ks_AssetData cdata = ks_assets_manager_get_data(cam, chandle);

    MyAsset* asset = reinterpret_cast<MyAsset*>(cdata);

    ks::log::info("asset.a = {}", asset->a);
    ks::log::info("asset.f = {}", asset->f);
    ks::log::info("asset.mammt = {}", asset->mammt);

    ks_assets_manager_asset_unload(cam, "mammt");

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