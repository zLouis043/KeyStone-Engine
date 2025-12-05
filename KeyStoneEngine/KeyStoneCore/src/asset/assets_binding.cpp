#include "../../include/asset/assets_binding.h"

#include <string.h>

static Ks_AssetsManager get_am_from_upvalue(Ks_Script_Ctx ctx) {
    Ks_Script_Object upval = ks_script_func_get_upvalue(ctx, 1);
    void* impl_ptr = ks_script_lightuserdata_get_ptr(ctx, upval);
    Ks_AssetsManager am;
    am.impl = impl_ptr;
    return am;
}

ks_returns_count l_assets_load(Ks_Script_Ctx ctx) {
    Ks_AssetsManager am = get_am_from_upvalue(ctx);

    const char* type = ks_script_obj_as_str(ctx, ks_script_get_arg(ctx, 1));
    const char* name = ks_script_obj_as_str(ctx, ks_script_get_arg(ctx, 2));
    Ks_Script_Object arg3 = ks_script_get_arg(ctx, 3);

    Ks_Handle handle = KS_INVALID_HANDLE;

    if (ks_script_obj_type(ctx, arg3) == KS_SCRIPT_OBJECT_TYPE_STRING) {
        const char* path = ks_script_obj_as_str(ctx, arg3);
        handle = ks_assets_manager_load_asset_from_file(am, type, name, path);
    }
    else {
        // Qui andrebbe l'overload per caricare da dati raw/userdata
        // handle = ks_assets_manager_load_asset_from_data(...)
    }

    ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, (double)handle));
    return 1;
}

ks_returns_count l_assets_valid(Ks_Script_Ctx ctx) {
    Ks_AssetsManager am = get_am_from_upvalue(ctx);

    double h_val = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    Ks_Handle handle = (Ks_Handle)h_val;

    ks_bool is_valid = ks_assets_is_handle_valid(am, handle);

    ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, is_valid ? 1 : 0));
    return 1;
}

ks_returns_count l_assets_get(Ks_Script_Ctx ctx) {
    Ks_AssetsManager am = get_am_from_upvalue(ctx);

    const char* name = ks_script_obj_as_str(ctx, ks_script_get_arg(ctx, 1));
    Ks_Handle handle = ks_assets_manager_get_asset(am, name);

    ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, (double)handle));
    return 1;
}

ks_returns_count l_assets_get_data(Ks_Script_Ctx ctx) {
    Ks_AssetsManager am = get_am_from_upvalue(ctx);

    double h_val = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    Ks_Handle handle = (Ks_Handle)h_val;

    void* ptr = ks_assets_manager_get_data(am, handle);

    if (!ptr) {
        ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
        return 1;
    }

    const char* type_name = ks_assets_manager_get_type_name(am, handle);

    if (!type_name) {
        ks_script_stack_push_obj(ctx, ks_script_create_lightuserdata(ctx, ptr));
        return 1;
    }

    Ks_Script_Object obj = ks_script_create_usertype_ref(ctx, type_name, ptr);

    ks_script_stack_push_obj(ctx, obj);
    return 1;
}

KS_API ks_no_ret ks_assets_manager_lua_bind(Ks_Script_Ctx ctx, Ks_AssetsManager am) {
    Ks_Script_Object am_upval = ks_script_create_lightuserdata(ctx, am.impl);

    Ks_Script_Table assets_tbl = ks_script_create_named_table(ctx, "assets");

    auto reg_func = [&](const char* name, ks_script_cfunc func) {
        ks_script_stack_push_obj(ctx, am_upval);
        Ks_Script_Function fn_obj = ks_script_create_cfunc_with_upvalues(ctx, KS_SCRIPT_FUNC_VOID(func), 1);
        ks_script_table_set(ctx, assets_tbl, ks_script_create_cstring(ctx, name), fn_obj);
    };

    reg_func("load", l_assets_load);
    reg_func("valid", l_assets_valid);
    reg_func("get", l_assets_get);
    reg_func("get_data", l_assets_get_data);
}