#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>
#include <fstream>
#include <thread> 
#include <chrono>

#include "../include/common.h"

void write_test_file(const char* path, const char* content) {
    std::ofstream out(path);
    out << content;
    out.close();
}

struct TextAsset {
    char content[64];
};

Ks_AssetData text_load_file(ks_str file_path) {
    TextAsset* asset = (TextAsset*)ks_alloc(sizeof(TextAsset), KS_LT_USER_MANAGED, KS_TAG_RESOURCE);
    memset(asset, 0, sizeof(TextAsset));

    std::ifstream in(file_path);
    if (in.is_open()) {
        in.read(asset->content, 63);
    }
    else {
        strcpy(asset->content, "ERROR");
    }
    return (Ks_AssetData)asset;
}

void text_destroy(Ks_AssetData data) {
    ks_dealloc(data);
}

TEST_CASE("C API: Assets Manager") {
    ks_memory_init();

    SUBCASE("Full Lifecycle") {
        Ks_AssetsManager am = ks_assets_manager_create();

        Ks_IAsset interface;
        interface.load_from_file_fn = my_asset_load_file;
        interface.load_from_data_fn = my_asset_load_data;
        interface.destroy_fn = my_asset_destroy;

        ks_assets_manager_register_asset_type(am, "MyCAsset", interface);

        const char* fake_path = "textures/player.png";
        Ks_Handle handle = ks_assets_manager_load_asset_from_file(am, "MyCAsset", "player_tex", fake_path);

        REQUIRE(handle != 0); 
        CHECK(ks_assets_is_handle_valid(am, handle) == ks_true);

        MyCAsset* asset_ptr = (MyCAsset*)ks_assets_manager_get_data(am, handle);
        REQUIRE(asset_ptr != nullptr);
        CHECK(asset_ptr->id == 100);
        CHECK(strcmp(asset_ptr->name, fake_path) == 0);

        Ks_Handle h2 = ks_assets_manager_get_asset(am, "player_tex");
        CHECK(h2 == handle);
        CHECK(ks_assets_manager_get_ref_count(am, handle) == 2);

        ks_assets_manager_asset_release(am, h2);
        CHECK(ks_assets_manager_get_ref_count(am, handle) == 1);

        ks_assets_manager_asset_release(am, handle);
        CHECK(ks_assets_is_handle_valid(am, handle) == ks_false);

        ks_assets_manager_destroy(am);
    }

    SUBCASE("Hot Reloading System") {
        Ks_AssetsManager am = ks_assets_manager_create();

        Ks_IAsset interface;
        interface.load_from_file_fn = text_load_file;
        interface.load_from_data_fn = nullptr;
        interface.destroy_fn = text_destroy;
        ks_assets_manager_register_asset_type(am, "TextAsset", interface);

        const char* test_path = "hot_reload_test.txt";
        write_test_file(test_path, "Version 1");

        Ks_Handle handle = ks_assets_manager_load_asset_from_file(am, "TextAsset", "test_doc", test_path);
        REQUIRE(handle != KS_INVALID_HANDLE);

        TextAsset* data = (TextAsset*)ks_assets_manager_get_data(am, handle);
        CHECK(std::string(data->content) == "Version 1");

        std::this_thread::sleep_for(std::chrono::milliseconds(1100));

        write_test_file(test_path, "Version 2 - RELOADED");

        bool reloaded = false;
        for (int i = 0; i < 10; i++) {
            ks_assets_manager_update(am);

            data = (TextAsset*)ks_assets_manager_get_data(am, handle);
            if (std::string(data->content) == "Version 2 - RELOADED") {
                reloaded = true;
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        CHECK(reloaded == true);
        CHECK(std::string(data->content) == "Version 2 - RELOADED");

        ks_assets_manager_destroy(am);
        std::remove(test_path);
    }

    ks_memory_shutdown();
}