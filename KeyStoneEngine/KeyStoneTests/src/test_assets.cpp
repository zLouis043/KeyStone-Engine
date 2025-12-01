#include <doctest/doctest.h>
#include <keystone.h>
#include <string.h>

#include "../include/common.h"

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

    ks_memory_shutdown();
}