#include <doctest/doctest.h>
#include <keystone.h>
#include <include/asset/assets_binding.h>
#include <string.h>

#include "../include/common.h"

TEST_CASE("Managers-Lua bindings Tests") {
	ks_memory_init();

	Ks_Script_Ctx ctx = ks_script_create_ctx();

	SUBCASE("Assets Manager Lua Bindings") {
		Ks_AssetsManager am = ks_assets_manager_create();

		Ks_IAsset interface;
		interface.load_from_file_fn = my_asset_load_file;
		interface.destroy_fn = my_asset_destroy;

        ks_assets_manager_register_asset_type(am, "MyAsset", interface);

        auto b = ks_script_usertype_begin(ctx, "MyAsset", sizeof(MyCAsset));
        ks_script_usertype_add_property(b, "id", my_asset_get_id, nullptr);
        ks_script_usertype_end(b);

        ks_assets_manager_lua_bind(ctx, am);

        const char* script = R"(
            local h = assets.load("MyAsset", "hero_tex", "path/to/tex.png")
        
            if assets.valid(h) == 0 then
                return "Invalid Handle"
            end

            local data = assets.get_data(h)
            
            local data = assets.get_data(h)
            print("Type of data:", type(data)) -- Aggiungi questa riga
            print("Value of data:", data)      -- Aggiungi questa riga
            print(data.id)

            print(data.id)
        
            local h_ref = assets.get("hero_tex")
        
            if h == h_ref then
                return "Success"
            else
                return "Handle Mismatch"
            end
        )";

        Ks_Script_Function_Call_Result res = ks_script_do_string(ctx, script);

        if (!ks_script_call_succeded(ctx, res)) {
            FAIL(ks_script_get_last_error_str(ctx));
        }

        const char* ret_str = ks_script_obj_as_str(ctx, ks_script_call_get_return(ctx, res));
        REQUIRE(ret_str != nullptr);
        CHECK(strcmp(ret_str, "Success") == 0);

		ks_assets_manager_destroy(am);
	}

	ks_script_destroy_ctx(ctx);

	ks_memory_shutdown();
}