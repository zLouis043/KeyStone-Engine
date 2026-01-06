#include "../../include/event/events_binding.h"
#include "../../include/event/events_manager.h"
#include "../../include/script/script_engine.h"
#include "../../include/core/reflection.h"
#include "../../include/core/log.h"
#include "../../include/profiler/profiler.h"
#include "../../include/memory/memory.h"
#include <unordered_map>
#include <string>
#include <string.h>

#define BINDING_HANDLE_MASK 0x00FFFFFF

static std::vector<Ks_Script_Object> g_lua_layouts_by_id;

struct LuaSubInfo {
    Ks_Script_Ctx ctx;
    Ks_Script_Object func;
    bool is_lua_wrapper;
    const Ks_Type_Info* native_info;
};

void ks_register_lua_event_reflection(void) {
    if (!ks_reflection_get_type("LuaEvent")) {
        ks_reflect_struct(LuaEvent,
            ks_reflect_field(Ks_Script_Object, layout),
            ks_reflect_field(Ks_Script_Object, payload),
            ks_reflect_field(const char*, event_name)
        );
    }
}

static bool validate_payload(Ks_Script_Ctx ctx, Ks_Script_Object layout, Ks_Script_Object payload) {
    int lay_type = ks_script_obj_type(ctx, layout);
    int pay_type = ks_script_obj_type(ctx, payload);

    if (lay_type == KS_TYPE_NIL || lay_type == KS_TYPE_UNKNOWN) {
        if (pay_type != KS_TYPE_NIL && pay_type != KS_TYPE_UNKNOWN) {
            KS_LOG_ERROR("[Events] Signal expects no payload.");
            return false;
        }
        return true;
    }

    if (pay_type != KS_TYPE_SCRIPT_TABLE && pay_type != KS_TYPE_USERDATA) return false;

    Ks_Script_Table_Iterator it = ks_script_table_iterate(ctx, layout);
    Ks_Script_Object key_obj, type_obj;
    bool valid = true;

    while (ks_script_iterator_next(ctx, &it, &key_obj, &type_obj)) {
        Ks_Script_Object val = ks_script_table_get(ctx, payload, key_obj);
        int exp = (int)ks_script_obj_as_integer(ctx, type_obj);
        int act = ks_script_obj_type(ctx, val);

        if (exp != act) {
            bool nums = (exp == KS_TYPE_INT || exp == KS_TYPE_FLOAT) && (act == KS_TYPE_INT || act == KS_TYPE_FLOAT);
            if (!nums) { valid = false; break; }
        }
    }
    ks_script_iterator_destroy(ctx, &it);
    return valid;
}

static void lua_event_bridge(Ks_EventData data, void* user_data) {
    KS_PROFILE_SCOPE("LuaEventBridge");
    LuaSubInfo* info = (LuaSubInfo*)user_data;

    int args = 0;
    if (data) {
        if (info->is_lua_wrapper) {
            const LuaEvent* w = (const LuaEvent*)data;
            ks_script_stack_push_obj(info->ctx, w->payload);
            args = 1;
        }
        else if (info->native_info) {
            KS_PROFILE_SCOPE("NativeUserdataCreate");
            Ks_Script_Object ud = ks_script_create_usertype_instance(info->ctx, info->native_info->name);
            if (ks_script_obj_type(info->ctx, ud) == KS_TYPE_USERDATA) {
                void* ptr = ks_script_usertype_get_ptr(info->ctx, ud);
                if (ptr) {
                    memcpy(ptr, data, info->native_info->size);
                    ks_script_stack_push_obj(info->ctx, ud);
                    args = 1;
                }
                else {
                    KS_LOG_ERROR("[Events] Failed to get ptr for native event '%s'", info->native_info->name);
                }
            }
            else {
                KS_LOG_ERROR("[Events] Failed to create usertype instance for '%s'. Is it registered in Lua?", info->native_info->name);
            }
        }
    }
    {
        KS_PROFILE_SCOPE("LuaPCall");
        ks_script_func_call(info->ctx, info->func, args, 0);
    }
}

static void cleanup_sub_info(void* user_data) {
    LuaSubInfo* info = (LuaSubInfo*)user_data;
    if (info) {
        ks_script_free_obj(info->ctx, info->func);
        ks_dealloc(info);
    }
}

ks_returns_count events_register_lua(Ks_Script_Ctx ctx) {
    Ks_Script_Object up = ks_script_func_get_upvalue(ctx, 1);
    Ks_EventManager em = (Ks_EventManager)ks_script_lightuserdata_get_ptr(ctx, up);

    const char* name = ks_script_obj_as_string_view(ctx, ks_script_get_arg(ctx, 1));
    if (!name) { ks_script_stack_push_obj(ctx, ks_script_create_integer(ctx, (ks_int64)KS_INVALID_HANDLE)); return 1; }

    Ks_Script_Object layout = ks_script_get_arg(ctx, 2);
    Ks_Handle h = KS_INVALID_HANDLE;

    if (ks_script_obj_type(ctx, layout) != KS_TYPE_NIL) {
        ks_reflection_register_typedef("LuaEvent", name);
        h = ks_event_manager_register_type(em, name);
    }
    else {
        h = ks_event_manager_register_signal(em, name);
    }

    if (h == KS_INVALID_HANDLE) { ks_script_stack_push_obj(ctx, ks_script_create_integer(ctx, (ks_int64)KS_INVALID_HANDLE)); return 1; }

    uint32_t idx = (uint32_t)(h & BINDING_HANDLE_MASK);

    if (idx >= g_lua_layouts_by_id.size()) {
        g_lua_layouts_by_id.resize(idx + 16, { KS_TYPE_NIL });
    }

    g_lua_layouts_by_id[idx] = ks_script_ref_obj(ctx, layout);

    ks_script_stack_push_integer(ctx, (ks_int64)h);
    return 1;
}

ks_returns_count events_publish_lua(Ks_Script_Ctx ctx) {
    KS_PROFILE_SCOPE("EventsPublishLua");
    Ks_Script_Object up = ks_script_func_get_upvalue(ctx, 1);
    Ks_EventManager em = (Ks_EventManager)ks_script_lightuserdata_get_ptr(ctx, up);

    Ks_Script_Object arg1 = ks_script_get_arg(ctx, 1);
    Ks_Handle h = KS_INVALID_HANDLE;

    if (arg1.type == KS_TYPE_INT || arg1.type == KS_TYPE_FLOAT) {
        h = (Ks_Handle)ks_script_obj_as_integer(ctx, arg1);
    }
    else if (arg1.type == KS_TYPE_CSTRING) {
        const char* name = ks_script_obj_as_string_view(ctx, arg1);
        h = ks_event_manager_get_event_handle(em, name);
    }

    if (h == KS_INVALID_HANDLE) return 0;

    Ks_Script_Object payload = ks_script_get_arg(ctx, 2);

    uint32_t idx = (uint32_t)(h & BINDING_HANDLE_MASK);
    Ks_Script_Object layout = (idx < g_lua_layouts_by_id.size()) ? g_lua_layouts_by_id[idx] : Ks_Script_Object{ KS_TYPE_NIL };

    if (ks_script_obj_type(ctx, payload) == KS_TYPE_NIL) {
        ks_event_manager_emit(em, h);
    }
    else {
        LuaEvent w;
        w.layout = layout;
        w.payload = payload;
        w.event_name = "";
        {
            KS_PROFILE_SCOPE("ManagerDispatch");
            ks_event_manager_publish(em, h, &w);
        }
    }
    return 0;
}

ks_returns_count events_subscribe_lua(Ks_Script_Ctx ctx) {
    Ks_Script_Object up = ks_script_func_get_upvalue(ctx, 1);
    Ks_EventManager em = (Ks_EventManager)ks_script_lightuserdata_get_ptr(ctx, up);

    Ks_Script_Object arg1 = ks_script_get_arg(ctx, 1);
    Ks_Handle h = KS_INVALID_HANDLE;
    char* name_copy = nullptr;

    if (arg1.type == KS_TYPE_INT) {
        h = (Ks_Handle)ks_script_obj_as_integer(ctx, arg1);
    }
    else {
        const char* name = ks_script_obj_as_cstring(ctx, arg1);
        if (name) {
            h = ks_event_manager_get_event_handle(em, name);
            if (h != KS_INVALID_HANDLE) {
                size_t len = strlen(name);
                name_copy = (char*)ks_alloc(len + 1, KS_LT_USER_MANAGED, KS_TAG_SCRIPT);
                memcpy(name_copy, name, len+1);
            }
        }
    }

    if (h == KS_INVALID_HANDLE) {
        if (name_copy) ks_dealloc(name_copy);
        ks_script_stack_push_obj(ctx, ks_script_create_integer(ctx, (ks_int64)KS_INVALID_HANDLE));
        return 1;
    }

    uint32_t idx = (uint32_t)(h & BINDING_HANDLE_MASK);
    bool is_lua = (idx < g_lua_layouts_by_id.size() && g_lua_layouts_by_id[idx].type != KS_TYPE_NIL);

    LuaSubInfo* info = (LuaSubInfo*)ks_alloc(sizeof(LuaSubInfo), KS_LT_USER_MANAGED, KS_TAG_SCRIPT);
    info->ctx = ctx;
    info->func = ks_script_ref_obj(ctx, ks_script_get_arg(ctx, 2));
    info->is_lua_wrapper = is_lua;

    if (!is_lua) {
        const char* evt_name = ks_event_manager_get_event_name(em, h);
        if (evt_name) {
            info->native_info = ks_reflection_get_type(evt_name);
            if (!info->native_info) {
                KS_LOG_WARN("[Events] Subscribed to '%s' (Handle %u) but no Reflection info found.", evt_name, h);
            }
        }
        else {
            KS_LOG_ERROR("[Events] Invalid handle %u passed to subscribe.", h);
            info->native_info = nullptr;
        }
    }
    else {
        info->native_info = nullptr;
    }

    Ks_Handle sub_h = ks_event_manager_subscribe_ex(em, h, lua_event_bridge, info, [](void* d) {
        LuaSubInfo* i = (LuaSubInfo*)d; ks_script_free_obj(i->ctx, i->func); ks_dealloc(i);
        });

    if (name_copy) ks_dealloc(name_copy);

    ks_script_stack_push_integer(ctx, (ks_int64)sub_h);
    return 1;
}

ks_returns_count events_unsubscribe_lua(Ks_Script_Ctx ctx) {
    Ks_Script_Object up = ks_script_func_get_upvalue(ctx, 1);
    Ks_EventManager em = (Ks_EventManager)ks_script_lightuserdata_get_ptr(ctx, up);
    Ks_Handle h = (Ks_Handle)ks_script_obj_as_integer(ctx, ks_script_get_arg(ctx, 1));

    ks_event_manager_unsubscribe(em, h);
    return 0;
}

ks_returns_count events_get_handle_lua(Ks_Script_Ctx ctx) {
    Ks_Script_Object up = ks_script_func_get_upvalue(ctx, 1);
    Ks_EventManager em = (Ks_EventManager)ks_script_lightuserdata_get_ptr(ctx, up);

    const char* name = ks_script_obj_as_string_view(ctx, ks_script_get_arg(ctx, 1));

    if (!name) {
        ks_script_stack_push_integer(ctx, (ks_int64)KS_INVALID_HANDLE);
        return 1;
    }

    Ks_Handle h = ks_event_manager_get_event_handle(em, name);

    ks_script_stack_push_integer(ctx, (ks_int64)h);
    return 1;
}

void ks_event_manager_lua_bind(Ks_EventManager em, Ks_Script_Ctx ctx) {
    g_lua_layouts_by_id.clear();
    ks_register_lua_event_reflection();

    auto* b = ks_script_usertype_begin_from_ref(ctx, "LuaEvent");
    ks_script_usertype_end(b);

    Ks_Script_Object em_ptr = ks_script_create_lightuserdata(ctx, em);
    Ks_Script_Table tbl = ks_script_create_named_table(ctx, "events");

    auto reg = [&](const char* n, ks_script_cfunc f) {
        ks_script_stack_push_obj(ctx, em_ptr);
        Ks_Script_Function fn = ks_script_create_cfunc_with_upvalues(ctx, KS_SCRIPT_FUNC_VOID(f), 1);
        ks_script_table_set(ctx, tbl, ks_script_create_cstring(ctx, n), fn);
        };

    reg("register", events_register_lua);
    reg("register_signal", events_register_lua);
    reg("get_handle", events_get_handle_lua);
    reg("publish", events_publish_lua);
    reg("emit", events_publish_lua);
    reg("subscribe", events_subscribe_lua);
    reg("unsubscribe", events_unsubscribe_lua);
}