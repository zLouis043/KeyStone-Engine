#include "../../include/event/events_manager.h"
#include "../../include/event/events_binding.h"
#include "../../include/core/log.h"
#include "../../include/memory/memory.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>
#include <string.h>

struct EventArgument {
    Ks_Type type;
    union {
        ks_bool b_val;
        ks_char c_val;
        ks_int i_val;
        ks_uint16 ui_val;
        ks_float f_val;
        ks_double d_val;
        ks_ptr p_val;
        Ks_Script_Table t_val;

    };
    std::vector<uint8_t> buffer;
};

struct PayloadInternal {
    std::vector<EventArgument> args;
    ks_ptr script_ctx;
};

struct LuaSubInfo {
    Ks_Script_Ctx ctx;
    Ks_Script_Ref func_ref;
    Ks_EventManager em;
};

//static std::mutex s_subs_mutex;
//static std::unordered_map<ks_uint32, LuaSubInfo*> s_lua_subscriptions;

static Ks_EventManager get_em(Ks_Script_Ctx ctx) {
    Ks_Script_Object up = ks_script_func_get_upvalue(ctx, 1);
    return (Ks_EventManager)ks_script_lightuserdata_get_ptr(ctx, up);
}

ks_returns_count l_events_register(Ks_Script_Ctx ctx) {
    Ks_EventManager em = get_em(ctx);
    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    Ks_Script_Table types_tbl = ks_script_obj_as_table(ctx, ks_script_get_arg(ctx, 2));

    std::vector<Ks_Type> types;
    ks_size count = ks_script_table_array_size(ctx, types_tbl);

    for (size_t i = 0; i < count; ++i) {
        Ks_Script_Object key = ks_script_create_number(ctx, (double)(i + 1));
        Ks_Script_Object val = ks_script_table_get(ctx, types_tbl, key);
        int type_int = (int)ks_script_obj_as_number(ctx, val);
        types.push_back((Ks_Type)type_int);
    }

    Ks_Handle h = ks_event_manager_register_impl(em, name, types.data(), types.size());
    ks_script_stack_push_obj(ctx, ks_script_create_number(ctx, (double)h));
    return 1;
}

ks_returns_count l_events_get(Ks_Script_Ctx ctx) {
    Ks_EventManager em = get_em(ctx);
    ks_str name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    Ks_Handle h = ks_event_manager_get_event_handle(em, name);
    ks_script_stack_push_integer(ctx, (ks_int64)h);
    return 1;
}

ks_returns_count l_events_publish(Ks_Script_Ctx ctx) {
    Ks_EventManager em = get_em(ctx);
    ks_int64 h_val = ks_script_obj_as_integer(ctx, ks_script_get_arg(ctx, 1));
    Ks_Handle evt = (Ks_Handle)h_val;

    ks_size sig_count = 0;
    const Ks_Type* sig = ks_event_manager_get_signature(em, evt, &sig_count);

    if (!sig && sig_count > 0) return 0;

    std::vector<EventArgument> packed_args;
    packed_args.reserve(sig_count);

    for (size_t i = 0; i < sig_count; ++i) {
        Ks_Type expected = sig[i];
        Ks_Script_Object lua_arg = ks_script_get_arg(ctx, i + 2);

        EventArgument arg;
        arg.type = expected;

        switch (expected) {
        case KS_TYPE_INT:
            arg.i_val = (int)ks_script_obj_as_integer(ctx, lua_arg);
            break;
        case KS_TYPE_UINT:
            arg.ui_val = (ks_uint32)ks_script_obj_as_integer(ctx, lua_arg);
            break;
        case KS_TYPE_FLOAT:
            arg.f_val = (float)ks_script_obj_as_number(ctx, lua_arg);
            break;
        case KS_TYPE_DOUBLE:
            arg.d_val = (float)ks_script_obj_as_number(ctx, lua_arg);
            break;
        case KS_TYPE_BOOL:
            arg.b_val = ks_script_obj_as_boolean(ctx, lua_arg);
            break;
        case KS_TYPE_CSTRING: {
            ks_str s = ks_script_obj_as_cstring(ctx, lua_arg);
            if (s) {
                ks_size len = strlen(s) + 1;
                arg.buffer.resize(len);
                memcpy(arg.buffer.data(), s, len);
            }
        } break;
        case KS_TYPE_SCRIPT_TABLE:
            if (lua_arg.type == KS_TYPE_SCRIPT_TABLE) {
                arg.t_val = ks_script_obj_as_table(ctx, lua_arg);
            }
            else {
                arg.t_val = ks_script_create_invalid_obj(ctx);
            }
            break;
        case KS_TYPE_USERDATA:
            if (lua_arg.type == KS_TYPE_USERDATA) {
                Ks_UserData ud = ks_script_obj_as_userdata(ctx, lua_arg);
                arg.buffer.resize(ud.size);
                memcpy(arg.buffer.data(), ud.data, ud.size);
            }
            break;
        case KS_TYPE_PTR: {
            if (lua_arg.type == KS_TYPE_USERDATA) {
                arg.p_val = ks_script_userdata_get_ptr(ctx, lua_arg);
            }
            else if (lua_arg.type == KS_TYPE_LIGHTUSERDATA) {
                arg.p_val = lua_arg.val.lightuserdata;
            }
        } break;
        default: break;
        }
        packed_args.push_back(arg);
    }

    PayloadInternal pi;
    pi.args = packed_args;
    pi.script_ctx = ctx;
    ks_event_manager_publish_direct(em, evt, (Ks_Event_Payload)&pi);
    return 0;
}

ks_bool lua_subscriber_thunk(Ks_Event_Payload data, Ks_Payload user_data) {
    LuaSubInfo* info = (LuaSubInfo*)user_data.data;
    Ks_Script_Ctx ctx = info->ctx;

    Ks_Script_Object func_obj;
    func_obj.type = KS_TYPE_SCRIPT_FUNCTION;
    func_obj.val.function_ref = info->func_ref;
    func_obj.state = KS_SCRIPT_OBJECT_VALID;

    ks_size count = ks_event_get_args_count(data);

    for (ks_size i = 0; i < count; ++i) {
        Ks_Type type = ks_event_get_arg_type(data, i);
        switch (type) {
        case KS_TYPE_INT:
            ks_script_stack_push_integer(ctx, ks_event_get_int(data, i));
            break;
        case KS_TYPE_FLOAT:
        case KS_TYPE_DOUBLE:
            ks_script_stack_push_number(ctx, ks_event_get_float(data, i));
            break;
        case KS_TYPE_BOOL:
            ks_script_stack_push_boolean(ctx, ks_event_get_bool(data, i));
            break;
        case KS_TYPE_CSTRING:
            ks_script_stack_push_cstring(ctx, ks_event_get_cstring(data, i));
            break;
        case KS_TYPE_SCRIPT_TABLE:
            ks_script_stack_push_obj(ctx, ks_event_get_script_table(data, i));
            break;
        case KS_TYPE_USERDATA:
            ks_script_stack_push_obj(ctx, ks_script_create_lightuserdata(ctx, ks_event_get_ptr(data, i)));
            break;
        default:
            ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
            break;
        }
    }

    ks_script_func_call(ctx, func_obj, count, 0);

    return ks_true;
}

void lua_sub_info_free(ks_ptr ptr) {
    LuaSubInfo* info = (LuaSubInfo*)ptr;
    if (info) {
        Ks_Script_Object func_ref;
        func_ref.val.function_ref = info->func_ref;
        func_ref.type = KS_TYPE_SCRIPT_FUNCTION;
        func_ref.state = KS_SCRIPT_OBJECT_VALID;

        ks_script_free_obj(info->ctx, func_ref);
        ks_dealloc(info);
    }
}

ks_returns_count l_events_subscribe(Ks_Script_Ctx ctx) {
    Ks_EventManager em = get_em(ctx);
    double h_val = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    Ks_Script_Object func = ks_script_get_arg(ctx, 2);

    LuaSubInfo* info = (LuaSubInfo*)ks_alloc(sizeof(LuaSubInfo), KS_LT_USER_MANAGED, KS_TAG_SCRIPT);
    //LuaSubInfo info;
    info->ctx = ctx;
    info->em = em;

    ks_script_promote(ctx, func);
    info->func_ref = func.val.function_ref;

    Ks_Payload pd;
    pd.data = info;
    pd.size = sizeof(LuaSubInfo);
    pd.owns_data = true;
    pd.free_fn = lua_sub_info_free;

    Ks_Handle sub = ks_event_manager_subscribe(em, (Ks_Handle)h_val, lua_subscriber_thunk, pd);

    {
        //std::lock_guard<std::mutex> lock(s_subs_mutex);
        //s_lua_subscriptions[(ks_uint32)sub] = info;
    }

    ks_script_stack_push_obj(ctx, ks_script_create_integer(ctx, sub));
    return 1;
}

ks_returns_count l_events_unsubscribe(Ks_Script_Ctx ctx) {
    Ks_EventManager em = get_em(ctx);
    double h_val = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    Ks_Handle sub = (Ks_Handle)h_val;

    ks_event_manager_unsubscribe(em, sub);

/*
    {
        std::lock_guard<std::mutex> lock(s_subs_mutex);
        auto it = s_lua_subscriptions.find((ks_uint32)sub);
        if (it != s_lua_subscriptions.end()) {
            LuaSubInfo* info = it->second;

            Ks_Script_Object func_ref;
            func_ref.val.function_ref = info->func_ref;
            func_ref.state = KS_SCRIPT_OBJECT_VALID;
            func_ref.type = KS_TYPE_SCRIPT_FUNCTION;

            ks_script_free_obj(ctx, func_ref);

            ks_dealloc(info);
            s_lua_subscriptions.erase(it);
        }
    }
 */
    return 0;
}

KS_API ks_no_ret ks_event_manager_lua_bind(Ks_EventManager em, Ks_Script_Ctx ctx) {
    Ks_Script_Object em_obj = ks_script_create_lightuserdata(ctx, em);
    Ks_Script_Table tbl = ks_script_create_named_table(ctx, "events");

    auto reg = [&](const char* n, ks_script_cfunc f) {
        ks_script_stack_push_obj(ctx, em_obj);
        Ks_Script_Function fn_obj = ks_script_create_cfunc_with_upvalues(ctx, KS_SCRIPT_FUNC_VOID(f), 1);
        ks_script_table_set(ctx, tbl, ks_script_create_cstring(ctx, n), fn_obj);
    };

    reg("register", l_events_register);
    reg("subscribe", l_events_subscribe);
    reg("unsubscribe", l_events_unsubscribe);
    reg("publish", l_events_publish);
    reg("get", l_events_get);
}

KS_API ks_no_ret ks_event_manager_lua_shutdown(Ks_EventManager em) {
    /*
    std::lock_guard<std::mutex> lock(s_subs_mutex);

    auto it = s_lua_subscriptions.begin();
    while (it != s_lua_subscriptions.end()) {
        LuaSubInfo* info = it->second;

        if (info->em == em) {
            ks_event_manager_unsubscribe(em, (Ks_Handle)it->first);
            ks_dealloc(info);
            it = s_lua_subscriptions.erase(it);
        }
        else {
            ++it;
        }
    }
    */
}