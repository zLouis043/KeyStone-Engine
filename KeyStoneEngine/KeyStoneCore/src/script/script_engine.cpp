#include "../../include/script/script_engine.h"
#include "../../include/script/script_engine_internal.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"

#ifdef __cplusplus
extern "C" {
#endif
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
#ifdef __cplusplus
}
#endif

#include <vector>
#include <stdarg.h>

static void* lua_custom_Alloc(void* ud, void* ptr, size_t osize, size_t nsize);
static Ks_Script_Object_Type lua_type_to_ks(int type);

Ks_Script_Ctx ks_script_create_ctx() {
	KsScriptEngineCtx* ctx = static_cast<KsScriptEngineCtx*>(ks_alloc_debug(
		sizeof(*ctx),
		KS_LT_PERMANENT,
		KS_TAG_INTERNAL_DATA,
		"KsScriptEngineCtx"
	));

	lua_State* state = lua_newstate(lua_custom_Alloc, nullptr);

    luaL_openlibs(state);

    new(ctx) KsScriptEngineCtx(state);
    
    return static_cast<Ks_Script_Ctx>(ctx);
}

KS_API ks_no_ret ks_script_destroy_ctx(Ks_Script_Ctx ctx)
{
    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    sctx->~KsScriptEngineCtx();
    ks_dealloc(sctx);
}

KS_API Ks_Script_Object ks_script_create_number(Ks_Script_Ctx ctx, ks_double val)
{
    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_NUMBER;
    obj.val.number = val;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_false;
    return obj;
}

KS_API Ks_Script_Object ks_script_create_boolean(Ks_Script_Ctx ctx, ks_bool val)
{
    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_BOOLEAN;
    obj.val.boolean = val;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_false;
    return obj;
}

KS_API Ks_Script_Object ks_script_create_nil(Ks_Script_Ctx ctx)
{
    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_NIL;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_false;
    std::memset(&obj.val, 0, sizeof(obj.val));
    return obj;
}

KS_API Ks_Script_Object ks_script_create_invalid_obj(Ks_Script_Ctx ctx)
{
    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_NIL;
    obj.state = KS_SCRIPT_OBJECT_INVALID;
    obj.requires_free = ks_false;
    std::memset(&obj.val, 0, sizeof(obj.val));
    return obj;
}

typedef struct ks_script_cfunc_wrapper {
    ks_cfunc user_func;
    Ks_Script_Ctx ctx;
} ks_script_cfunc_wrapper;

static int lua_cfunc_bridge(lua_State* L) {
    ks_script_cfunc_wrapper* wrapper = static_cast<ks_script_cfunc_wrapper*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    if (!wrapper || !wrapper->user_func) {
        luaL_error(L, "Invalid C function wrapper");
        return 0;
    }

    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(wrapper->ctx);

    try {
        ks_returns_count result_count = wrapper->user_func(wrapper->ctx);
        return result_count;
    }
    catch (const std::exception& e) {
        luaL_error(L, "Error in C function: %s", e.what());
        return 0;
    }
    catch (...) {
        luaL_error(L, "Unknown error in C function");
        return 0;
    }
}

KS_API Ks_Script_Function ks_script_create_cfunc(Ks_Script_Ctx ctx, ks_cfunc f)
{
    if (!ctx || !f) return ks_script_create_invalid_obj(ctx);

    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_cfunc_wrapper* wrapper = static_cast<ks_script_cfunc_wrapper*>(
        lua_newuserdata(L, sizeof(ks_script_cfunc_wrapper)));

    wrapper->user_func = f;
    wrapper->ctx = ctx;

    lua_pushcclosure(L, lua_cfunc_bridge, 1);

    Ks_Script_Function obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_false;
    obj.val.function = sctx->store_in_registry();

    return obj;
}

KS_API Ks_Script_Function ks_script_create_cfunc_with_upvalues(Ks_Script_Ctx ctx, ks_cfunc f, ks_size n_upvalues)
{
    if (!ctx || !f) return ks_script_create_invalid_obj(ctx);

    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_cfunc_wrapper* wrapper = static_cast<ks_script_cfunc_wrapper*>(
        lua_newuserdata(L, sizeof(ks_script_cfunc_wrapper)));

    wrapper->user_func = f;
    wrapper->ctx = ctx;

    int total_upvalues = n_upvalues + 1;

    lua_pushcclosure(L, lua_cfunc_bridge, total_upvalues);

    Ks_Script_Function obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_false;
    obj.val.function = sctx->store_in_registry();

    return obj;
}

KS_API Ks_Script_Table ks_script_create_table(Ks_Script_Ctx ctx)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_newtable(L);
    Ks_Script_Ref ref = sctx->store_in_registry();

    Ks_Script_Table obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_TABLE;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_false;
    obj.val.table = ref;

    return obj;


}

KS_API Ks_Script_Table ks_script_create_table_with_capacity(Ks_Script_Ctx ctx, ks_size array_sz, ks_size hash_sz)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_createtable(L, array_sz, hash_sz);
    Ks_Script_Ref ref = sctx->store_in_registry();

    Ks_Script_Table obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_TABLE;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_false;
    obj.val.table = ref;

    return obj;
}

KS_API Ks_Script_Table ks_script_create_named_table(Ks_Script_Ctx ctx, ks_str name)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);;
    Ks_Script_Table tbl = ks_script_create_table(ctx);
    ks_script_set_global(ctx, name, tbl);
    return tbl;
}

KS_API Ks_Script_Object ks_script_create_cstring(Ks_Script_Ctx ctx, ks_str val)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_string(ctx, val);

    Ks_Script_Table obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_STRING;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.string.data = lua_tostring(L, -1);
    obj.val.string.len = strlen(val);
    obj.requires_free = ks_false;

    lua_pop(L, 1);

    return obj;
}

KS_API Ks_Script_Object ks_script_create_cstring_dup(Ks_Script_Ctx ctx, ks_str val) {
    if (!ctx || !val) return ks_script_create_invalid_obj(ctx);

    size_t len = strlen(val);
    char* user_copy = (char*)ks_alloc_debug(
        len + 1,
        KS_LT_USER_MANAGED,
        KS_TAG_SCRIPT,
        "ScriptStringDup"
    );
    strcpy(user_copy, val);

    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_STRING;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_true;
    obj.val.string.data = user_copy;
    obj.val.string.len = len;

    return obj;
}

KS_API Ks_Script_Object ks_script_create_lstring(Ks_Script_Ctx ctx, ks_str str, ks_size len)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_string(ctx, str);

    Ks_Script_Table obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_STRING;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.string.data = lua_tostring(L, -1);;
    obj.val.string.len = len;
    obj.requires_free = ks_false;

    lua_tostring(L, -1);

    return obj;
}

KS_API Ks_Script_Object ks_script_create_lstring_dup(Ks_Script_Ctx ctx, ks_str val, ks_size len) {
    if (!ctx || !val) return ks_script_create_invalid_obj(ctx);

    char* user_copy = (char*)ks_alloc_debug(
        len + 1,
        KS_LT_USER_MANAGED,
        KS_TAG_SCRIPT,
        "ScriptStringDup"
    );
    strcpy(user_copy, val);

    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_STRING;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.requires_free = ks_true;
    obj.val.string.data = user_copy;
    obj.val.string.len = len;

    return obj;
}

Ks_Script_Function_Call_Result ks_script_func_callv_impl(Ks_Script_Ctx ctx, Ks_Script_Function f, ...)
{
    if (!ctx || f.type != KS_SCRIPT_OBJECT_TYPE_FUNCTION) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    std::vector<Ks_Script_Object> args;
    ks_va_list vargs;
    va_start(vargs, f);

    while (true) {
        Ks_Script_Object arg = va_arg(vargs, Ks_Script_Object);
        if (arg.state == KS_SCRIPT_OBJECT_INVALID) break;
        args.push_back(arg);
    }
    va_end(vargs);

    sctx->get_from_registry(f.val.function);

    for (const auto& arg : args) {
        ks_script_stack_push_obj(ctx, arg);
    }

    ks_int top_before = lua_gettop(L) - args.size() - 1;

    if (lua_pcall(L, args.size(), LUA_MULTRET, 0) != LUA_OK) {
        ks_str err = const_cast<ks_str>(lua_tostring(L, -1));
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME,
            err ? err : "Function call failed");
        lua_pop(L, 1);
        return ks_script_create_invalid_obj(ctx);
    }

    ks_int n_results = lua_gettop(L) - top_before;
    
    if (n_results == 0) {
        return ks_script_create_nil(ctx);
    }
    else if (n_results == 1) {
        return ks_script_stack_pop_obj(ctx);
    }
    else {
        Ks_Script_Table result_table = ks_script_create_table(ctx);

        for (int i = n_results; i >= 1; i--) {
            Ks_Script_Object val = ks_script_stack_pop_obj(ctx);
            Ks_Script_Object key = ks_script_create_number(ctx, i);
            ks_script_table_set(ctx, result_table, key, val);
        }

        return result_table;
    }
}

KS_API Ks_Script_Object ks_script_func_get_upvalue(Ks_Script_Ctx ctx, ks_upvalue_idx i)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_getupvalue(L, -1, i);

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return ks_script_create_invalid_obj(ctx);
    }

    return ks_script_stack_pop_obj(ctx);
}

KS_API ks_bool ks_script_call_succeded(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res)
{
    return res.state = KS_SCRIPT_OBJECT_VALID;
}

KS_API Ks_Script_Object ks_script_call_get_return(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res)
{
    return static_cast<Ks_Script_Object>(res);
}

KS_API ks_size ks_script_call_get_returns_count(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res)
{
    if (ks_script_obj_is(res, KS_SCRIPT_OBJECT_TYPE_TABLE)) {
        return ks_script_table_array_size(ctx, res);
    }

    return res.state == KS_SCRIPT_OBJECT_VALID ? 1 : 0;
}

KS_API Ks_Script_Object ks_script_call_get_return_at(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res, ks_size idx)
{
    if (ks_script_obj_is(res, KS_SCRIPT_OBJECT_TYPE_TABLE)) {
        return ks_script_table_get(ctx, res, ks_script_create_number(ctx, idx));
    }

    if (idx == 0 && res.state == KS_SCRIPT_OBJECT_VALID) {
        return static_cast<Ks_Script_Object>(res);
    }

    return ks_script_create_invalid_obj(ctx);
}

KS_API ks_no_ret ks_script_stack_push_number(Ks_Script_Ctx ctx, ks_double val)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushnumber(L, val);
}

KS_API ks_no_ret ks_script_stack_push_boolean(Ks_Script_Ctx ctx, ks_bool val)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushboolean(L, val);
}

KS_API ks_no_ret ks_script_stack_push_string(Ks_Script_Ctx ctx, ks_str val)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushstring(L, val);
}

KS_API ks_no_ret ks_script_stack_push_obj(Ks_Script_Ctx ctx, Ks_Script_Object val)
{

    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    switch (val.type) {
    case KS_SCRIPT_OBJECT_TYPE_UNKNOWN: {
        lua_pushnil(L);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_NIL: {
        lua_pushnil(L);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_STRING: {
        lua_pushstring(L, val.val.string.data);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_NUMBER: {
        lua_pushnumber(L, val.val.number);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_BOOLEAN: {
        lua_pushboolean(L, val.val.boolean);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_TABLE: {
        sctx->get_from_registry(val.val.table);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_FUNCTION: {
        sctx->get_from_registry(val.val.function);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_COROUTINE: {
        sctx->get_from_registry(val.val.coroutine);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_USERDATA: {} break;
    case KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA: {} break;
    }
}

KS_API ks_double ks_script_stack_pop_number(Ks_Script_Ctx ctx)
{
    if (!ctx) return 0.0;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (!lua_isnumber(L, -1)) return 0.0;
    ks_double val = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return val;
}

KS_API ks_bool ks_script_stack_pop_boolean(Ks_Script_Ctx ctx)
{
    if (!ctx) return ks_false;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (!lua_isboolean(L, -1)) return ks_false;
    ks_bool val = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return val;
}

KS_API ks_str ks_script_stack_pop_string(Ks_Script_Ctx ctx)
{
    if (!ctx) return nullptr;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (!lua_isstring(L, -1)) return nullptr;
    ks_str val = lua_tostring(L, -1);
    lua_pop(L, 1);
    return val;
}

static void* lua_custom_Alloc(void* ud, void* ptr, size_t osize, size_t nsize) {
    if (nsize == 0) {
        if (ptr) {
            ks_dealloc(ptr);
        }
        return nullptr;
    }
    else if (ptr == nullptr) {
        return ks_alloc_debug(nsize, KS_LT_USER_MANAGED, KS_TAG_SCRIPT, "LuaData");
    }

    return ks_realloc(ptr, nsize);
}

KS_API ks_no_ret ks_script_set_global(Ks_Script_Ctx ctx, ks_str name, Ks_Script_Object val)
{
    if (!ctx || !name) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_obj(ctx, val);
    lua_setglobal(L, name);
}

KS_API Ks_Script_Object ks_script_get_global(Ks_Script_Ctx ctx, ks_str name)
{
    if (!ctx || !name) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_getglobal(L, name);
    return ks_script_stack_pop_obj(ctx);
}

KS_API Ks_Script_Function ks_script_load_string(Ks_Script_Ctx ctx, ks_str string)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (luaL_loadstring(L, string) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Failed to load string");
        lua_pop(L, 1);
        return ks_script_create_invalid_obj(ctx);
    }

    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.function = sctx->store_in_registry();
    return obj;
}

KS_API Ks_Script_Function ks_script_load_file(Ks_Script_Ctx ctx, ks_str file_path)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (luaL_loadfile(L, file_path) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Failed to load file");
        lua_pop(L, 1);
        return ks_script_create_invalid_obj(ctx);
    }

    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.function = sctx->store_in_registry();
    return obj;
}

KS_API Ks_Script_Function_Call_Result ks_script_do_string(Ks_Script_Ctx ctx, ks_str string)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (luaL_dostring(L, string) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Failed to execute string");
        lua_pop(L, 1);
        return ks_script_create_invalid_obj(ctx);
    }

    if (lua_gettop(L) > 0) {
        return ks_script_stack_pop_obj(ctx);
    }

    return ks_script_create_nil(ctx);
}

KS_API Ks_Script_Function_Call_Result ks_script_do_file(Ks_Script_Ctx ctx, ks_str file_path)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (luaL_dofile(L, file_path) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Failed to execute file");
        lua_pop(L, 1);
        return ks_script_create_invalid_obj(ctx);
    }

    if (lua_gettop(L) > 0) {
        return ks_script_stack_pop_obj(ctx);
    }

    return ks_script_create_nil(ctx);
}

KS_API Ks_Script_Object ks_script_require(Ks_Script_Ctx ctx, ks_str module_name)
{
    if (!ctx || !module_name) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    Ks_Script_Object require = ks_script_get_global(ctx, "require");
    ks_script_stack_push_obj(ctx, require);
    ks_script_stack_push_string(ctx, module_name);

    if (lua_pcall(L, 1 ,1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Failed to require module");
        lua_pop(L, 1);
        return ks_script_create_invalid_obj(ctx);
    }

    return ks_script_stack_pop_obj(ctx);
}

KS_API ks_no_ret ks_script_register_module(Ks_Script_Ctx ctx, ks_str name, Ks_Script_Table module)
{
    if (!ctx || !name) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    Ks_Script_Table package = ks_script_get_global(ctx, "package");
    Ks_Script_Object loaded = ks_script_table_get(ctx, package, ks_script_create_cstring(ctx, "loaded"));
    ks_script_table_set(ctx, loaded, ks_script_create_cstring(ctx, name), module);
}

KS_API ks_no_ret ks_script_add_package_path(Ks_Script_Ctx ctx, ks_str path)
{
    if (!ctx || !path) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    Ks_Script_Table package = ks_script_get_global(ctx, "package");

    std::string current_path = lua_tostring(L, -1);
    current_path += ";";
    current_path += path;

    ks_script_stack_pop_obj(ctx);

    Ks_Script_Object package_path = ks_script_create_cstring(ctx, "path");
    Ks_Script_Object curr_path = ks_script_create_lstring(ctx, current_path.data(), current_path.length());
    ks_script_table_set(ctx, package, package_path, curr_path);

}

KS_API ks_no_ret ks_script_gc_collect(Ks_Script_Ctx ctx)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_gc(L, LUA_GCCOLLECT, 0);
}

KS_API ks_no_ret ks_script_gc_stop(Ks_Script_Ctx ctx)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_gc(L, LUA_GCSTOP, 0);
}

KS_API ks_no_ret ks_script_gc_restart(Ks_Script_Ctx ctx)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_gc(L, LUA_GCRESTART, 0);
}

KS_API ks_size ks_script_get_mem_used(Ks_Script_Ctx ctx)
{
    if (!ctx) return -1;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    return lua_gc(L, LUA_GCCOUNT, 0) * 1024;
}

KS_API ks_no_ret ks_script_dump_registry(Ks_Script_Ctx ctx)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    KS_LOG_DEBUG("=== LUA REGISTRY DUMP ===\n");
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        KS_LOG_DEBUG("Key: ");
        if (lua_type(L, -2) == LUA_TNIL) {
            KS_LOG_DEBUG("(nil)");
        }
        if (lua_type(L, -2) == LUA_TNUMBER) {
            KS_LOG_DEBUG("%d", (int)lua_tointeger(L, -2));
        }
        else if (lua_type(L, -2) == LUA_TBOOLEAN) {
            KS_LOG_DEBUG("%s", lua_toboolean(L, -2) ? "True" : "False");
        }
        else if (lua_type(L, -2) == LUA_TSTRING) {
            KS_LOG_DEBUG("%s", lua_tostring(L, -2));
        }
        else {
            KS_LOG_WARN("TODO: REST OF THE TYPES FOR dump_registry");
        }
        KS_LOG_DEBUG(" -> Value type: %s\n", lua_typename(L, lua_type(L, -1)));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    KS_LOG_DEBUG("========================\n");
}

KS_API Ks_Script_Object ks_script_stack_pop_obj(Ks_Script_Ctx ctx)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (lua_gettop(L) == 0) {
        return ks_script_create_invalid_obj(ctx);
    }

    Ks_Script_Object obj = ks_script_stack_peek(ctx, -1);
    lua_pop(L, 1);
    return obj;
}

KS_API Ks_Script_Object ks_script_stack_get_top(Ks_Script_Ctx ctx)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (lua_gettop(L) == 0) return ks_script_create_invalid_obj(ctx);
    return ks_script_stack_peek(ctx, -1);
}

KS_API Ks_Script_Object ks_script_stack_peek(Ks_Script_Ctx ctx, ks_stack_idx i)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    int type = lua_type(L, i);

    switch (type) {
    case LUA_TNIL:
        return ks_script_create_nil(ctx);
    case LUA_TNUMBER:
        return ks_script_create_number(ctx, lua_tonumber(L, i));
    case LUA_TBOOLEAN:
        return ks_script_create_boolean(ctx, lua_toboolean(L, i));
    case LUA_TSTRING:
        return ks_script_create_cstring(ctx, lua_tostring(L, i));
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TTHREAD: {
        Ks_Script_Object obj;
        obj.type = lua_type_to_ks(type);
        int abs_index = lua_absindex(L, i);
        lua_pushvalue(L, abs_index);
        obj.val.table = sctx->store_in_registry();
        lua_pop(L, 1);
        return obj;
    }

    default:
        return ks_script_create_invalid_obj(ctx);
    }

    return ks_script_create_invalid_obj(ctx);
}

KS_API Ks_Script_Object ks_script_stack_get(Ks_Script_Ctx ctx, ks_stack_idx i)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    Ks_Script_Object obj = ks_script_stack_peek(ctx, i);
}

KS_API ks_size ks_script_stack_size(Ks_Script_Ctx ctx)
{
    if (!ctx) return 0;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    return lua_gettop(L);
}

KS_API ks_no_ret ks_script_stack_clear(Ks_Script_Ctx ctx)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_settop(L, 0);
}

KS_API ks_no_ret ks_script_stack_remove(Ks_Script_Ctx ctx, ks_stack_idx i)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_remove(L, i);
}

KS_API ks_no_ret ks_script_stack_insert(Ks_Script_Ctx ctx, ks_stack_idx i)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_insert(L, i);
}

KS_API ks_no_ret ks_script_stack_replace(Ks_Script_Ctx ctx, ks_stack_idx i)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_replace(L, i);
}

KS_API ks_no_ret ks_script_stack_copy(Ks_Script_Ctx ctx, ks_stack_idx from, ks_stack_idx to)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushvalue(L, from);
    lua_replace(L, to);
}

KS_API ks_no_ret ks_script_stack_dump(Ks_Script_Ctx ctx)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    int top = lua_gettop(L);
    KS_LOG_TRACE("=== STACK (size=%d) ===\n", top);

    for (int i = 1; i <= top; i++) {
        KS_LOG_TRACE("[%d] %s: ", i, lua_typename(L, lua_type(L, i)));

        switch (lua_type(L, i)) {
        case LUA_TNUMBER:
            KS_LOG_TRACE("%f\n", lua_tonumber(L, i));
            break;
        case LUA_TSTRING:
            KS_LOG_TRACE("%s\n", lua_tostring(L, i));
            break;
        case LUA_TBOOLEAN:
            KS_LOG_TRACE("%s\n", lua_toboolean(L, i) ? "true" : "false");
        case LUA_TNIL:
            KS_LOG_TRACE("nil\n");
            break;
        default:
            KS_LOG_TRACE("%p\n", lua_topointer(L, i));
            break;
        }
    }

    KS_LOG_TRACE("======================\n");
}

KS_API Ks_Script_Object_Type ks_script_obj_type(Ks_Script_Object obj)
{
    return obj.type;
}

KS_API ks_bool ks_script_obj_is(Ks_Script_Object obj, Ks_Script_Object_Type type)
{
    return obj.type == type;
}

KS_API ks_double ks_script_obj_as_number(Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_NUMBER) return 0.0;
    return obj.val.number;
}

KS_API ks_bool ks_script_obj_as_boolean(Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_BOOLEAN) return ks_false;
    return obj.val.boolean;
}

KS_API ks_str ks_script_obj_as_str(Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_STRING) return nullptr;
    return obj.val.string.data;
}

KS_API Ks_Script_Table ks_script_obj_as_table(Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_TABLE) {
        Ks_Script_Table tbl;
        tbl.state = KS_SCRIPT_OBJECT_INVALID;
        return tbl;
    }
    return static_cast<Ks_Script_Table>(obj);
}

KS_API Ks_Script_Function ks_script_obj_as_function(Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_FUNCTION) {
        Ks_Script_Function fn;
        fn.state = KS_SCRIPT_OBJECT_INVALID;
        return fn;
    }
    return static_cast<Ks_Script_Function>(obj);
}

KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine(Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_COROUTINE) {
        Ks_Script_Coroutine crtn;
        crtn.state = KS_SCRIPT_OBJECT_INVALID;
        return crtn;
    }
    return static_cast<Ks_Script_Coroutine>(obj);
}

KS_API ks_double ks_script_obj_as_number_or(Ks_Script_Object obj, ks_double def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_NUMBER) return def;
    return obj.val.number;
}

KS_API ks_bool ks_script_obj_as_boolean_or(Ks_Script_Object obj, ks_bool def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_BOOLEAN) return def;
    return obj.val.boolean;
}

KS_API ks_str ks_script_obj_as_str_or(Ks_Script_Object obj, ks_str def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_STRING) return def;
    return obj.val.string.data;
}

KS_API Ks_Script_Table ks_script_obj_as_table_or(Ks_Script_Object obj, Ks_Script_Table def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_TABLE) return def;
    return static_cast<Ks_Script_Table>(obj);
}

KS_API Ks_Script_Function ks_script_as_function_or(Ks_Script_Object obj, Ks_Script_Function def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_FUNCTION) return def;
    return static_cast<Ks_Script_Function>(obj);
}

KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine_or(Ks_Script_Object obj, Ks_Script_Coroutine def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_COROUTINE) return def;
    return static_cast<Ks_Script_Coroutine>(obj);
}

KS_API ks_bool ks_script_obj_try_as_number(Ks_Script_Object obj, ks_double* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_NUMBER) return ks_false;
    *out = obj.val.number;
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_boolean(Ks_Script_Object obj, ks_bool* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_BOOLEAN) return ks_false;
    *out = obj.val.boolean;
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_string(Ks_Script_Object obj, ks_str* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_STRING) return ks_false;
    *out = obj.val.string.data;
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_table(Ks_Script_Object obj, Ks_Script_Table* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_TABLE) return ks_false;
    *out = static_cast<Ks_Script_Table>(obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_function(Ks_Script_Object obj, Ks_Script_Function* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_FUNCTION) return ks_false;
    *out = static_cast<Ks_Script_Function>(obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_coroutine(Ks_Script_Object obj, Ks_Script_Coroutine* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_COROUTINE) return ks_false;
    *out = static_cast<Ks_Script_Coroutine>(obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_has_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return ks_false;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_obj(ctx, obj);
    ks_bool has_mt = lua_getmetatable(L, -1);
    lua_pop(L, has_mt ? 2 : 1);
    return has_mt;
}

KS_API Ks_Script_Table ks_script_obj_get_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_obj(ctx, obj);
    if (!lua_getmetatable(L, -1)) {
        lua_pop(L, 1);
        return ks_script_create_invalid_obj(ctx);
    }

    Ks_Script_Ref ref = sctx->store_in_registry();
    lua_pop(L, 1);

    Ks_Script_Table mt;
    mt.type = KS_SCRIPT_OBJECT_TYPE_TABLE;
    mt.state = KS_SCRIPT_OBJECT_VALID;
    mt.val.table = ref;
    return mt;
}

KS_API ks_no_ret ks_script_obj_set_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table mt)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_obj(ctx, obj);
    ks_script_stack_push_obj(ctx, mt);
    lua_setmetatable(L, -2);
    lua_pop(L, 1);
}

KS_API ks_bool ks_script_obj_is_callable(Ks_Script_Object obj)
{
    return obj.type == KS_SCRIPT_OBJECT_TYPE_FUNCTION ||
        (obj.type == KS_SCRIPT_OBJECT_TYPE_TABLE && obj.state == KS_SCRIPT_OBJECT_VALID);
}

KS_API ks_bool ks_script_obj_is_iterable(Ks_Script_Object obj)
{
    return obj.type == KS_SCRIPT_OBJECT_TYPE_TABLE;
}

KS_API ks_no_ret ks_script_obj_dump(Ks_Script_Object obj)
{
    switch (obj.type) {
    case KS_SCRIPT_OBJECT_TYPE_NIL:
        KS_LOG_TRACE("nil\n");
        break;
    case KS_SCRIPT_OBJECT_TYPE_NUMBER:
        KS_LOG_TRACE("%f\n", obj.val.number);
        break;
    case KS_SCRIPT_OBJECT_TYPE_BOOLEAN:
        KS_LOG_TRACE("%s\n", obj.val.boolean ? "true" : "false");
        break;
    case KS_SCRIPT_OBJECT_TYPE_STRING:
        KS_LOG_TRACE("\"%.*s\"\n", (int)obj.val.string.len, obj.val.string.data);
        break;
    case KS_SCRIPT_OBJECT_TYPE_TABLE:
        KS_LOG_TRACE("table: ref=%d\n", obj.val.table);
        break;
    case KS_SCRIPT_OBJECT_TYPE_FUNCTION:
        KS_LOG_TRACE("function: ref=%d\n", obj.val.function);
        break;
    case KS_SCRIPT_OBJECT_TYPE_COROUTINE:
        KS_LOG_TRACE("coroutine: ref=%d\n", obj.val.coroutine);
        break;
    case KS_SCRIPT_OBJECT_TYPE_USERDATA:
        KS_LOG_TRACE("userdata: %p (size=%zu)\n",
            obj.val.userdata.data, obj.val.userdata.size);
        break;
    case KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA:
        KS_LOG_TRACE("lightuserdata: %p\n", obj.val.lightuserdata);
        break;
    default:
        printf("unknown\n");
        break;
    }
}

KS_API ks_str ks_script_obj_to_string(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return nullptr;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_obj(ctx, obj);

    if (luaL_callmeta(L, -1, "__tostring")) {
        size_t len;
        const char* str = lua_tolstring(L, -1, &len);
        if (str) {
            char* copy = (char*)ks_alloc_debug(
                (len + 1) * sizeof(char),
                KS_LT_USER_MANAGED,
                KS_TAG_GARBAGE,
                "ObjToStringCopy"
            );
            std::memcpy((void*)copy, str, len);
            return copy;
        }
    }

    const char* str = lua_tostring(L, -1);
    if (str) {
        ks_size len = strlen(str);
        char* copy = (char*)ks_alloc_debug(
            (len + 1) * sizeof(char),
            KS_LT_USER_MANAGED,
            KS_TAG_GARBAGE,
            "ObjToStringCopy"
        );
        std::memcpy((void*)copy, str, len);
        return copy;
    }

    return nullptr;
}


static Ks_Script_Object_Type lua_type_to_ks(int type) {
    switch (type) {
    case -1: return KS_SCRIPT_OBJECT_TYPE_UNKNOWN;
    case LUA_TNIL: return KS_SCRIPT_OBJECT_TYPE_NIL;
    case LUA_TNUMBER: return KS_SCRIPT_OBJECT_TYPE_NUMBER;
    case LUA_TSTRING: return KS_SCRIPT_OBJECT_TYPE_STRING;
    case LUA_TBOOLEAN: return KS_SCRIPT_OBJECT_TYPE_BOOLEAN;
    case LUA_TTABLE: return KS_SCRIPT_OBJECT_TYPE_TABLE;
    case LUA_TFUNCTION: return KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    case LUA_TTHREAD: return KS_SCRIPT_OBJECT_TYPE_COROUTINE;
    case LUA_TLIGHTUSERDATA: return KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA;
    case LUA_TUSERDATA: return KS_SCRIPT_OBJECT_TYPE_USERDATA;
    }

    return KS_SCRIPT_OBJECT_TYPE_UNKNOWN;
}

KS_API Ks_Script_Error ks_script_get_last_error(Ks_Script_Ctx ctx)
{
    if (!ctx) return KS_SCRIPT_ERROR_CTX_NOT_CREATED;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);

    auto error_info = sctx->get_error_info();
    return error_info.error;
}

KS_API ks_str ks_script_get_last_error_str(Ks_Script_Ctx ctx)
{
    if (!ctx) return NULL;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);

    auto error_info = sctx->get_error_info();
    return error_info.message;
}

KS_API Ks_Script_Error_Info ks_script_get_last_error_info(Ks_Script_Ctx ctx)
{
    if (!ctx) {
        Ks_Script_Error_Info info;
        info.error = KS_SCRIPT_ERROR_CTX_NOT_CREATED;
        info.message = "Ks_Script_Ctx was not created!";
    }

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);


    auto error_info = sctx->get_error_info();
    return error_info;
}

KS_API ks_no_ret ks_script_clear_error(Ks_Script_Ctx ctx)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    sctx->clear_error();
}

KS_API ks_bool ks_script_table_has(Ks_Script_Ctx ctx, Ks_Script_Table tbl, Ks_Script_Object key)
{
    if (!ctx) return ks_false;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(tbl.val.table);
    ks_script_stack_push_obj(ctx, key);
    lua_gettable(L, -2);

    ks_bool has = !lua_isnil(L, -1);
    return has;
}

KS_API ks_no_ret ks_script_table_set(Ks_Script_Ctx ctx, Ks_Script_Table tbl, Ks_Script_Object key, Ks_Script_Object value)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(tbl.val.table);
    ks_script_stack_push_obj(ctx, key);
    ks_script_stack_push_obj(ctx, value);
    lua_settable(L, -3);

}

KS_API Ks_Script_Object ks_script_table_get(Ks_Script_Ctx ctx, Ks_Script_Table tbl, Ks_Script_Object key)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(tbl.val.table);

    ks_script_stack_push_obj(ctx, key);

    lua_gettable(L, -2);

    Ks_Script_Object obj = ks_script_stack_pop_obj(ctx);
    return obj;
}

KS_API ks_size ks_script_table_array_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl)
{
    if (!ctx) return 0;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(tbl.val.table);
    ks_size len = lua_rawlen(L, -1);
    return len;
}

KS_API ks_size ks_script_table_total_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl)
{
    if (!ctx) return 0;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(tbl.val.table);
    ks_size count = 0;
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        count++;
        lua_pop(L, 1);
    }
    return count;
}

KS_API Ks_Script_Table_Iterator ks_script_table_iterate(Ks_Script_Ctx ctx, Ks_Script_Table tbl)
{
    Ks_Script_Table_Iterator iter;
    iter.table_ref = tbl.val.table;
    iter.iter_started = ks_false;
    iter.current_key_ref = KS_SCRIPT_INVALID_REF;
    iter.valid = (tbl.type == KS_SCRIPT_OBJECT_TYPE_TABLE && tbl.state == KS_SCRIPT_OBJECT_VALID);
    return iter;
}

KS_API ks_bool ks_script_iterator_has_next(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator* iterator)
{
    if (!ctx || !iterator || !iterator->valid) return ks_false;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(iterator->table_ref);

    ks_bool has_next = ks_false;

    if (!iterator->iter_started) {
        lua_pushnil(L);
        has_next = (lua_next(L, -2) != 0);
        if (has_next) {
            lua_pop(L, 2);
        }
    }
    else {
        if (iterator->current_key_ref != KS_SCRIPT_INVALID_REF) {
            sctx->get_from_registry(iterator->current_key_ref);
            has_next = (lua_next(L, -2) != 0);
            if (has_next) {
                lua_pop(L, 2);
            }
            lua_pop(L, 1);
        }
        else {
            has_next = ks_false;
        }
    }

    lua_pop(L, 1);
    return has_next;
}

KS_API ks_bool ks_script_iterator_next(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator* iterator, Ks_Script_Object* key, Ks_Script_Object* value)
{
    if (!ctx || !iterator || !iterator->valid) {
        if (key) *key = ks_script_create_invalid_obj(ctx);
        if (value) *value = ks_script_create_invalid_obj(ctx);
        return ks_false;
    }

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(iterator->table_ref);
    if (!iterator->iter_started) {
        lua_pushnil(L);
        iterator->iter_started = ks_true;
    }
    else {
        if (iterator->current_key_ref != KS_SCRIPT_INVALID_REF) {
            sctx->get_from_registry(iterator->current_key_ref);
            sctx->release_from_registry(iterator->current_key_ref);
        }
        else {
            lua_pushnil(L); 
        }
    }

    if (lua_next(L, -2) != 0) {
        if (key) {
            *key = ks_script_stack_peek(ctx, -2);
        }
        if (value) {
            *value = ks_script_stack_peek(ctx, -1);
        }

        if (key && iterator->current_key_ref != KS_SCRIPT_INVALID_REF) {
            sctx->release_from_registry(iterator->current_key_ref);
        }

        lua_pushvalue(L, -2);
        iterator->current_key_ref = sctx->store_in_registry();

        lua_pop(L, 2);
        lua_pop(L, 1);
        return ks_true;
    }
    else {
        iterator->valid = ks_false;
        if (key) *key = ks_script_create_invalid_obj(ctx);
        if (value) *value = ks_script_create_invalid_obj(ctx);

        if (iterator->current_key_ref != KS_SCRIPT_INVALID_REF) {
            sctx->release_from_registry(iterator->current_key_ref);
            iterator->current_key_ref = KS_SCRIPT_INVALID_REF;
        }

        lua_pop(L, 1);
        return ks_false;
    }

    return ks_false;
}

KS_API ks_no_ret ks_script_iterator_destroy(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator* iterator)
{
    if (!iterator) return;

    if (iterator->current_key_ref != KS_SCRIPT_INVALID_REF) {
        auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
        sctx->release_from_registry(iterator->current_key_ref);
    }

    iterator->valid = ks_false;
    iterator->current_key_ref = KS_SCRIPT_INVALID_REF;
}

KS_API ks_no_ret ks_script_iterator_reset(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator* iterator)
{
    if (!iterator) return;
    if (iterator->current_key_ref != KS_SCRIPT_INVALID_REF) {
        auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
        sctx->release_from_registry(iterator->current_key_ref);
    }

    iterator->iter_started = ks_false;
    iterator->current_key_ref = KS_SCRIPT_INVALID_REF;
    iterator->valid = ks_true;
}

KS_API Ks_Script_Table_Iterator KS_API ks_script_iterator_clone(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator* iterator)
{
    Ks_Script_Table_Iterator clone = *iterator;
    return clone;
}

KS_API ks_no_ret ks_script_func_call(Ks_Script_Ctx ctx, Ks_Script_Function f, ks_size n_args, ks_size n_rets)
{
    if (!ctx || !ks_script_obj_is_callable(f)) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(f.val.function);

    ks_script_stack_insert(ctx, -(n_args + 1));

    if (lua_pcall(L, n_args, n_rets, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Function call failed");
        lua_pop(L, 1);
    }
}
