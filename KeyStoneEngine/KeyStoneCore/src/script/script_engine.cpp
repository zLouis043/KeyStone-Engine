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

#include <string.h>
#include <vector>
#include <map>
#include <stdarg.h>

static char KS_CTX_REGISTRY_KEY = 0;

enum DispatchMode {
    DISPATCH_NORMAL = 1,
    DISPATCH_METHOD = 2,
    DISPATCH_CONSTRUCTOR = 3
};

struct AutoCallFrame {
    KsScriptEngineCtx* ctx;
    AutoCallFrame(KsScriptEngineCtx* c, int arg_off, int uv_off) : ctx(c) {
        if (ctx) ctx->push_call_frame(arg_off, uv_off);
    }
    ~AutoCallFrame() {
        if (ctx) ctx->pop_call_frame();
    }
};

struct KsUsertypeInstanceHandle {
    void* instance;
    bool is_borrowed;
};

static void* lua_custom_Alloc(void* ud, void* ptr, size_t osize, size_t nsize);
static Ks_Script_Object_Type lua_type_to_ks(int type);
static ks_str ks_metamethod_to_str(Ks_Script_Metamethod mt);
static int universal_method_thunk(lua_State* L);
static int usertype_gc_thunk(lua_State* L);
static int usertype_index_thunk(lua_State* L);
static int usertype_newindex_thunk(lua_State* L);
static int usertype_auto_constructor_thunk(lua_State* L);
static int overload_dispatcher_thunk(lua_State* L);
static int generic_cfunc_thunk(lua_State* L);
static int instance_method_thunk(lua_State* L);
static void push_overload_dispatcher(lua_State* L, const std::vector<MethodInfo>& overloads, DispatchMode mode, ks_size instance_size = 0, const char* type_name = nullptr, ks_size n_user_upvalues = 0);
static bool check_signature_match(lua_State* L, int sig_tbl_idx, int start_idx, int args_to_check);
static void register_methods_to_table(lua_State* L, int table_idx, const std::map<std::string, std::vector<MethodInfo>>& methods_map, DispatchMode mode);
static void chain_usertype_tables(lua_State* L, int child_idx, const std::string& base_name, const char* table_suffix);
static void save_usertype_table(lua_State* L, int table_idx, const std::string& type_name, const char* table_suffix);

static int ks_script_error_handler(lua_State* L);

Ks_Script_Ctx ks_script_create_ctx() {
	KsScriptEngineCtx* ctx = static_cast<KsScriptEngineCtx*>(ks_alloc_debug(
		sizeof(*ctx),
		KS_LT_PERMANENT,
		KS_TAG_INTERNAL_DATA,
		"KsScriptEngineCtx"
	));

	lua_State* state = lua_newstate(lua_custom_Alloc, nullptr);

    luaL_openlibs(state);

    KsScriptEngineCtx* ctx_cpp = new(ctx) KsScriptEngineCtx(state);

    lua_pushlightuserdata(state, (void*)&KS_CTX_REGISTRY_KEY);
    lua_pushlightuserdata(state, (void*)ctx_cpp);
    lua_settable(state, LUA_REGISTRYINDEX);
    
    return static_cast<Ks_Script_Ctx>(ctx);
}

KS_API ks_no_ret ks_script_destroy_ctx(Ks_Script_Ctx ctx)
{
    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    sctx->~KsScriptEngineCtx();
    ks_dealloc(sctx);
}

KS_API ks_no_ret ks_script_begin_scope(Ks_Script_Ctx ctx)
{
    if (!ctx) return;
    static_cast<KsScriptEngineCtx*>(ctx)->begin_scope();
}

KS_API ks_no_ret ks_script_end_scope(Ks_Script_Ctx ctx)
{
    if (!ctx) return;
    static_cast<KsScriptEngineCtx*>(ctx)->end_scope();
}

KS_API ks_no_ret ks_script_promote(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);

    Ks_Script_Ref ref = KS_SCRIPT_INVALID_REF;
    switch (obj.type) {
    case KS_SCRIPT_OBJECT_TYPE_TABLE:
    case KS_SCRIPT_OBJECT_TYPE_FUNCTION:
    case KS_SCRIPT_OBJECT_TYPE_COROUTINE:
    case KS_SCRIPT_OBJECT_TYPE_USERDATA:
    case KS_SCRIPT_OBJECT_TYPE_STRING:
        ref = obj.val.generic_ref;
        break;
    default:
        return;
    }

    if (ref != KS_SCRIPT_INVALID_REF) {
        sctx->promote_to_parent(ref);
    }
}

KS_API Ks_Script_Object ks_script_create_number(Ks_Script_Ctx ctx, ks_double val)
{
    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_NUMBER;
    obj.val.number = val;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    return obj;
}

KS_API Ks_Script_Object ks_script_create_boolean(Ks_Script_Ctx ctx, ks_bool val)
{
    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_BOOLEAN;
    obj.val.boolean = val;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    return obj;
}

KS_API Ks_Script_Object ks_script_create_nil(Ks_Script_Ctx ctx)
{
    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_NIL;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    memset(&obj.val, 0, sizeof(obj.val));
    return obj;
}

KS_API Ks_Script_Object ks_script_create_invalid_obj(Ks_Script_Ctx ctx)
{
    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_NIL;
    obj.state = KS_SCRIPT_OBJECT_INVALID;
    memset(&obj.val, 0, sizeof(obj.val));
    return obj;
}

KS_API Ks_Script_Function ks_script_create_cfunc(Ks_Script_Ctx ctx, ks_script_cfunc f)
{
    if (!ctx || !f) return ks_script_create_invalid_obj(ctx);

    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushlightuserdata(L, (void*)f);
    lua_pushcclosure(L, generic_cfunc_thunk, 1);

    Ks_Script_Function obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.function_ref = sctx->store_in_registry();

    return obj;
}

Ks_Script_Function ks_script_create_overloaded_cfunc(Ks_Script_Ctx ctx, Ks_Script_Overload_Def* overloads, ks_size count)
{
    if (!ctx || !overloads || count == 0) return ks_script_create_invalid_obj(ctx);
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    std::vector<MethodInfo> infos;
    infos.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        std::vector<Ks_Script_Object_Type> sig;
        if (overloads[i].signature && overloads[i].signature_len > 0) {
            sig.assign(overloads[i].signature, overloads[i].signature + overloads[i].signature_len);
        }
        infos.push_back({ "", overloads[i].func, sig });
    }

    push_overload_dispatcher(L, infos, DISPATCH_NORMAL, 0, nullptr);

    Ks_Script_Function func;
    func.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    func.val.function_ref = sctx->store_in_registry();
    func.state = KS_SCRIPT_OBJECT_VALID;
    return func;
}

KS_API Ks_Script_Function ks_script_create_cfunc_with_upvalues(Ks_Script_Ctx ctx, ks_script_cfunc f, ks_size n_upvalues)
{
    if (!ctx || !f) return ks_script_create_invalid_obj(ctx);

    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushlightuserdata(L, (void*)f);

    if (n_upvalues > 0) {
        lua_insert(L, -(int)(n_upvalues + 1));
    }

    int total_upvalues = n_upvalues + 1;

    lua_pushcclosure(L, generic_cfunc_thunk, total_upvalues);

    Ks_Script_Function obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.function_ref = sctx->store_in_registry();

    return obj;
}

Ks_Script_Function ks_script_create_overloaded_cfunc_with_upvalues(Ks_Script_Ctx ctx, Ks_Script_Overload_Def* overloads, ks_size count, ks_size n_upvalues)
{
    if (!ctx || !overloads || count == 0) return ks_script_create_invalid_obj(ctx);
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    std::vector<MethodInfo> infos;
    infos.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        std::vector<Ks_Script_Object_Type> sig;
        if (overloads[i].signature && overloads[i].signature_len > 0) {
            sig.assign(overloads[i].signature, overloads[i].signature + overloads[i].signature_len);
        }
        infos.push_back({ "", overloads[i].func, sig });
    }

    push_overload_dispatcher(L, infos, DISPATCH_NORMAL, 0, nullptr, n_upvalues);

    Ks_Script_Function func;
    func.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
    func.val.function_ref = sctx->store_in_registry();
    return func;
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
    obj.val.table_ref = ref;

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
    obj.val.table_ref = ref;

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

    lua_pushstring(L, val);
    Ks_Script_Ref ref = sctx->store_in_registry();

    Ks_Script_Table obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_STRING;
    obj.state = KS_SCRIPT_OBJECT_VALID; 
    obj.val.string_ref = ref;

    return obj;
}

KS_API Ks_Script_Object ks_script_create_lstring(Ks_Script_Ctx ctx, ks_str str, ks_size len)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushlstring(L, str, len);
    Ks_Script_Ref ref = sctx->store_in_registry();

    Ks_Script_Table obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_STRING;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.string_ref = ref;
    return obj;
}

KS_API Ks_Script_Userdata ks_script_create_userdata(Ks_Script_Ctx ctx, ks_size size)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_newuserdatauv(L, size, 0);

    Ks_Script_Ref ref = sctx->store_in_registry();

    Ks_Script_Userdata obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_USERDATA;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.userdata_ref = ref;

    return obj;
}

Ks_Script_Userdata ks_script_create_usertype_instance(Ks_Script_Ctx ctx, ks_str type_name)
{
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    UsertypeInfo* info = sctx->get_usertype_info(type_name);
    if (!info) return ks_script_create_invalid_obj(ctx);

    ks_size instance_size = info->size;

    size_t total_size = sizeof(KsUsertypeInstanceHandle) + instance_size;
    void* raw_mem = lua_newuserdatauv(L, total_size, 0);

    auto* handle = static_cast<KsUsertypeInstanceHandle*>(raw_mem);

    handle->instance = static_cast<uint8_t*>(raw_mem) + sizeof(KsUsertypeInstanceHandle);
    handle->is_borrowed = false;

    luaL_setmetatable(L, type_name);

    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_USERDATA;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.userdata_ref = sctx->store_in_registry();

    return obj;
}

Ks_Script_Object ks_script_create_usertype_ref(Ks_Script_Ctx ctx, ks_str type_name, void* ptr)
{
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    auto* handle = static_cast<KsUsertypeInstanceHandle*>(lua_newuserdatauv(L, sizeof(KsUsertypeInstanceHandle), 0));

    handle->instance = ptr;
    handle->is_borrowed = true; 

    luaL_setmetatable(L, type_name);

    Ks_Script_Ref ref = sctx->store_in_registry();

    Ks_Script_Object obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_USERDATA;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.userdata_ref = sctx->store_in_registry();

    return obj;
}

Ks_Script_Function_Call_Result ks_script_func_callv_impl(Ks_Script_Ctx ctx, Ks_Script_Function f, ...)
{
    if (!ctx || f.type != KS_SCRIPT_OBJECT_TYPE_FUNCTION) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    int top_entry = lua_gettop(L);

    std::vector<Ks_Script_Object> args;
    ks_va_list vargs;
    va_start(vargs, f);

    while (true) {
        Ks_Script_Object arg = va_arg(vargs, Ks_Script_Object);
        if (arg.state == KS_SCRIPT_OBJECT_INVALID) break;
        args.push_back(arg);
    }
    va_end(vargs);

    lua_pushcfunction(L, ks_script_error_handler);
    int err_func_idx = lua_gettop(L);

    sctx->get_from_registry(f.val.function_ref);

    for (const auto& arg : args) {
        ks_script_stack_push_obj(ctx, arg);
    }

    if (lua_pcall(L, args.size(), LUA_MULTRET, err_func_idx) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err);
        lua_pop(L, 2);
        return ks_script_create_invalid_obj(ctx);
    }

    lua_remove(L, err_func_idx);

    ks_int n_results = lua_gettop(L) - top_entry;
    
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

            sctx->get_from_registry(result_table.val.table_ref);
            ks_script_stack_push_obj(ctx, val);
            lua_rawseti(L, -2, i);
            lua_pop(L, 1);

            ks_script_free_obj(ctx, val);
        }

        return result_table;
    }
}

KS_API Ks_Script_Object ks_script_func_get_upvalue(Ks_Script_Ctx ctx, ks_upvalue_idx i)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    int idx = i + sctx->current_frame().upval_offset;
    if (sctx->current_frame().upval_offset == 0) idx = i + 1;

    lua_pushvalue(L, lua_upvalueindex(idx));

    return ks_script_stack_pop_obj(ctx);
}

KS_API ks_bool ks_script_call_succeded(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res)
{
    return res.state == KS_SCRIPT_OBJECT_VALID;
}

KS_API Ks_Script_Object ks_script_call_get_return(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res)
{
    return static_cast<Ks_Script_Object>(res);
}

KS_API ks_size ks_script_call_get_returns_count(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res)
{
    if (ks_script_obj_is(ctx, res, KS_SCRIPT_OBJECT_TYPE_TABLE)) {
        return ks_script_table_array_size(ctx, res);
    }

    return res.state == KS_SCRIPT_OBJECT_VALID ? 1 : 0;
}

KS_API Ks_Script_Object ks_script_call_get_return_at(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res, ks_size idx)
{
    if (ks_script_obj_is(ctx, res, KS_SCRIPT_OBJECT_TYPE_TABLE)) {
        auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
        lua_State* L = sctx->get_raw_state();

        sctx->get_from_registry(res.val.table_ref);
        lua_rawgeti(L, -1, (lua_Integer)idx);

        Ks_Script_Object obj = ks_script_stack_pop_obj(ctx);
        lua_pop(L, 1);

        return obj;
    }

    if ((idx == 1 || idx == 0) && res.state == KS_SCRIPT_OBJECT_VALID) {
        return static_cast<Ks_Script_Object>(res);
    }

    return ks_script_create_invalid_obj(ctx);
}

KS_API Ks_Script_Coroutine ks_script_create_coroutine(Ks_Script_Ctx ctx, Ks_Script_Function func)
{
    if (!ks_script_obj_is(ctx, func, KS_SCRIPT_OBJECT_TYPE_FUNCTION)) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_State* co = lua_newthread(L);

    Ks_Script_Ref ref = sctx->store_in_registry();

    sctx->get_from_registry(func.val.function_ref);
    lua_xmove(L, co, 1);

    Ks_Script_Coroutine obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_COROUTINE;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.coroutine_ref = ref;

    return obj;
}

KS_API Ks_Script_Coroutine_Status ks_script_coroutine_status(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine)
{
    if (!ks_script_obj_is(ctx, coroutine, KS_SCRIPT_OBJECT_TYPE_COROUTINE)) return KS_SCRIPT_COROUTINE_DEAD;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(coroutine.val.coroutine_ref);
    lua_State* co = lua_tothread(L, -1);
    lua_pop(L, 1);

    if (!co) return KS_SCRIPT_COROUTINE_DEAD;

    int status = lua_status(co);

    if (status == LUA_OK) {
        if (lua_gettop(co) == 0) return KS_SCRIPT_COROUTINE_DEAD;
        return KS_SCRIPT_COROUTINE_SUSPENDED;
    }
    else if (status == LUA_YIELD) {
        return KS_SCRIPT_COROUTINE_SUSPENDED;
    }
    return KS_SCRIPT_COROUTINE_ERROR;
}

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_resume(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine, ks_size n_args)
{
    if (!ks_script_obj_is(ctx, coroutine, KS_SCRIPT_OBJECT_TYPE_COROUTINE)) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(coroutine.val.coroutine_ref);
    lua_State* co = lua_tothread(L, -1);
    lua_pop(L, 1);

    if(!co) return ks_script_create_invalid_obj(ctx);

    lua_xmove(L, co, (int)n_args);

    int n_results = 0;

    int status = lua_resume(co, L, (int)n_args, &n_results);

    if (status == LUA_OK || status == LUA_YIELD) {
        if (n_results > 0) lua_xmove(co, L, n_results);

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

                sctx->get_from_registry(result_table.val.table_ref);
                ks_script_stack_push_obj(ctx, val);
                lua_rawseti(L, -2, i);
                lua_pop(L, 1);

                ks_script_free_obj(ctx, val);
            }
            return result_table;
        }
    }
    else {
        /*
        const char* err = lua_tostring(co, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Coroutine resume failed");
        lua_pop(co, 1);
        return ks_script_create_invalid_obj(ctx);
        */
        const char* err_msg = lua_tostring(co, -1);

        luaL_traceback(co, co, err_msg, 1);
        const char* full_trace = lua_tostring(co, -1);

        KS_LOG_ERROR("[LUA COROUTINE EXCEPTION] %s", full_trace);

        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err_msg ? err_msg : "Coroutine resume failed");

        lua_pop(co, 2);

        return ks_script_create_invalid_obj(ctx);
    }
}

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_resumev_impl(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine, ...)
{
    if (!ks_script_obj_is(ctx, coroutine, KS_SCRIPT_OBJECT_TYPE_COROUTINE)) return ks_script_create_invalid_obj(ctx);

    va_list args;
    va_start(args, coroutine);
    int n_args = 0;
    while (true) {
        Ks_Script_Object arg = va_arg(args, Ks_Script_Object);
        if (arg.state == KS_SCRIPT_OBJECT_INVALID) break;
        ks_script_stack_push_obj(ctx, arg);
        n_args++;
    }
    va_end(args);

    return ks_script_coroutine_resume(ctx, coroutine, n_args);
}

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_yield(Ks_Script_Ctx ctx, ks_size n_results)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    return (Ks_Script_Function_Call_Result)ks_script_create_number(ctx, lua_yield(L, (int)n_results));
}

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_yieldv_impl(Ks_Script_Ctx ctx, ...)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    va_list args;
    va_start(args, ctx);
    int n_results = 0;
    while (true) {
        Ks_Script_Object res = va_arg(args, Ks_Script_Object);
        if (res.state == KS_SCRIPT_OBJECT_INVALID) break;
        ks_script_stack_push_obj(ctx, res);
        n_results++;
    }
    va_end(args);

    return ks_script_coroutine_yield(ctx, n_results);
}

KS_API Ks_Script_LightUserdata ks_script_create_lightuserdata(Ks_Script_Ctx ctx, ks_ptr ptr)
{
    Ks_Script_LightUserdata obj;
    obj.type = KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.lightuserdata = ptr;
    return obj;
}

KS_API ks_ptr ks_script_lightuserdata_get_ptr(Ks_Script_Ctx ctx, Ks_Script_LightUserdata lud)
{
    if (lud.type != KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA) return nullptr;
    return lud.val.lightuserdata;
}

KS_API ks_ptr ks_script_userdata_get_ptr(Ks_Script_Ctx ctx, Ks_Script_Userdata ud)
{
    if (!ctx || ud.type != KS_SCRIPT_OBJECT_TYPE_USERDATA) return nullptr;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(ud.val.userdata_ref);

    void* ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);

    return ptr;
}

KS_API ks_no_ret ks_script_set_type_name(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str type_name)
{
    if (!ctx || !type_name) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    ks_script_stack_push_obj(ctx, obj);

    luaL_setmetatable(L, type_name);
    lua_pop(L, 1);
}

ks_ptr ks_script_get_self(Ks_Script_Ctx ctx)
{
    if (!ctx) return nullptr;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    auto* handle = static_cast<KsUsertypeInstanceHandle*>(lua_touserdata(L, 1));
    if (!handle) return nullptr;

    return handle->instance;
}

ks_ptr ks_script_usertype_get_ptr(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return nullptr;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_obj(ctx, obj);

    if (!lua_isuserdata(L, -1)) {
        lua_pop(L, 1);
        return nullptr;
    }

    auto* handle = static_cast<KsUsertypeInstanceHandle*>(lua_touserdata(L, -1));
    void* instance = handle ? handle->instance : nullptr;

    lua_pop(L, 1);
    return instance;
}

KS_API Ks_Script_Userytype_Builder ks_script_usertype_begin(Ks_Script_Ctx ctx, ks_str type_name, ks_size instance_size)
{
    void* mem = ks_alloc_debug(sizeof(KsUsertypeBuilder), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA, "UsertypeBuilder");
    return new(mem) KsUsertypeBuilder(ctx, type_name, instance_size);
}

KS_API ks_no_ret ks_script_usertype_inherits_from(Ks_Script_Userytype_Builder builder, ks_str base_type_name)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b && base_type_name) b->base_type_name = base_type_name;
}

KS_API ks_no_ret ks_script_usertype_add_constructor(Ks_Script_Userytype_Builder builder, ks_script_cfunc ctor)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b) b->constructors.push_back({ "ctor", ctor, {} });
}

ks_no_ret ks_script_usertype_add_constructor_overload(Ks_Script_Userytype_Builder builder, ks_script_cfunc ctor, Ks_Script_Object_Type* args, ks_size num_args)
{
    if (builder && ctor) {
        auto* b = reinterpret_cast<KsUsertypeBuilder*>(builder);
        std::vector<Ks_Script_Object_Type> sig;
        if (args && num_args > 0) sig.assign(args, args + num_args);
        b->constructors.push_back({ "ctor", ctor, sig });
    }
}

KS_API ks_no_ret ks_script_usertype_set_destructor(Ks_Script_Userytype_Builder builder, ks_script_deallocator dtor)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b) b->destructor = dtor;
}

KS_API ks_no_ret ks_script_usertype_add_method(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc func)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b && name && func) b->methods[name].push_back({ name, func, {} });
}

KS_API ks_no_ret ks_script_usertype_add_overload(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc func, Ks_Script_Object_Type* args, ks_size num_args) {
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b && name && func) {
        std::vector<Ks_Script_Object_Type> sig;
        if (args && num_args > 0) {
            sig.assign(args, args + num_args);
        }
        b->methods[name].push_back({ name, func, sig });
    }
}

KS_API ks_no_ret ks_script_usertype_add_static_method(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc func)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b && name && func) b->static_methods[name].push_back({ name, func, {} });
}

ks_no_ret ks_script_usertype_add_static_overload(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc func, Ks_Script_Object_Type* args, ks_size num_args)
{
    if (builder && name && func) {
        auto* b = reinterpret_cast<KsUsertypeBuilder*>(builder);
        std::vector<Ks_Script_Object_Type> sig;
        if (args && num_args > 0) sig.assign(args, args + num_args);
        b->static_methods[name].push_back({ name, func, sig });
    }
}

KS_API ks_no_ret ks_script_usertype_add_property(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc getter, ks_script_cfunc setter)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b && name) b->properties.push_back({ name, getter, setter });
}

KS_API ks_no_ret ks_script_usertype_add_metamethod(Ks_Script_Userytype_Builder builder, Ks_Script_Metamethod mt, ks_script_cfunc func)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b && func) b->metamethods[mt] = func;
}

KS_API ks_no_ret ks_script_usertype_end(Ks_Script_Userytype_Builder builder)
{
    if (!builder) return;
    auto* b = reinterpret_cast<KsUsertypeBuilder*>(builder);
    auto* sctx = static_cast<KsScriptEngineCtx*>(b->ctx);
    lua_State* L = sctx->get_raw_state();

    if (luaL_newmetatable(L, b->type_name.c_str()) == 0) {
    }
    int mt_idx = lua_gettop(L);

    if (b->destructor) {
        lua_pushstring(L, "__gc");
        lua_pushlightuserdata(L, (void*)b->destructor);
        lua_pushcclosure(L, usertype_gc_thunk, 1);
        lua_settable(L, mt_idx);
    }

    for (auto const& [mt, func] : b->metamethods) {
        const char* mt_name = ks_metamethod_to_str(mt);
        if (mt_name && func) {
            lua_pushstring(L, mt_name);
            lua_pushlightuserdata(L, (void*)func);
            lua_pushcclosure(L, generic_cfunc_thunk, 1);
            lua_settable(L, mt_idx);
        }
    }

    lua_newtable(L); int methods_tbl_idx = lua_gettop(L);
    lua_newtable(L); int getters_tbl_idx = lua_gettop(L);
    lua_newtable(L); int setters_tbl_idx = lua_gettop(L);

    if (!b->base_type_name.empty()) {
        chain_usertype_tables(L, methods_tbl_idx, b->base_type_name, "_methods");
        chain_usertype_tables(L, getters_tbl_idx, b->base_type_name, "_getters");
        chain_usertype_tables(L, setters_tbl_idx, b->base_type_name, "_setters");
    }

    save_usertype_table(L, methods_tbl_idx, b->type_name, "_methods");
    save_usertype_table(L, getters_tbl_idx, b->type_name, "_getters");
    save_usertype_table(L, setters_tbl_idx, b->type_name, "_setters");

    register_methods_to_table(L, methods_tbl_idx, b->methods, DISPATCH_METHOD);

    for (auto& p : b->properties) {
        if (p.getter) {
            lua_pushstring(L, p.name.c_str());
            lua_pushlightuserdata(L, (void*)p.getter);
            lua_pushcclosure(L, instance_method_thunk, 1);
            lua_settable(L, getters_tbl_idx);
        }
        if (p.setter) {
            lua_pushstring(L, p.name.c_str());
            lua_pushlightuserdata(L, (void*)p.setter);
            lua_pushcclosure(L, instance_method_thunk, 1);
            lua_settable(L, setters_tbl_idx);
        }
    }

    lua_pushstring(L, "__index");
    lua_pushvalue(L, methods_tbl_idx);
    lua_pushvalue(L, getters_tbl_idx);
    lua_pushcclosure(L, usertype_index_thunk, 2);
    lua_settable(L, mt_idx);

    lua_pushstring(L, "__newindex");
    lua_pushvalue(L, setters_tbl_idx);
    lua_pushcclosure(L, usertype_newindex_thunk, 1);
    lua_settable(L, mt_idx);

    lua_newtable(L); int class_tbl_idx = lua_gettop(L);

    register_methods_to_table(L, class_tbl_idx, b->static_methods, DISPATCH_NORMAL);

    if (!b->constructors.empty()) {
        lua_newtable(L);
        lua_pushstring(L, "__call");

        if (b->constructors.size() == 1 && b->constructors[0].signature.empty()) {
            lua_pushlightuserdata(L, (void*)b->constructors[0].func);
            lua_pushinteger(L, (lua_Integer)b->instance_size);
            lua_pushstring(L, b->type_name.c_str());
            lua_pushcclosure(L, usertype_auto_constructor_thunk, 3);
        }
        else {
            push_overload_dispatcher(L, b->constructors, DISPATCH_CONSTRUCTOR, b->instance_size, b->type_name.c_str());
        }

        lua_settable(L, -3);
        lua_setmetatable(L, class_tbl_idx);
    }

    lua_setglobal(L, b->type_name.c_str());

    lua_pop(L, 4);

    UsertypeInfo info;
    info.name = b->type_name;
    info.size = b->instance_size;
    sctx->register_usertype_info(b->type_name, info);

    b->~KsUsertypeBuilder();
    ks_dealloc(b);
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
        sctx->get_from_registry(val.val.string_ref);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_NUMBER: {
        lua_pushnumber(L, val.val.number);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_BOOLEAN: {
        lua_pushboolean(L, val.val.boolean);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_TABLE: {
        sctx->get_from_registry(val.val.table_ref);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_FUNCTION: {
        sctx->get_from_registry(val.val.function_ref);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_COROUTINE: {
        sctx->get_from_registry(val.val.coroutine_ref);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_USERDATA: {
        sctx->get_from_registry(val.val.userdata_ref);
    } break;
    case KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA: {
        lua_pushlightuserdata(L, val.val.lightuserdata);
    } break;
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

static int ks_script_error_handler(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    if (msg == NULL) {
        if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING) {
            return 1;
        }
        else {
            msg = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
        }
    }

    luaL_traceback(L, L, msg, 1);

    const char* full_trace = lua_tostring(L, -1);
    KS_LOG_ERROR("[LUA EXCEPTION] %s", full_trace);

    return 1;
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
    obj.val.function_ref = sctx->store_in_registry();
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
    obj.val.function_ref = sctx->store_in_registry();
    return obj;
}

KS_API Ks_Script_Function_Call_Result ks_script_do_string(Ks_Script_Ctx ctx, ks_str string)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    int top_before = lua_gettop(L);

    lua_pushcfunction(L, ks_script_error_handler);
    int err_func_idx = lua_gettop(L);

    if (luaL_loadstring(L, string) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Syntax Error");
        return ks_script_create_invalid_obj(ctx);
    }

    if (lua_pcall(L, 0, LUA_MULTRET, err_func_idx) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Runtime Error");
        lua_pop(L, 2);
        return ks_script_create_invalid_obj(ctx);
    }

    lua_remove(L, err_func_idx);

    int n_results = lua_gettop(L) - top_before;

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

            sctx->get_from_registry(result_table.val.table_ref);
            ks_script_stack_push_obj(ctx, val);
            lua_rawseti(L, -2, i);
            lua_pop(L, 1);

            ks_script_free_obj(ctx, val);
        }

        return result_table;
    }
}

KS_API Ks_Script_Function_Call_Result ks_script_do_file(Ks_Script_Ctx ctx, ks_str file_path)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushcfunction(L, ks_script_error_handler);
    int err_func_idx = lua_gettop(L);

    int top_before = lua_gettop(L);

    if (luaL_loadfile(L, file_path) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Failed to load file");
        lua_pop(L, 2);
        return ks_script_create_invalid_obj(ctx);
    }

    if (lua_pcall(L, 0, LUA_MULTRET, err_func_idx) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Runtime Error in file");
        lua_pop(L, 2);
        return ks_script_create_invalid_obj(ctx);
    }

    lua_remove(L, err_func_idx);

    int n_results = lua_gettop(L) - top_before;

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

            sctx->get_from_registry(result_table.val.table_ref);
            ks_script_stack_push_obj(ctx, val);
            lua_rawseti(L, -2, i);
            lua_pop(L, 1);

            ks_script_free_obj(ctx, val);
        }

        return result_table;
    }
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
    case LUA_TLIGHTUSERDATA: return ks_script_create_lightuserdata(ctx, lua_touserdata(L, i));
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TTHREAD: 
    case LUA_TUSERDATA: {

        Ks_Script_Object obj;

        if (type == LUA_TTABLE) obj.type = KS_SCRIPT_OBJECT_TYPE_TABLE;
        else if (type == LUA_TFUNCTION) obj.type = KS_SCRIPT_OBJECT_TYPE_FUNCTION;
        else if (type == LUA_TTHREAD) obj.type = KS_SCRIPT_OBJECT_TYPE_COROUTINE;
        else if (type == LUA_TUSERDATA) obj.type = KS_SCRIPT_OBJECT_TYPE_USERDATA;

        lua_pushvalue(L, i);
        obj.val.generic_ref = sctx->store_in_registry();

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
    return obj;
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

KS_API Ks_Script_Object_Type ks_script_obj_type(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    return obj.type;
}

KS_API ks_bool ks_script_obj_is_valid(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    return ctx != NULL && obj.state != KS_SCRIPT_OBJECT_INVALID;
}

KS_API ks_bool ks_script_obj_is(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Object_Type type)
{
    return obj.type == type;
}

KS_API ks_double ks_script_obj_as_number(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_NUMBER) return 0.0;
    return obj.val.number;
}

KS_API ks_bool ks_script_obj_as_boolean(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_BOOLEAN) return ks_false;
    return obj.val.boolean;
}

KS_API ks_str ks_script_obj_as_str(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_STRING) return nullptr;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(obj.val.string_ref);

    size_t len;
    const char* lua_str = lua_tolstring(L, -1, &len);

    char* copy = (char*)ks_alloc_debug(len + 1, KS_LT_FRAME, KS_TAG_SCRIPT, "StringCopy");
    memcpy(copy, lua_str, len + 1);

    lua_pop(L, 1);

    return copy;
}

KS_API Ks_Script_Table ks_script_obj_as_table(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_TABLE) {
        Ks_Script_Table tbl;
        tbl.state = KS_SCRIPT_OBJECT_INVALID;
        return tbl;
    }
    return static_cast<Ks_Script_Table>(obj);
}

KS_API Ks_Script_Function ks_script_obj_as_function(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_FUNCTION) {
        Ks_Script_Function fn;
        fn.state = KS_SCRIPT_OBJECT_INVALID;
        return fn;
    }
    return static_cast<Ks_Script_Function>(obj);
}

KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_COROUTINE) {
        Ks_Script_Coroutine crtn;
        crtn.state = KS_SCRIPT_OBJECT_INVALID;
        return crtn;
    }
    return static_cast<Ks_Script_Coroutine>(obj);
}

KS_API ks_double ks_script_obj_as_number_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_double def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_NUMBER) return def;
    return obj.val.number;
}

KS_API ks_bool ks_script_obj_as_boolean_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_bool def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_BOOLEAN) return def;
    return obj.val.boolean;
}

KS_API ks_str ks_script_obj_as_str_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_STRING) return def;
    return ks_script_obj_as_str(ctx, obj);
}

KS_API Ks_Script_Table ks_script_obj_as_table_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_TABLE) return def;
    return static_cast<Ks_Script_Table>(obj);
}

KS_API Ks_Script_Function ks_script_as_function_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Function def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_FUNCTION) return def;
    return static_cast<Ks_Script_Function>(obj);
}

KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Coroutine def)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_COROUTINE) return def;
    return static_cast<Ks_Script_Coroutine>(obj);
}

KS_API ks_bool ks_script_obj_try_as_number(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_double* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_NUMBER) return ks_false;
    *out = obj.val.number;
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_boolean(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_bool* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_BOOLEAN) return ks_false;
    *out = obj.val.boolean;
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_string(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_STRING) return ks_false;
    *out = ks_script_obj_as_str(ctx, obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_table(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_TABLE) return ks_false;
    *out = static_cast<Ks_Script_Table>(obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_function(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Function* out)
{
    if (obj.type != KS_SCRIPT_OBJECT_TYPE_FUNCTION) return ks_false;
    *out = static_cast<Ks_Script_Function>(obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_coroutine(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Coroutine* out)
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
    mt.val.table_ref = ref;
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

KS_API ks_bool ks_script_obj_is_callable(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    return obj.type == KS_SCRIPT_OBJECT_TYPE_FUNCTION ||
        (obj.type == KS_SCRIPT_OBJECT_TYPE_TABLE && obj.state == KS_SCRIPT_OBJECT_VALID);
}

KS_API ks_bool ks_script_obj_is_iterable(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    return obj.type == KS_SCRIPT_OBJECT_TYPE_TABLE;
}

KS_API ks_no_ret ks_script_obj_dump(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

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
    case KS_SCRIPT_OBJECT_TYPE_STRING: {
        sctx->get_from_registry(obj.val.string_ref);
        size_t len;
        const char* str = lua_tolstring(L, -1, &len);
        KS_LOG_TRACE("\"%.*s\" (len=%zu)\n", (int)len, str, len);
        lua_pop(L, 1);
    }break;
    case KS_SCRIPT_OBJECT_TYPE_TABLE:
        KS_LOG_TRACE("table: ref=%d\n", obj.val.table_ref);
        break;
    case KS_SCRIPT_OBJECT_TYPE_FUNCTION:
        KS_LOG_TRACE("function: ref=%d\n", obj.val.function_ref);
        break;
    case KS_SCRIPT_OBJECT_TYPE_COROUTINE:
        KS_LOG_TRACE("coroutine: ref=%d\n", obj.val.coroutine_ref);
        break;
    case KS_SCRIPT_OBJECT_TYPE_USERDATA: {

        sctx->get_from_registry(obj.val.userdata_ref);
        void* ptr = lua_touserdata(L, -1);
        size_t size = lua_rawlen(L, -1);

        KS_LOG_TRACE("userdata: %p (size=%zu)\n",
            ptr, size);

        lua_pop(L, 1);
    }break;
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
            memcpy((void*)copy, str, len);
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
        memcpy((void*)copy, str, len);
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

KS_API ks_no_ret ks_script_free_obj(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);

    switch (obj.type) {
    case KS_SCRIPT_OBJECT_TYPE_TABLE:
    case KS_SCRIPT_OBJECT_TYPE_FUNCTION:
    case KS_SCRIPT_OBJECT_TYPE_COROUTINE:
    case KS_SCRIPT_OBJECT_TYPE_USERDATA:
    case KS_SCRIPT_OBJECT_TYPE_STRING:
        if (obj.val.generic_ref != KS_SCRIPT_INVALID_REF) {
            sctx->release_from_registry(obj.val.generic_ref);
        }
        break;
    default:
        break;
    }
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

    sctx->get_from_registry(tbl.val.table_ref);
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

    sctx->get_from_registry(tbl.val.table_ref);
    ks_script_stack_push_obj(ctx, key);
    ks_script_stack_push_obj(ctx, value);
    lua_settable(L, -3);

    lua_pop(L, 1);

}

KS_API Ks_Script_Object ks_script_table_get(Ks_Script_Ctx ctx, Ks_Script_Table tbl, Ks_Script_Object key)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(tbl.val.table_ref);

    ks_script_stack_push_obj(ctx, key);

    lua_gettable(L, -2);

    Ks_Script_Object obj = ks_script_stack_pop_obj(ctx);

    lua_pop(L, 1);

    return obj;
}

KS_API ks_size ks_script_table_array_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl)
{
    if (!ctx) return 0;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(tbl.val.table_ref);
    ks_size len = lua_rawlen(L, -1);

    lua_pop(L, 1);

    return len;
}

KS_API ks_size ks_script_table_total_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl)
{
    if (!ctx) return 0;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(tbl.val.table_ref);
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
    iter.table_ref = tbl.val.table_ref;
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

Ks_Script_Object ks_script_get_arg(Ks_Script_Ctx ctx, ks_stack_idx n)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    return ks_script_stack_peek(ctx, n + sctx->current_frame().arg_offset);
}

Ks_Script_Object ks_script_get_upvalue(Ks_Script_Ctx ctx, ks_upvalue_idx n)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    int real_uv_idx = n + sctx->current_frame().upval_offset;
    lua_pushvalue(L, lua_upvalueindex(real_uv_idx));
    return ks_script_stack_pop_obj(ctx);
}

KS_API ks_no_ret ks_script_func_call(Ks_Script_Ctx ctx, Ks_Script_Function f, ks_size n_args, ks_size n_rets)
{
    if (!ctx || !ks_script_obj_is_callable(ctx, f)) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushcfunction(L, ks_script_error_handler);
    lua_insert(L, -(int)n_args - 1);
    int err_func_idx = lua_gettop(L) - (int)n_args;

    sctx->get_from_registry(f.val.function_ref);
    lua_insert(L, -(int)n_args - 1);

    //ks_script_stack_insert(ctx, -(n_args + 1));
    
    if (lua_pcall(L, n_args, n_rets, err_func_idx) != LUA_OK) {
        ks_str err = lua_tostring(L, -1);
        sctx->set_internal_error(KS_SCRIPT_ERROR_RUNTIME, err ? err : "Function call failed");
        lua_pop(L, 2);
    }
    else {
        lua_remove(L, err_func_idx);
    }
}

static const char* ks_metamethod_to_str(Ks_Script_Metamethod mt) {
    switch (mt) {
    case KS_SCRIPT_MT_ADD: return "__add";
    case KS_SCRIPT_MT_SUB: return "__sub";
    case KS_SCRIPT_MT_MUL: return "__mul";
    case KS_SCRIPT_MT_DIV: return "__div";
    case KS_SCRIPT_MT_IDIV: return "__idiv";
    case KS_SCRIPT_MT_MOD: return "__mod";
    case KS_SCRIPT_MT_POW: return "__pow";
    case KS_SCRIPT_MT_UNM: return "__unm";
    case KS_SCRIPT_MT_BNOT: return "__bnot";
    case KS_SCRIPT_MT_BAND: return "__band";
    case KS_SCRIPT_MT_BOR: return "__bor";
    case KS_SCRIPT_MT_BXOR: return "__bxor";
    case KS_SCRIPT_MT_SHL: return "__shl";
    case KS_SCRIPT_MT_SHR: return "__shr";
    case KS_SCRIPT_MT_CONCAT: return "__concat";
    case KS_SCRIPT_MT_LEN: return "__len";
    case KS_SCRIPT_MT_EQ: return "__eq";
    case KS_SCRIPT_MT_LT: return "__lt";
    case KS_SCRIPT_MT_LE: return "__le";
    case KS_SCRIPT_MT_TOSTRING: return "__tostring";
    case KS_SCRIPT_MT_CALL: return "__call";
    case KS_SCRIPT_MT_GC: return "__gc";
    case KS_SCRIPT_MT_CLOSE: return "__close";
    default: return nullptr;
    }
}

static int usertype_gc_thunk(lua_State* L) {

    auto* handle = static_cast<KsUsertypeInstanceHandle*>(lua_touserdata(L, 1));

    if (!handle || handle->is_borrowed) return 0;

    auto dtor = reinterpret_cast<ks_script_deallocator>(lua_touserdata(L, lua_upvalueindex(1)));

    if (handle->instance && dtor) {
        ks_size size = lua_rawlen(L, 1);
        dtor(handle->instance, size);
    }
    return 0;
}

static int usertype_index_thunk(lua_State* L) {
    lua_settop(L, 2);

    lua_pushvalue(L, 2);
    lua_gettable(L, lua_upvalueindex(1));

    if (!lua_isnil(L, -1)) {
        return 1;
    }
    lua_pop(L, 1);


    lua_pushvalue(L, 2);
    lua_gettable(L, lua_upvalueindex(2));

    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        return 1;
    }

    return 0;
}

static int usertype_newindex_thunk(lua_State* L) {
    lua_pushvalue(L, 2);
    lua_gettable(L, lua_upvalueindex(1));

    if (!lua_isnil(L, -1)) {
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 3);
        lua_call(L, 2, 0);
        return 0;
    }
    return luaL_error(L, "Attempt to set unknown property or field on usertype");
}

static int universal_method_thunk(lua_State* L) {
    lua_pushlightuserdata(L, (void*)&KS_CTX_REGISTRY_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* ctx = static_cast<KsScriptEngineCtx*>(lua_touserdata(L, -1));
    auto func = reinterpret_cast<ks_script_cfunc>(lua_touserdata(L, lua_upvalueindex(1)));

    if (ctx && func) {
        return func(static_cast<Ks_Script_Ctx>(ctx));
    }

    return luaL_error(L, "KeyStone Internal Error: Invalid context or function in method thunk");
}

static int usertype_auto_constructor_thunk(lua_State* L) {
    lua_remove(L, 1);

    ks_size size = (ks_size)lua_tointeger(L, lua_upvalueindex(2));
    const char* type_name = lua_tostring(L, lua_upvalueindex(3));

    size_t total_size = sizeof(KsUsertypeInstanceHandle) + size;
    void* raw_mem = lua_newuserdatauv(L, total_size, 0);

    auto* handle = static_cast<KsUsertypeInstanceHandle*>(raw_mem);
    handle->instance = static_cast<ks_byte*>(raw_mem) + sizeof(KsUsertypeInstanceHandle);
    handle->is_borrowed = false;

    luaL_setmetatable(L, type_name);

    lua_insert(L, 1);

    lua_pushlightuserdata(L, (void*)&KS_CTX_REGISTRY_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* ctx = static_cast<KsScriptEngineCtx*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    auto ctor_func = reinterpret_cast<ks_script_cfunc>(lua_touserdata(L, lua_upvalueindex(1)));
    if (ctx && ctor_func) {
        ctor_func(static_cast<Ks_Script_Ctx>(ctx));
    }

    lua_settop(L, 1);
    return 1;
}

static int generic_cfunc_thunk(lua_State* L) {
    lua_pushlightuserdata(L, (void*)&KS_CTX_REGISTRY_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* ctx = static_cast<KsScriptEngineCtx*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    auto func = reinterpret_cast<ks_script_cfunc>(lua_touserdata(L, lua_upvalueindex(1)));

    if (ctx && func) {

        AutoCallFrame frame(ctx, 0, 1);
        return func(static_cast<Ks_Script_Ctx>(ctx));
    }
    return luaL_error(L, "Internal Error: generic_cfunc_thunk");
}

static int instance_method_thunk(lua_State* L) {
    lua_pushlightuserdata(L, (void*)&KS_CTX_REGISTRY_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* ctx = static_cast<KsScriptEngineCtx*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    auto func = reinterpret_cast<ks_script_cfunc>(lua_touserdata(L, lua_upvalueindex(1)));

    if (ctx && func) {
        AutoCallFrame frame(ctx, 1, 1);
        return func(static_cast<Ks_Script_Ctx>(ctx));
    }
    return luaL_error(L, "Internal Error: instance_method_thunk");
}

static int overload_dispatcher_thunk(lua_State* L) {
    int overloads_tab_idx = lua_upvalueindex(1);
    int num_overloads = (int)lua_rawlen(L, overloads_tab_idx);
    DispatchMode mode = (DispatchMode)lua_tointeger(L, lua_upvalueindex(2));

    bool is_auto_ctor = (mode == DISPATCH_CONSTRUCTOR);
    if (is_auto_ctor) {
        ks_size size = (ks_size)lua_tointeger(L, lua_upvalueindex(3));
        lua_remove(L, 1); 
        if (size > 0) {
            const char* tname = lua_tostring(L, lua_upvalueindex(4));

            size_t total_size = sizeof(KsUsertypeInstanceHandle) + size;
            void* raw_mem = lua_newuserdatauv(L, total_size, 0);

            auto* handle = static_cast<KsUsertypeInstanceHandle*>(raw_mem);
            handle->instance = static_cast<ks_byte*>(raw_mem) + sizeof(KsUsertypeInstanceHandle);
            handle->is_borrowed = false;
            luaL_setmetatable(L, tname);
            lua_insert(L, 1);
        }
    }

    int start_match_idx = (mode == DISPATCH_NORMAL) ? 1 : 2;
    int original_top = lua_gettop(L);
    int actual_args_count = original_top - start_match_idx + 1;
    if (actual_args_count < 0) actual_args_count = 0;

    for (int i = 1; i <= num_overloads; ++i) {
        lua_rawgeti(L, overloads_tab_idx, i);
        lua_getfield(L, -1, "sig");
        int sig_tbl_idx = lua_gettop(L);

        if (check_signature_match(L, sig_tbl_idx, start_match_idx, actual_args_count)) {

            lua_getfield(L, -2, "func");
            auto func = reinterpret_cast<ks_script_cfunc>(lua_touserdata(L, -1));
            lua_settop(L, original_top);

            lua_pushlightuserdata(L, (void*)&KS_CTX_REGISTRY_KEY);
            lua_gettable(L, LUA_REGISTRYINDEX);
            auto* ctx = static_cast<KsScriptEngineCtx*>(lua_touserdata(L, -1));
            lua_pop(L, 1);

            if (ctx && func) {
                int arg_offset = (mode == DISPATCH_NORMAL) ? 0 : 1;
                AutoCallFrame frame(ctx, arg_offset, 4);

                int n_res = func(static_cast<Ks_Script_Ctx>(ctx));
                if (is_auto_ctor) { lua_settop(L, 1); return 1; }
                return n_res;
            }
            return luaL_error(L, "Dispatch Internal Error");
        }
        lua_pop(L, 2);
    }
    return luaL_error(L, "No matching overload found.");
}

static void push_overload_dispatcher(lua_State* L, const std::vector<MethodInfo>& overloads, DispatchMode mode, ks_size instance_size, const char* type_name, ks_size n_user_upvalues) {
    lua_createtable(L, (int)overloads.size(), 0);
    for (size_t i = 0; i < overloads.size(); ++i) {
        lua_createtable(L, 0, 2);
        lua_pushstring(L, "func"); lua_pushlightuserdata(L, (void*)overloads[i].func); lua_settable(L, -3);
        lua_pushstring(L, "sig");
        lua_createtable(L, (int)overloads[i].signature.size(), 0);
        for (size_t j = 0; j < overloads[i].signature.size(); ++j) {
            lua_pushinteger(L, (int)overloads[i].signature[j]);
            lua_rawseti(L, -2, (int)(j + 1));
        }
        lua_settable(L, -3);
        lua_rawseti(L, -2, (int)(i + 1));
    }

    lua_pushinteger(L, (int)mode);
    lua_pushinteger(L, (lua_Integer)instance_size);
    if (type_name) lua_pushstring(L, type_name); else lua_pushnil(L);

    if (n_user_upvalues > 0) {
        lua_rotate(L, -(int)(n_user_upvalues + 4), (int)n_user_upvalues);
    }

    lua_pushcclosure(L, overload_dispatcher_thunk, 4 + (int)n_user_upvalues);
}

static bool check_signature_match(lua_State* L, int sig_tbl_idx, int start_idx, int args_to_check) {
    size_t sig_len = lua_rawlen(L, sig_tbl_idx);

    if (static_cast<size_t>(args_to_check) != sig_len) {
        return false;
    }

    if (sig_len == 0) return true;

    for (size_t i = 0; i < sig_len; ++i) {
        lua_rawgeti(L, sig_tbl_idx, (int)(i + 1));
        int expected_type_enum = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        int stack_idx = start_idx + (int)i;
        int actual_lua_type = lua_type(L, stack_idx);

        bool match = false;
        switch (expected_type_enum) {
        case KS_SCRIPT_OBJECT_TYPE_NUMBER: match = (actual_lua_type == LUA_TNUMBER); break;
        case KS_SCRIPT_OBJECT_TYPE_STRING: match = (actual_lua_type == LUA_TSTRING); break;
        case KS_SCRIPT_OBJECT_TYPE_BOOLEAN: match = (actual_lua_type == LUA_TBOOLEAN); break;
        case KS_SCRIPT_OBJECT_TYPE_TABLE: match = (actual_lua_type == LUA_TTABLE); break;
        case KS_SCRIPT_OBJECT_TYPE_FUNCTION: match = (actual_lua_type == LUA_TFUNCTION); break;
        case KS_SCRIPT_OBJECT_TYPE_USERDATA: match = (actual_lua_type == LUA_TUSERDATA); break;
        case KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA: match = (actual_lua_type == LUA_TLIGHTUSERDATA); break;
        case KS_SCRIPT_OBJECT_TYPE_NIL: match = (actual_lua_type == LUA_TNIL); break;
        case KS_SCRIPT_OBJECT_TYPE_UNKNOWN: match = true; break;
        default: match = false;
        }

        if (!match) return false;
    }

    return true;
}

static void register_methods_to_table(lua_State* L, int table_idx, const std::map<std::string, std::vector<MethodInfo>>& methods_map, DispatchMode mode) {
    for (auto const& [name, overloads] : methods_map) {
        lua_pushstring(L, name.c_str());

        if (overloads.size() == 1 && overloads[0].signature.empty()) {
            lua_pushlightuserdata(L, (void*)overloads[0].func);

            if (mode == DISPATCH_METHOD) {
                lua_pushcclosure(L, instance_method_thunk, 1);
            }
            else {
                lua_pushcclosure(L, generic_cfunc_thunk, 1);
            }
        }
        else {
            push_overload_dispatcher(L, overloads, mode);
        }
        lua_settable(L, table_idx);
    }
}

static void chain_usertype_tables(lua_State* L, int child_idx, const std::string& base_name, const char* table_suffix) {
    if (base_name.empty()) return;
    lua_newtable(L);

    std::string registry_key = base_name + table_suffix;
    lua_pushstring(L, registry_key.c_str());
    lua_gettable(L, LUA_REGISTRYINDEX);

    if (lua_istable(L, -1)) {
        lua_setfield(L, -2, "__index");
        lua_setmetatable(L, child_idx);
    }
    else {
        lua_pop(L, 2);
    }
}

static void save_usertype_table(lua_State* L, int table_idx, const std::string& type_name, const char* table_suffix) {
    lua_pushvalue(L, table_idx);
    std::string key = type_name + table_suffix;
    lua_setfield(L, LUA_REGISTRYINDEX, key.c_str());
}