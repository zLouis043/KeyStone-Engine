#include "../../include/asset/assets_binding.h"
#include "../../include/memory/memory.h"

#include <string.h>

struct AssetProxy {
    Ks_Handle handle;
    Ks_AssetsManager am;
};

struct BindContext {
    Ks_AssetsManager am;
    Ks_JobManager js;
};

static BindContext* get_bind_ctx(Ks_Script_Ctx ctx) {
    Ks_Script_Object upval = ks_script_func_get_upvalue(ctx, 1);
    return (BindContext*)ks_script_lightuserdata_get_ptr(ctx, upval);
}

static Ks_AssetsManager get_am_from_upvalue(Ks_Script_Ctx ctx) {
    Ks_Script_Object upval = ks_script_func_get_upvalue(ctx, 1);
    ks_ptr impl_ptr = ks_script_lightuserdata_get_ptr(ctx, upval);
    return (Ks_AssetsManager)impl_ptr;
}

ks_returns_count l_asset_proxy_index(Ks_Script_Ctx ctx) {
    Ks_Script_Object proxy_obj = ks_script_get_arg(ctx, 1);
    auto* proxy = (AssetProxy*)ks_script_usertype_get_ptr(ctx, proxy_obj);

    if (!proxy) return 0;

    Ks_Script_Object key = ks_script_get_arg(ctx, 2);

    void* ptr = ks_assets_manager_get_data(proxy->am, proxy->handle);
    if (!ptr) {
        ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
        return 1;
    }

    const char* type_name = ks_assets_manager_get_type_name(proxy->am, proxy->handle);

    Ks_Script_Object ref = ks_script_create_usertype_ref(ctx, type_name, ptr);

    Ks_Script_Object result = ks_script_table_get(ctx, ref, key);

    ks_script_stack_push_obj(ctx, result);
    return 1;
}

ks_returns_count l_asset_proxy_newindex(Ks_Script_Ctx ctx) {
    Ks_Script_Object proxy_obj = ks_script_get_arg(ctx, 1);
    auto* proxy = (AssetProxy*)ks_script_usertype_get_ptr(ctx, proxy_obj);
    if (!proxy) return 0;

    Ks_Script_Object key = ks_script_get_arg(ctx, 2);
    Ks_Script_Object val = ks_script_get_arg(ctx, 3);

    void* ptr = ks_assets_manager_get_data(proxy->am, proxy->handle);
    if (!ptr) return 0;

    const char* type_name = ks_assets_manager_get_type_name(proxy->am, proxy->handle);

    Ks_Script_Object ref = ks_script_create_usertype_ref(ctx, type_name, ptr);

    ks_script_table_set(ctx, ref, key, val);

    return 0;
}

ks_returns_count l_asset_proxy_eq(Ks_Script_Ctx ctx) {
    Ks_Script_Object obj1 = ks_script_get_arg(ctx, 1);
    Ks_Script_Object obj2 = ks_script_get_arg(ctx, 2);

    if (obj1.type != KS_TYPE_USERDATA || obj2.type != KS_TYPE_USERDATA) {
        ks_script_stack_push_boolean(ctx, false);
        return 1;
    }

    auto* p1 = (AssetProxy*)ks_script_usertype_get_ptr(ctx, obj1);
    auto* p2 = (AssetProxy*)ks_script_usertype_get_ptr(ctx, obj2);

    bool are_equal = (p1 && p2) &&
        (p1->handle == p2->handle);

    ks_script_stack_push_boolean(ctx, are_equal);
    return 1;
}

static Ks_Handle extract_handle(Ks_Script_Ctx ctx, int arg_idx) {
    Ks_Script_Object obj = ks_script_get_arg(ctx, arg_idx);

    if (obj.type == KS_TYPE_USERDATA) {
        auto* proxy = (AssetProxy*)ks_script_usertype_get_ptr(ctx, obj);
        if (proxy) return proxy->handle;
    }
    else if (obj.type == KS_TYPE_INT) {
        return (Ks_Handle)ks_script_obj_as_integer(ctx, obj);
    }
    else if (obj.type == KS_TYPE_DOUBLE) {
        return (Ks_Handle)ks_script_obj_as_number(ctx, obj);
    }

    return KS_INVALID_HANDLE;
}

ks_returns_count l_assets_load(Ks_Script_Ctx ctx) {
    BindContext* bctx = get_bind_ctx(ctx);

    const char* type = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 2));
    Ks_Script_Object arg3 = ks_script_get_arg(ctx, 3);

    Ks_Handle handle = KS_INVALID_HANDLE;

    Ks_Type t = ks_script_obj_type(ctx, arg3);
    if (t == KS_TYPE_CSTRING) {
        const char* path = ks_script_obj_as_cstring(ctx, arg3);
        handle = ks_assets_manager_load_asset_from_file(bctx->am, type, name, path);
    }
    else {
        // TODO: HERE SHOULD GO THE OVERLOAD TO LOAD DATA FROM RAW/USERDATA
        // handle = ks_assets_manager_load_asset_from_data(...)
    }

    Ks_Script_Userdata ud = ks_script_create_usertype_instance(ctx, "AssetHandle");

    auto* proxy = (AssetProxy*)ks_script_usertype_get_ptr(ctx, ud);
    if (proxy) {
        proxy->handle = handle;
        proxy->am = bctx->am;
    }

    ks_script_stack_push_obj(ctx, ud);
    return 1;
}

ks_returns_count l_assets_load_async(Ks_Script_Ctx ctx) {
    BindContext* bctx = get_bind_ctx(ctx);
    if (!bctx || !bctx->js) return 0;

    const char* type = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 2));
    const char* path = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 3));

    Ks_Handle handle = ks_assets_manager_load_async(bctx->am, type, name, path, bctx->js);

    Ks_Script_Userdata ud = ks_script_create_usertype_instance(ctx, "AssetHandle");
    auto* proxy = (AssetProxy*)ks_script_usertype_get_ptr(ctx, ud);
    if (proxy) {
        proxy->handle = handle;
        proxy->am = bctx->am;
    }
    ks_script_stack_push_obj(ctx, ud);
    return 1;
}

ks_returns_count l_assets_state(Ks_Script_Ctx ctx) {
    BindContext* bctx = get_bind_ctx(ctx);

    Ks_Script_Object arg = ks_script_get_arg(ctx, 1);
    Ks_Handle handle = KS_INVALID_HANDLE;

    if (arg.type == KS_TYPE_USERDATA) {
        auto* proxy = (AssetProxy*)ks_script_usertype_get_ptr(ctx, arg);
        if (proxy) handle = proxy->handle;
    }
    else {
        handle = (Ks_Handle)ks_script_obj_as_integer(ctx, arg);
    }

    int state = (int)ks_assets_get_state(bctx->am, handle);

    const char* s_str = "none";
    switch (state) {
    case KS_ASSET_STATE_LOADING: s_str = "loading"; break;
    case KS_ASSET_STATE_READY: s_str = "ready"; break;
    case KS_ASSET_STATE_FAILED: s_str = "failed"; break;
    }
    ks_script_stack_push_cstring(ctx, s_str);
    return 1;
}

ks_returns_count l_assets_valid(Ks_Script_Ctx ctx) {
    BindContext* bctx = get_bind_ctx(ctx);

    Ks_Handle handle = extract_handle(ctx, 1);

    ks_bool is_valid = ks_assets_is_handle_valid(bctx->am, handle);

    ks_script_stack_push_obj(ctx, ks_script_create_integer(ctx, is_valid ? 1 : 0));
    return 1;
}

ks_returns_count l_assets_get(Ks_Script_Ctx ctx) {
    BindContext* bctx = get_bind_ctx(ctx);

    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    Ks_Handle handle = ks_assets_manager_get_asset(bctx->am, name);

    Ks_Script_Userdata ud = ks_script_create_usertype_instance(ctx, "AssetHandle");

    auto* proxy = (AssetProxy*)ks_script_usertype_get_ptr(ctx, ud);
    if (proxy) {
        proxy->handle = handle;
        proxy->am = bctx->am;
    }

    ks_script_stack_push_obj(ctx, ud);
    return 1;
}

ks_returns_count l_assets_get_data(Ks_Script_Ctx ctx) {
    BindContext* bctx = get_bind_ctx(ctx);

    Ks_Handle handle = extract_handle(ctx, 1);

    void* ptr = ks_assets_manager_get_data(bctx->am, handle);

    if (!ptr) {
        ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
        return 1;
    }

    const char* type_name = ks_assets_manager_get_type_name(bctx->am, handle);

    if (!type_name) {
        ks_script_stack_push_obj(ctx, ks_script_create_lightuserdata(ctx, ptr));
        return 1;
    }

    Ks_Script_Object obj = ks_script_create_usertype_ref(ctx, type_name, ptr);

    ks_script_stack_push_obj(ctx, obj);
    return 1;
}

KS_API ks_no_ret ks_assets_manager_lua_bind(Ks_Script_Ctx ctx, Ks_AssetsManager am, Ks_JobManager js) {

    BindContext* bctx = (BindContext*)ks_alloc(sizeof(BindContext), KS_LT_PERMANENT, KS_TAG_SCRIPT);
    bctx->am = am;
    bctx->js = js;

    Ks_Script_Object upval = ks_script_create_lightuserdata(ctx, bctx);

    auto b = ks_script_usertype_begin(ctx, "AssetHandle", sizeof(AssetProxy));
    ks_script_usertype_add_metamethod(b, KS_SCRIPT_MT_INDEX, KS_SCRIPT_FUNC(
        l_asset_proxy_index, KS_TYPE_USERDATA, KS_TYPE_CSTRING)
    );
    ks_script_usertype_add_metamethod(b, KS_SCRIPT_MT_NEWINDEX, KS_SCRIPT_FUNC(
        l_asset_proxy_newindex, KS_TYPE_USERDATA, KS_TYPE_CSTRING, KS_TYPE_SCRIPT_ANY)
    );
    ks_script_usertype_add_metamethod(b, KS_SCRIPT_MT_EQ, KS_SCRIPT_FUNC(
        l_asset_proxy_eq, KS_TYPE_USERDATA, KS_TYPE_USERDATA)
    );
    ks_script_usertype_end(b);

    Ks_Script_Table assets_tbl = ks_script_create_named_table(ctx, "assets");

    auto reg_func = [&](const char* name, ks_script_cfunc func) {
        ks_script_stack_push_obj(ctx, upval);
        Ks_Script_Function fn_obj = ks_script_create_cfunc_with_upvalues(ctx, KS_SCRIPT_FUNC_VOID(func), 1);
        ks_script_table_set(ctx, assets_tbl, ks_script_create_cstring(ctx, name), fn_obj);
    };

    reg_func("load", l_assets_load);
    reg_func("load_async", l_assets_load_async);
    reg_func("state", l_assets_state);
    reg_func("valid", l_assets_valid);
    reg_func("get", l_assets_get);
    reg_func("get_data", l_assets_get_data);
}