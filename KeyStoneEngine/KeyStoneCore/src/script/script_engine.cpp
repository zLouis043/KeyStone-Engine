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
static Ks_Type lua_type_to_ks(lua_State* L, int idx);
static ks_str ks_metamethod_to_str(Ks_Script_Metamethod mt);
static int universal_method_thunk(lua_State* L);
static int usertype_gc_thunk(lua_State* L);
static int usertype_index_thunk(lua_State* L);
static int usertype_newindex_thunk(lua_State* L);
static int usertype_auto_constructor_thunk(lua_State* L);
static int overload_dispatcher_thunk(lua_State* L);
static int generic_cfunc_thunk(lua_State* L);
static int instance_method_thunk(lua_State* L);
static int usertype_field_getter_thunk(lua_State* L);
static int usertype_field_setter_thunk(lua_State* L);
static void push_overload_dispatcher(lua_State* L, const std::vector<MethodInfo>& overloads, DispatchMode mode, ks_size instance_size = 0, const char* type_name = nullptr, ks_size n_user_upvalues = 0);
static bool check_signature_match(lua_State* L, int sig_tbl_idx, int start_idx, int args_to_check);
static void register_methods_to_table(lua_State* L, int table_idx, const std::map<std::string, std::vector<MethodInfo>>& methods_map, DispatchMode mode);
static void chain_usertype_tables(lua_State* L, int child_idx, const std::string& base_name, const char* table_suffix);
static void save_usertype_table(lua_State* L, int table_idx, const std::string& type_name, const char* table_suffix);
static std::vector<MethodInfo> convert_sigs(const Ks_Script_Sig_Def* sigs, size_t count, const char* name = "");
static int enum_newindex_error(lua_State* L);

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
    case KS_TYPE_SCRIPT_TABLE:
    case KS_TYPE_SCRIPT_FUNCTION:
    case KS_TYPE_SCRIPT_COROUTINE:
    case KS_TYPE_USERDATA:
    case KS_TYPE_CSTRING:
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
    obj.type = KS_TYPE_DOUBLE;
    obj.val.number = val;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    return obj;
}

Ks_Script_Object ks_script_create_integer(Ks_Script_Ctx ctx, ks_int64 val)
{
    Ks_Script_Object obj;
    obj.type = KS_TYPE_INT;
    obj.val.integer = val; 
    obj.state = KS_SCRIPT_OBJECT_VALID;
    return obj;
}

KS_API Ks_Script_Object ks_script_create_boolean(Ks_Script_Ctx ctx, ks_bool val)
{
    Ks_Script_Object obj;
    obj.type = KS_TYPE_BOOL;
    obj.val.boolean = val;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    return obj;
}

KS_API Ks_Script_Object ks_script_create_nil(Ks_Script_Ctx ctx)
{
    Ks_Script_Object obj;
    obj.type = KS_TYPE_NIL;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    memset(&obj.val, 0, sizeof(obj.val));
    return obj;
}

KS_API Ks_Script_Object ks_script_create_invalid_obj(Ks_Script_Ctx ctx)
{
    Ks_Script_Object obj;
    obj.type = KS_TYPE_NIL;
    obj.state = KS_SCRIPT_OBJECT_INVALID;
    memset(&obj.val, 0, sizeof(obj.val));
    return obj;
}

KS_API Ks_Script_Function ks_script_create_cfunc(Ks_Script_Ctx ctx, const Ks_Script_Sig_Def* sigs, ks_size count)
{
    if (!ctx || !sigs || count == 0) return ks_script_create_invalid_obj(ctx);

    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (count == 1 && sigs[0].num_args == 0 && sigs[0].args == nullptr) {
        lua_pushlightuserdata(L, (void*)sigs[0].func);
        lua_pushcclosure(L, generic_cfunc_thunk, 1);
    }
    else {
        std::vector<MethodInfo> infos = convert_sigs(sigs, count);
        push_overload_dispatcher(L, infos, DISPATCH_NORMAL);
    }

    Ks_Script_Function obj;
    obj.type = KS_TYPE_SCRIPT_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.function_ref = sctx->store_in_registry();

    return obj;
}

KS_API Ks_Script_Function ks_script_create_cfunc_with_upvalues(Ks_Script_Ctx ctx, const Ks_Script_Sig_Def* sigs, ks_size count, ks_size n_upvalues)
{
    if (!ctx || !sigs || count == 0) return ks_script_create_invalid_obj(ctx);

    KsScriptEngineCtx* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (count == 1 && sigs[0].num_args == 0 && sigs[0].args == nullptr) {
        lua_pushlightuserdata(L, (void*)sigs[0].func);
        if (n_upvalues > 0) lua_insert(L, -(int)(n_upvalues + 1));
        lua_pushcclosure(L, generic_cfunc_thunk, (int)n_upvalues + 1);
    }
    else {
        std::vector<MethodInfo> infos = convert_sigs(sigs, count);
        push_overload_dispatcher(L, infos, DISPATCH_NORMAL, 0, nullptr, n_upvalues);
    }

    Ks_Script_Function obj;
    obj.type = KS_TYPE_SCRIPT_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.function_ref = sctx->store_in_registry();

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
    obj.type = KS_TYPE_SCRIPT_TABLE;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.table_ref = ref;

    return obj;


}

KS_API Ks_Script_Table ks_script_create_table_with_capacity(Ks_Script_Ctx ctx, ks_size array_sz, ks_size hash_sz)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_createtable(L, (int)array_sz, (int)hash_sz);
    Ks_Script_Ref ref = sctx->store_in_registry();

    Ks_Script_Table obj;
    obj.type = KS_TYPE_SCRIPT_TABLE;
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
    obj.type = KS_TYPE_CSTRING;
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
    obj.type = KS_TYPE_CSTRING;
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
    obj.type = KS_TYPE_USERDATA;
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
    obj.type = KS_TYPE_USERDATA;
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
    obj.type = KS_TYPE_USERDATA;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.userdata_ref = ref;

    return obj;
}

Ks_Script_Function_Call_Result ks_script_func_callv_impl(Ks_Script_Ctx ctx, Ks_Script_Function f, ...)
{
    if (!ctx || f.type != KS_TYPE_SCRIPT_FUNCTION) return ks_script_create_invalid_obj(ctx);

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

    if (lua_pcall(L, (int)args.size(), LUA_MULTRET, err_func_idx) != LUA_OK) {
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

    int idx = (int)(i + sctx->current_frame().upval_offset);
    if (sctx->current_frame().upval_offset == 0) idx = (int)(i + 1);

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
    if (ks_script_obj_is(ctx, res, KS_TYPE_SCRIPT_TABLE)) {
        return ks_script_table_array_size(ctx, res);
    }

    return res.state == KS_SCRIPT_OBJECT_VALID ? 1 : 0;
}

KS_API Ks_Script_Object ks_script_call_get_return_at(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res, ks_size idx)
{
    if (ks_script_obj_is(ctx, res, KS_TYPE_SCRIPT_TABLE)) {
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
    if (!ks_script_obj_is(ctx, func, KS_TYPE_SCRIPT_FUNCTION)) return ks_script_create_invalid_obj(ctx);

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_State* co = lua_newthread(L);

    Ks_Script_Ref ref = sctx->store_in_registry();

    sctx->get_from_registry(func.val.function_ref);
    lua_xmove(L, co, 1);

    Ks_Script_Coroutine obj;
    obj.type = KS_TYPE_SCRIPT_COROUTINE;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.coroutine_ref = ref;

    return obj;
}

KS_API Ks_Script_Coroutine_Status ks_script_coroutine_status(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine)
{
    if (!ks_script_obj_is(ctx, coroutine, KS_TYPE_SCRIPT_COROUTINE)) return KS_SCRIPT_COROUTINE_DEAD;

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
    if (!ks_script_obj_is(ctx, coroutine, KS_TYPE_SCRIPT_COROUTINE)) return ks_script_create_invalid_obj(ctx);

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
    if (!ks_script_obj_is(ctx, coroutine, KS_TYPE_SCRIPT_COROUTINE)) return ks_script_create_invalid_obj(ctx);

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
    obj.type = KS_TYPE_LIGHTUSERDATA;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.lightuserdata = ptr;
    return obj;
}

KS_API ks_ptr ks_script_lightuserdata_get_ptr(Ks_Script_Ctx ctx, Ks_Script_LightUserdata lud)
{
    if (lud.type != KS_TYPE_LIGHTUSERDATA) return nullptr;
    return lud.val.lightuserdata;
}

KS_API ks_ptr ks_script_userdata_get_ptr(Ks_Script_Ctx ctx, Ks_Script_Userdata ud)
{
    if (!ctx || ud.type != KS_TYPE_USERDATA) return nullptr;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(ud.val.userdata_ref);

    void* ptr = lua_touserdata(L, -1);
    lua_pop(L, 1);

    return ptr;
}

ks_size ks_script_userdata_get_size(Ks_Script_Ctx ctx, Ks_Script_Userdata ud)
{
    if (!ctx || ud.type != KS_TYPE_USERDATA) return 0;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(ud.val.userdata_ref);

    if (!lua_isuserdata(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }

    ks_size size = lua_rawlen(L, -1);

    lua_pop(L, 1);

    return size;
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

Ks_UserData ks_script_usertype_get_body(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    Ks_UserData result = { nullptr, 0 };
    if (!ctx || obj.type != KS_TYPE_USERDATA) return result;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(obj.val.userdata_ref);

    if (!lua_isuserdata(L, -1)) {
        lua_pop(L, 1);
        return result;
    }

    auto* handle = static_cast<KsUsertypeInstanceHandle*>(lua_touserdata(L, -1));
    size_t total_size = lua_rawlen(L, -1);

    if (handle->is_borrowed) {
        result.data = handle->instance;
        result.size = 0; 
    }

    else {
        if (total_size > sizeof(KsUsertypeInstanceHandle)) {
            result.data = handle->instance;
            result.size = total_size - sizeof(KsUsertypeInstanceHandle);
        }
    }

    lua_pop(L, 1);
    return result;
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

KS_API ks_no_ret ks_script_usertype_add_constructor(Ks_Script_Userytype_Builder builder, const Ks_Script_Sig_Def* sigs, ks_size count)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (!b || !sigs || count == 0) return;

    std::vector<MethodInfo> infos = convert_sigs(sigs, count, "ctor");
    auto& ctor_vec = b->constructors;
    ctor_vec.insert(ctor_vec.end(), infos.begin(), infos.end());
}

KS_API ks_no_ret ks_script_usertype_set_destructor(Ks_Script_Userytype_Builder builder, ks_script_deallocator dtor)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b) b->destructor = dtor;
}

KS_API ks_no_ret ks_script_usertype_add_method(Ks_Script_Userytype_Builder builder, ks_str name, const Ks_Script_Sig_Def* sigs, ks_size count)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (!b || !name || !sigs || count == 0) return;

    std::vector<MethodInfo> infos = convert_sigs(sigs, count, name);
    auto& method_vec = b->methods[name];
    method_vec.insert(method_vec.end(), infos.begin(), infos.end());
}

KS_API ks_no_ret ks_script_usertype_add_static_method(Ks_Script_Userytype_Builder builder, ks_str name, const Ks_Script_Sig_Def* sigs, ks_size count)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (!b || !name || !sigs || count == 0) return;

    std::vector<MethodInfo> infos = convert_sigs(sigs, count, name);
    auto& method_vec = b->static_methods[name];
    method_vec.insert(method_vec.end(), infos.begin(), infos.end());
}

KS_API
ks_no_ret ks_script_usertype_add_field(Ks_Script_Userytype_Builder builder, ks_str name, Ks_Type type, ks_size offset, ks_str type_alias)
{
    auto* b = static_cast<KsUsertypeBuilder*>(builder);
    if (b && name) {
        std::string tname = (type_alias) ? type_alias : "";
        b->fields.push_back({ name, type, offset, tname });
    }
}
ks_no_ret ks_script_usertype_add_property(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc getter, ks_script_cfunc setter)
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

    lua_pushstring(L, "__ks_usertype_name");
    lua_pushstring(L, b->type_name.c_str());
    lua_settable(L, mt_idx);

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

    for (auto& field : b->fields) {
        lua_pushstring(L, field.name.c_str());
        lua_pushinteger(L, (int)field.offset);
        lua_pushinteger(L, (int)field.type);
        lua_pushstring(L, field.type_name.c_str());
        lua_pushcclosure(L, usertype_field_getter_thunk, 3);
        lua_settable(L, getters_tbl_idx);

        lua_pushstring(L, field.name.c_str());
        lua_pushinteger(L, (int)field.offset);
        lua_pushinteger(L, (int)field.type);
        lua_pushstring(L, field.type_name.c_str());
        lua_pushcclosure(L, usertype_field_setter_thunk, 3);
        lua_settable(L, setters_tbl_idx);
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

KS_API ks_no_ret ks_script_register_enum_impl(Ks_Script_Ctx ctx, ks_str enum_name, const Ks_Script_Enum_Member* members, ks_size count)
{
    if (!ctx || !enum_name || !members || count == 0) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_newtable(L);
    for (ks_size i = 0; i < count; ++i) {
        lua_pushstring(L, members[i].name);
        lua_pushinteger(L, (lua_Integer)members[i].value);
        lua_settable(L, -3);
    }

    lua_newtable(L);

    lua_newtable(L);

    lua_pushvalue(L, -3);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, enum_newindex_error);
    lua_setfield(L, -2, "__newindex");

    lua_pushliteral(L, "readonly");
    lua_setfield(L, -2, "__metatable");

    lua_setmetatable(L, -2);

    lua_setglobal(L, enum_name);

    lua_pop(L, 1);
}

KS_API ks_no_ret ks_script_stack_push_number(Ks_Script_Ctx ctx, ks_double val)
{
    if (!ctx) return;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushnumber(L, val);
}

ks_no_ret ks_script_stack_push_integer(Ks_Script_Ctx ctx, ks_int64 val)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_pushinteger(sctx->get_raw_state(), (lua_Integer)val);
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
    case KS_TYPE_UNKNOWN:
    case KS_TYPE_VOID: 
    case KS_TYPE_SCRIPT_ANY:
    case KS_TYPE_NIL: {
        lua_pushnil(L);
    } break;
    case KS_TYPE_CSTRING: {
        sctx->get_from_registry(val.val.string_ref);
    } break;
    case KS_TYPE_INT:
    case KS_TYPE_UINT:
    case KS_TYPE_CHAR: {
        lua_pushinteger(L, (lua_Integer)val.val.integer);
    }break;
    case KS_TYPE_DOUBLE:
    case KS_TYPE_FLOAT: {
        lua_pushnumber(L, val.val.number);
    }break;
    case KS_TYPE_BOOL: {
        lua_pushboolean(L, val.val.boolean);
    } break;
    case KS_TYPE_SCRIPT_TABLE: {
        sctx->get_from_registry(val.val.table_ref);
    } break;
    case KS_TYPE_SCRIPT_FUNCTION: {
        sctx->get_from_registry(val.val.function_ref);
    } break;
    case KS_TYPE_SCRIPT_COROUTINE: {
        sctx->get_from_registry(val.val.coroutine_ref);
    } break;
    case KS_TYPE_USERDATA: {
        sctx->get_from_registry(val.val.userdata_ref);
    } break;
    case KS_TYPE_PTR:
    case KS_TYPE_LIGHTUSERDATA: {
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

ks_int64 ks_script_stack_pop_integer(Ks_Script_Ctx ctx)
{
    if (!ctx) return 0;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    if (!lua_isinteger(L, -1) && !lua_isnumber(L, -1)) return 0;
    ks_int64 val = (ks_int64)lua_tointeger(L, -1);
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

KS_API Ks_Script_Function ks_script_load_cstring(Ks_Script_Ctx ctx, ks_str string)
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
    obj.type = KS_TYPE_SCRIPT_FUNCTION;
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
    obj.type = KS_TYPE_SCRIPT_FUNCTION;
    obj.state = KS_SCRIPT_OBJECT_VALID;
    obj.val.function_ref = sctx->store_in_registry();
    return obj;
}

KS_API Ks_Script_Function_Call_Result ks_script_do_cstring(Ks_Script_Ctx ctx, ks_str string)
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

    int type = lua_type(L, (int)i);

    switch (type) {
    case LUA_TNIL:
        return ks_script_create_nil(ctx);
    case LUA_TNUMBER:
        if (lua_isinteger(L, (int)i)) return ks_script_create_integer(ctx, lua_tointeger(L, (int)i));
        return ks_script_create_number(ctx, lua_tonumber(L, (int)i));
    case LUA_TBOOLEAN:
        return ks_script_create_boolean(ctx, lua_toboolean(L, (int)i));
    case LUA_TSTRING:
        return ks_script_create_cstring(ctx, lua_tostring(L, (int)i));
    case LUA_TLIGHTUSERDATA: return ks_script_create_lightuserdata(ctx, lua_touserdata(L, (int)i));
    case LUA_TTABLE:
    case LUA_TFUNCTION:
    case LUA_TTHREAD: 
    case LUA_TUSERDATA: {

        Ks_Script_Object obj;

        if (type == LUA_TTABLE) obj.type = KS_TYPE_SCRIPT_TABLE;
        else if (type == LUA_TFUNCTION) obj.type = KS_TYPE_SCRIPT_FUNCTION;
        else if (type == LUA_TTHREAD) obj.type = KS_TYPE_SCRIPT_COROUTINE;
        else if (type == LUA_TUSERDATA) obj.type = KS_TYPE_USERDATA;

        lua_pushvalue(L, (int)i);
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

    Ks_Script_Object obj = ks_script_stack_peek(ctx, (int)i);
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
    lua_remove(L, (int)i);
}

KS_API ks_no_ret ks_script_stack_insert(Ks_Script_Ctx ctx, ks_stack_idx i)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_insert(L, (int)i);
}

KS_API ks_no_ret ks_script_stack_replace(Ks_Script_Ctx ctx, ks_stack_idx i)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    lua_replace(L, (int)i);
}

KS_API ks_no_ret ks_script_stack_copy(Ks_Script_Ctx ctx, ks_stack_idx from, ks_stack_idx to)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    lua_pushvalue(L, (int)from);
    lua_replace(L, (int)to);
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

KS_API Ks_Type ks_script_obj_type(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    return obj.type;
}

KS_API ks_bool ks_script_obj_is_valid(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    return ctx != NULL && obj.state != KS_SCRIPT_OBJECT_INVALID;
}

KS_API ks_bool ks_script_obj_is(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Type type)
{
    if (!ctx) return ks_false;

    if (obj.state == KS_SCRIPT_OBJECT_INVALID) return ks_false;

    if (obj.type == type) return ks_true;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_obj(ctx, obj);
    int lua_t = lua_type(L, -1);
    bool match = false;

    switch (type) {
    case KS_TYPE_NIL:
        match = (lua_t == LUA_TNIL);
        break;

    case KS_TYPE_BOOL:
        match = (lua_t == LUA_TBOOLEAN);
        break;
    case KS_TYPE_INT:
    case KS_TYPE_UINT:
    case KS_TYPE_CHAR:
        match = lua_isinteger(L, -1) || (lua_isnumber(L, -1) && !lua_isstring(L, -1));
        break;

    case KS_TYPE_FLOAT:
    case KS_TYPE_DOUBLE:
        match = lua_isnumber(L, -1) && !lua_isstring(L, -1);
        break;

    case KS_TYPE_CSTRING:
        match = (lua_t == LUA_TSTRING);
        break;

    case KS_TYPE_SCRIPT_TABLE:
        match = (lua_t == LUA_TTABLE);
        break;

    case KS_TYPE_SCRIPT_FUNCTION:
        match = (lua_t == LUA_TFUNCTION);
        break;

    case KS_TYPE_SCRIPT_COROUTINE:
        match = (lua_t == LUA_TTHREAD);
        break;

    case KS_TYPE_USERDATA:
        match = (lua_t == LUA_TUSERDATA);
        break;

    case KS_TYPE_LIGHTUSERDATA:
        match = (lua_t == LUA_TLIGHTUSERDATA);
        break;

    case KS_TYPE_PTR:
        match = (lua_t == LUA_TUSERDATA || lua_t == LUA_TLIGHTUSERDATA);
        break;

    case KS_TYPE_SCRIPT_ANY:
        match = true;
        break;

    default:
        match = false;
    }

    lua_pop(L, 1);
    return match ? ks_true : ks_false;
}
ks_str ks_script_obj_get_usertype_name(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx || obj.type != KS_TYPE_USERDATA) return nullptr;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    ks_script_stack_push_obj(ctx, obj);

    if (!lua_getmetatable(L, -1)) {
        lua_pop(L, 1);
        return nullptr;
    }

    lua_pushstring(L, "__ks_usertype_name");
    lua_rawget(L, -2);

    const char* result = nullptr;

    if (lua_isstring(L, -1)) {
        size_t len;
        const char* raw_str = lua_tolstring(L, -1, &len);

        char* copy = (char*)ks_alloc_debug(
            len + 1,
            KS_LT_FRAME,
            KS_TAG_SCRIPT,
            "UsertypeNameCopy"
        );
        memcpy(copy, raw_str, len + 1);
        result = copy;
    }

    lua_pop(L, 3);

    return result;
}
KS_API ks_double ks_script_obj_as_number(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type == KS_TYPE_DOUBLE || obj.type == KS_TYPE_FLOAT) {
        return obj.val.number;
    }
    if (obj.type == KS_TYPE_INT || obj.type == KS_TYPE_UINT || obj.type == KS_TYPE_CHAR) {
        return (ks_double)obj.val.integer;
    }
    return 0.0;
}

ks_int64 ks_script_obj_as_integer(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type == KS_TYPE_INT || obj.type == KS_TYPE_UINT || obj.type == KS_TYPE_CHAR) {
        return obj.val.integer;
    }
    if (obj.type == KS_TYPE_DOUBLE || obj.type == KS_TYPE_FLOAT) {
        return (ks_int64)obj.val.number;
    }
    return 0;
}

KS_API ks_bool ks_script_obj_as_boolean(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_TYPE_BOOL) return ks_false;
    return obj.val.boolean;
}

KS_API ks_str ks_script_obj_as_cstring(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_TYPE_CSTRING) return nullptr;
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

KS_API Ks_UserData ks_script_obj_as_userdata(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    Ks_UserData data;
    data.data = nullptr;
    data.size = 0;

    if (!ctx || obj.type != KS_TYPE_USERDATA) return data;

    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    sctx->get_from_registry(obj.val.userdata_ref);

    void* ptr = lua_touserdata(L, -1);
    size_t size = lua_rawlen(L, -1);
    lua_pop(L, 1);

    data.data = ptr;
    data.size = size;

    return data;
}

KS_API Ks_Script_Table ks_script_obj_as_table(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_TYPE_SCRIPT_TABLE) {
        Ks_Script_Table tbl;
        tbl.state = KS_SCRIPT_OBJECT_INVALID;
        return tbl;
    }
    return static_cast<Ks_Script_Table>(obj);
}

KS_API Ks_Script_Function ks_script_obj_as_function(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_TYPE_SCRIPT_FUNCTION) {
        Ks_Script_Function fn;
        fn.state = KS_SCRIPT_OBJECT_INVALID;
        return fn;
    }
    return static_cast<Ks_Script_Function>(obj);
}

KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (obj.type != KS_TYPE_SCRIPT_COROUTINE) {
        Ks_Script_Coroutine crtn;
        crtn.state = KS_SCRIPT_OBJECT_INVALID;
        return crtn;
    }
    return static_cast<Ks_Script_Coroutine>(obj);
}

KS_API ks_double ks_script_obj_as_number_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_double def)
{
    if (obj.type == KS_TYPE_DOUBLE || obj.type == KS_TYPE_FLOAT) return obj.val.number;
    if (obj.type == KS_TYPE_INT || obj.type == KS_TYPE_UINT || obj.type == KS_TYPE_CHAR) return (ks_double)obj.val.integer;
    return def;
}

ks_int64 ks_script_obj_as_integer_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_int64 def)
{
    if (obj.type == KS_TYPE_INT || obj.type == KS_TYPE_UINT) return obj.val.integer;
    if (obj.type == KS_TYPE_DOUBLE) return (ks_int64)obj.val.number;
    return def;
}

KS_API ks_bool ks_script_obj_as_boolean_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_bool def)
{
    if (obj.type != KS_TYPE_BOOL) return def;
    return obj.val.boolean;
}

KS_API ks_str ks_script_obj_as_cstring_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str def)
{
    if (obj.type != KS_TYPE_CSTRING) return def;
    return ks_script_obj_as_cstring(ctx, obj);
}

Ks_UserData ks_script_obj_as_userdata_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_UserData def)
{
    if (obj.type != KS_TYPE_USERDATA) return def;
    return ks_script_obj_as_userdata(ctx, obj);
}

KS_API Ks_Script_Table ks_script_obj_as_table_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table def)
{
    if (obj.type != KS_TYPE_SCRIPT_TABLE) return def;
    return static_cast<Ks_Script_Table>(obj);
}

KS_API Ks_Script_Function ks_script_as_function_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Function def)
{
    if (obj.type != KS_TYPE_SCRIPT_FUNCTION) return def;
    return static_cast<Ks_Script_Function>(obj);
}

KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Coroutine def)
{
    if (obj.type != KS_TYPE_SCRIPT_COROUTINE) return def;
    return static_cast<Ks_Script_Coroutine>(obj);
}

KS_API ks_bool ks_script_obj_try_as_number(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_double* out)
{
    if (obj.type != KS_TYPE_DOUBLE && obj.type != KS_TYPE_FLOAT &&
        obj.type != KS_TYPE_INT && obj.type != KS_TYPE_UINT) return ks_false;
    *out = obj.val.number;
    return ks_true;
}

ks_bool ks_script_obj_try_as_integer(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_int64* out)
{
    if (obj.type == KS_TYPE_INT || obj.type == KS_TYPE_UINT) {
        *out = obj.val.integer;
        return ks_true;
    }
    if (obj.type == KS_TYPE_DOUBLE) {
        *out = (ks_int64)obj.val.number;
        return ks_true;
    }
    return ks_false;
}

KS_API ks_bool ks_script_obj_try_as_boolean(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_bool* out)
{
    if (obj.type != KS_TYPE_BOOL) return ks_false;
    *out = obj.val.boolean;
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_cstring(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str* out)
{
    if (obj.type != KS_TYPE_CSTRING) return ks_false;
    *out = ks_script_obj_as_cstring(ctx, obj);
    return ks_true;
}

ks_bool ks_script_obj_try_as_userdata(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_UserData* out)
{
    if (obj.type != KS_TYPE_USERDATA) return ks_false;
    *out = ks_script_obj_as_userdata(ctx, obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_table(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table* out)
{
    if (obj.type != KS_TYPE_SCRIPT_TABLE) return ks_false;
    *out = static_cast<Ks_Script_Table>(obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_function(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Function* out)
{
    if (obj.type != KS_TYPE_SCRIPT_FUNCTION) return ks_false;
    *out = static_cast<Ks_Script_Function>(obj);
    return ks_true;
}

KS_API ks_bool ks_script_obj_try_as_coroutine(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Coroutine* out)
{
    if (obj.type != KS_TYPE_SCRIPT_COROUTINE) return ks_false;
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
    mt.type = KS_TYPE_SCRIPT_TABLE;
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
    return obj.type == KS_TYPE_SCRIPT_FUNCTION ||
        (obj.type == KS_TYPE_SCRIPT_TABLE && obj.state == KS_SCRIPT_OBJECT_VALID);
}

KS_API ks_bool ks_script_obj_is_iterable(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    return obj.type == KS_TYPE_SCRIPT_TABLE;
}

KS_API ks_no_ret ks_script_obj_dump(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();

    switch (obj.type) {
    case KS_TYPE_NIL:
        KS_LOG_TRACE("nil\n");
        break;
    case KS_TYPE_DOUBLE:
        KS_LOG_TRACE("%f\n", obj.val.number);
        break;
    case KS_TYPE_BOOL:
        KS_LOG_TRACE("%s\n", obj.val.boolean ? "true" : "false");
        break;
    case KS_TYPE_CSTRING: {
        sctx->get_from_registry(obj.val.string_ref);
        size_t len;
        const char* str = lua_tolstring(L, -1, &len);
        KS_LOG_TRACE("\"%.*s\" (len=%zu)\n", (int)len, str, len);
        lua_pop(L, 1);
    }break;
    case KS_TYPE_SCRIPT_TABLE:
        KS_LOG_TRACE("table: ref=%d\n", obj.val.table_ref);
        break;
    case KS_TYPE_SCRIPT_FUNCTION:
        KS_LOG_TRACE("function: ref=%d\n", obj.val.function_ref);
        break;
    case KS_TYPE_SCRIPT_COROUTINE:
        KS_LOG_TRACE("coroutine: ref=%d\n", obj.val.coroutine_ref);
        break;
    case KS_TYPE_USERDATA: {

        sctx->get_from_registry(obj.val.userdata_ref);
        void* ptr = lua_touserdata(L, -1);
        size_t size = lua_rawlen(L, -1);

        KS_LOG_TRACE("userdata: %p (size=%zu)\n",
            ptr, size);

        lua_pop(L, 1);
    }break;
    case KS_TYPE_LIGHTUSERDATA:
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


static Ks_Type lua_type_to_ks(lua_State* L, int idx) {
    int type = lua_type(L, idx);
    switch (type) {
    case -1: return KS_TYPE_UNKNOWN;
    case LUA_TNIL: return KS_TYPE_NIL;
    case LUA_TNUMBER: 
        if (lua_isinteger(L, idx)) return KS_TYPE_INT;
        return KS_TYPE_DOUBLE;
    case LUA_TSTRING: return KS_TYPE_CSTRING;
    case LUA_TBOOLEAN: return KS_TYPE_BOOL;
    case LUA_TTABLE: return KS_TYPE_SCRIPT_TABLE;
    case LUA_TFUNCTION: return KS_TYPE_SCRIPT_FUNCTION;
    case LUA_TTHREAD: return KS_TYPE_SCRIPT_COROUTINE;
    case LUA_TLIGHTUSERDATA: return KS_TYPE_LIGHTUSERDATA;
    case LUA_TUSERDATA: return KS_TYPE_USERDATA;
    }

    return KS_TYPE_UNKNOWN;
}

KS_API ks_no_ret ks_script_free_obj(Ks_Script_Ctx ctx, Ks_Script_Object obj)
{
    if (!ctx) return;
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);

    switch (obj.type) {
    case KS_TYPE_SCRIPT_TABLE:
    case KS_TYPE_SCRIPT_FUNCTION:
    case KS_TYPE_SCRIPT_COROUTINE:
    case KS_TYPE_USERDATA:
    case KS_TYPE_CSTRING:
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
    iter.valid = (tbl.type == KS_TYPE_SCRIPT_TABLE && tbl.state == KS_SCRIPT_OBJECT_VALID);
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

KS_API Ks_Script_Table_Iterator ks_script_iterator_clone(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator* iterator)
{
    Ks_Script_Table_Iterator clone = *iterator;
    return clone;
}

Ks_Script_Object ks_script_get_arg(Ks_Script_Ctx ctx, ks_stack_idx n)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    return ks_script_stack_peek(ctx, (int)(n + sctx->current_frame().arg_offset));
}

Ks_Script_Object ks_script_get_upvalue(Ks_Script_Ctx ctx, ks_upvalue_idx n)
{
    if (!ctx) return ks_script_create_invalid_obj(ctx);
    auto* sctx = static_cast<KsScriptEngineCtx*>(ctx);
    lua_State* L = sctx->get_raw_state();
    int real_uv_idx = (int)(n + sctx->current_frame().upval_offset);
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
    
    if (lua_pcall(L, (int)n_args, (int)n_rets, err_func_idx) != LUA_OK) {
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
        int current_top = lua_gettop(L);
        int first_user_abs_idx = current_top - 4 - (int)n_user_upvalues + 1;

        for (ks_size k = 0; k < n_user_upvalues; ++k) {
            lua_pushvalue(L, first_user_abs_idx);
            lua_remove(L, first_user_abs_idx);
        }

        //lua_rotate(L, -(int)(n_user_upvalues + 4), (int)n_user_upvalues);
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
        Ks_Type expected = (Ks_Type)lua_tointeger(L, -1);
        lua_pop(L, 1);

        int stack_idx = start_idx + (int)i;
        int actual_lua_type = lua_type(L, stack_idx);

        bool match = false;
        switch (expected) {
            case KS_TYPE_INT:
            case KS_TYPE_UINT:
            case KS_TYPE_CHAR:
                match = lua_isinteger(L, stack_idx) || lua_isnumber(L, stack_idx);
                break;
            case KS_TYPE_FLOAT:
            case KS_TYPE_DOUBLE:
                match = lua_isnumber(L, stack_idx);
                break;
            case KS_TYPE_CSTRING:
                match = lua_isstring(L, stack_idx);
                break;
            case KS_TYPE_BOOL:
                match = lua_isboolean(L, stack_idx);
                break;
            case KS_TYPE_SCRIPT_TABLE:
                match = lua_istable(L, stack_idx);
                break;
            case KS_TYPE_SCRIPT_FUNCTION:
                match = lua_isfunction(L, stack_idx);
                break;
            case KS_TYPE_USERDATA:
                match = lua_isuserdata(L, stack_idx);
                break;
            case KS_TYPE_PTR:
                match = lua_islightuserdata(L, stack_idx) || lua_isuserdata(L, stack_idx);
                break;
            case KS_TYPE_SCRIPT_ANY:
                match = true;
                break;
            default:
                match = false;
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

static int usertype_field_getter_thunk(lua_State* L) {
    auto* handle = static_cast<KsUsertypeInstanceHandle*>(lua_touserdata(L, 1));
    if (!handle || !handle->instance) return 0;

    ks_size offset = (ks_size)lua_tointeger(L, lua_upvalueindex(1));
    Ks_Type type = (Ks_Type)lua_tointeger(L, lua_upvalueindex(2));

    ks_byte* field_ptr = static_cast<ks_byte*>(handle->instance) + offset;

    switch (type) {
    case KS_TYPE_INT:     lua_pushinteger(L, *reinterpret_cast<ks_int*>(field_ptr)); break;
    case KS_TYPE_FLOAT:   lua_pushnumber(L, *reinterpret_cast<ks_float*>(field_ptr)); break;
    case KS_TYPE_DOUBLE:  lua_pushnumber(L, *reinterpret_cast<ks_double*>(field_ptr)); break;
    case KS_TYPE_BOOL:    lua_pushboolean(L, *reinterpret_cast<ks_bool*>(field_ptr)); break;
    case KS_TYPE_CSTRING: lua_pushstring(L, *reinterpret_cast<const char**>(field_ptr)); break; 

    case KS_TYPE_USERDATA: {
        const char* type_name = lua_tostring(L, lua_upvalueindex(3));

        auto* sub_handle = static_cast<KsUsertypeInstanceHandle*>(
            lua_newuserdatauv(L, sizeof(KsUsertypeInstanceHandle), 0)
            );

        sub_handle->instance = field_ptr;
        sub_handle->is_borrowed = true; 

        luaL_setmetatable(L, type_name);
        break;
    }
    default: lua_pushnil(L); break;
    }
    return 1;
}

static int usertype_field_setter_thunk(lua_State* L) {
    auto* handle = static_cast<KsUsertypeInstanceHandle*>(lua_touserdata(L, 1));
    if (!handle || !handle->instance) return luaL_error(L, "Invalid instance");

    ks_size offset = (ks_size)lua_tointeger(L, lua_upvalueindex(1));
    Ks_Type type = (Ks_Type)lua_tointeger(L, lua_upvalueindex(2));
    ks_byte* dst_ptr = static_cast<ks_byte*>(handle->instance) + offset;

    switch (type) {
    case KS_TYPE_INT:     *reinterpret_cast<ks_int*>(dst_ptr) = (ks_int)lua_tointeger(L, 2); break;
    case KS_TYPE_FLOAT:   *reinterpret_cast<ks_float*>(dst_ptr) = (ks_float)lua_tonumber(L, 2); break;
    case KS_TYPE_DOUBLE:  *reinterpret_cast<ks_double*>(dst_ptr) = (ks_double)lua_tonumber(L, 2); break;
    case KS_TYPE_BOOL:    *reinterpret_cast<ks_bool*>(dst_ptr) = (ks_bool)lua_toboolean(L, 2); break;

    case KS_TYPE_USERDATA: {
        const char* expected_type = lua_tostring(L, lua_upvalueindex(3));

        void* src_raw = luaL_checkudata(L, 2, expected_type);
        auto* src_handle = static_cast<KsUsertypeInstanceHandle*>(src_raw);

        if (!src_handle || !src_handle->instance) {
            return luaL_error(L, "Assignment source is null or invalid");
        }

        size_t size_to_copy = lua_rawlen(L, 2) - sizeof(KsUsertypeInstanceHandle);
        memcpy(dst_ptr, src_handle->instance, size_to_copy);
        break;
    }
    default: return luaL_error(L, "Unsupported field type for assignment");
    }
    return 0;
}

static std::vector<MethodInfo> convert_sigs(const Ks_Script_Sig_Def* sigs, size_t count, const char* name) {
    std::vector<MethodInfo> infos;
    infos.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        std::vector<Ks_Type> signature;
        if (sigs[i].args && sigs[i].num_args > 0) {
            signature.assign(sigs[i].args, sigs[i].args + sigs[i].num_args);
        }
        infos.push_back({ name, sigs[i].func, signature });
    }
    return infos;
}

static int enum_newindex_error(lua_State* L) {
    return luaL_error(L, "Attempt to modify a read-only enum table");
}