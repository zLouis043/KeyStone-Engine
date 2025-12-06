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

struct EventDefinition {
    std::string name;
    std::vector<Ks_Type> signature;
};

struct Subscriber {
    Ks_Handle sub_handle;
    ks_event_callback callback;
    ks_ptr user_data;
};

class EventManager_Impl {
public:
    EventManager_Impl() {
        event_type_id = ks_handle_register("Event");
        sub_type_id = ks_handle_register("Subscription");
    }

    ~EventManager_Impl() {
        definitions.clear();
        subscribers.clear();
        name_to_handle.clear();
    }

    Ks_Handle register_event(const char* name, const Ks_Type* types, size_t count) {
        std::lock_guard<std::mutex> lock(mtx);
        std::string s_name = name;

        if (name_to_handle.find(s_name) != name_to_handle.end()) {
            return name_to_handle[s_name];
        }

        Ks_Handle handle = ks_handle_make(event_type_id);
        name_to_handle[s_name] = handle;

        EventDefinition def;
        def.name = s_name;
        if (types && count > 0) {
            def.signature.assign(types, types + count);
        }
        definitions[handle] = def;

        return handle;
    }

    Ks_Handle get_handle(const char* name) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = name_to_handle.find(name);
        if (it == name_to_handle.end()) return KS_INVALID_HANDLE;
        return it->second;
    }

    Ks_Handle subscribe(Ks_Handle evt, ks_event_callback cb, ks_ptr user_data) {
        std::lock_guard<std::mutex> lock(mtx);
        if (definitions.find(evt) == definitions.end()) return KS_INVALID_HANDLE;

        Ks_Handle sub_h = ks_handle_make(sub_type_id);
        subscribers[evt].push_back({ sub_h, cb, user_data });
        return sub_h;
    }

    void unsubscribe(Ks_Handle sub) {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto& [evt, subs] : subscribers) {
            auto it = std::remove_if(subs.begin(), subs.end(),
                [sub](const Subscriber& s) { return s.sub_handle == sub; });
            if (it != subs.end()) {
                subs.erase(it, subs.end());
                return;
            }
        }
    }

    void dispatch_payload(Ks_Handle evt, PayloadInternal& payload) {
        std::vector<Subscriber> subs_copy;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (subscribers.find(evt) != subscribers.end()) {
                subs_copy = subscribers[evt];
            }
        }
        for (const auto& sub : subs_copy) {
            sub.callback(&payload, sub.user_data);
        }
    }

    void publish_variadic(Ks_Handle evt, va_list args) {
        if (!ks_handle_is_type(evt, event_type_id)) {
            KS_LOG_ERROR("Invalid event handle passed to publish");
            return;
        }

        std::vector<Ks_Type> signature;
        std::vector<Subscriber> current_subs;

        {
            std::lock_guard<std::mutex> lock(mtx);
            auto def_it = definitions.find(evt);
            if (def_it == definitions.end()) return;
            signature = def_it->second.signature;

            auto sub_it = subscribers.find(evt);
            if (sub_it != subscribers.end()) {
                current_subs = sub_it->second;
            }
        }

        if (current_subs.empty()) return;

        PayloadInternal payload;
        payload.script_ctx = nullptr;

        for (Ks_Type type : signature) {
            EventArgument arg;
            arg.type = type;

            switch (type) {
            case KS_TYPE_BOOL:
                arg.b_val = (ks_int)va_arg(args, ks_int);
                break;
            case KS_TYPE_CHAR:
                arg.c_val = (ks_int)va_arg(args, ks_int);
                break;
            case KS_TYPE_INT:
                arg.i_val = (ks_int)va_arg(args, ks_int);
                break;
            case KS_TYPE_UINT:
                arg.ui_val = (ks_uint32)va_arg(args, ks_uint32);
                break;
            case KS_TYPE_FLOAT:
                arg.f_val = (float)va_arg(args, ks_double);
                break;
            case KS_TYPE_DOUBLE:
                arg.d_val = (ks_double)va_arg(args, ks_double);
                break;
            case KS_TYPE_CSTRING: {
                ks_str s = (ks_str)va_arg(args, ks_str);
                if (s) {
                    size_t len = strlen(s) + 1;
                    arg.buffer.resize(len);
                    memcpy(arg.buffer.data(), s, len);
                }
            } break;
            case KS_TYPE_LSTRING: {
                Ks_LStr s = (Ks_LStr)va_arg(args, Ks_LStr);
                if (s.data) {
                    arg.buffer.resize(s.len);
                    memcpy(arg.buffer.data(), s.data, s.len);
                }
            } break;
            case KS_TYPE_PTR: {
                ks_ptr p = (ks_ptr)va_arg(args, ks_ptr);
                arg.p_val = p;
            } break;
            case KS_TYPE_USERDATA: {
                Ks_UserData ud = (Ks_UserData)va_arg(args, Ks_UserData);
                if (ud.data && ud.size > 0) {
                    arg.buffer.resize(ud.size);
                    memcpy(arg.buffer.data(), ud.data, ud.size);
                }
            } break;
            case KS_TYPE_SCRIPT_TABLE: {
                Ks_Script_Table tbl = va_arg(args, Ks_Script_Table);
                arg.t_val = tbl;
            } break;
            default: break;
            }
            payload.args.push_back(arg);
        }
        
        dispatch_payload(evt, payload);
    }

    void publish_direct(Ks_Handle evt, PayloadInternal& payload) {
        std::vector<Subscriber> current_subs;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (subscribers.find(evt) != subscribers.end()) {
                current_subs = subscribers[evt];
            }
        }
        
        dispatch_payload(evt, payload);
    }

    const std::vector<Ks_Type>* get_signature(Ks_Handle evt) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = definitions.find(evt);
        if (it != definitions.end()) return &it->second.signature;
        return nullptr;
    }

private:
    Ks_Handle_Id event_type_id;
    Ks_Handle_Id sub_type_id;
    std::mutex mtx;

    std::unordered_map<std::string, Ks_Handle> name_to_handle;
    std::unordered_map<Ks_Handle, EventDefinition> definitions;
    std::unordered_map<Ks_Handle, std::vector<Subscriber>> subscribers;
};

KS_API Ks_EventManager ks_event_manager_create() {
    return new (ks_alloc(sizeof(EventManager_Impl), KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA)) EventManager_Impl();
}

KS_API ks_no_ret ks_event_manager_destroy(Ks_EventManager em) {
    if (em) {
        static_cast<EventManager_Impl*>(em)->~EventManager_Impl();
        ks_dealloc(em);
    }
}

KS_API Ks_Handle ks_event_manager_register_impl(Ks_EventManager em, ks_str name, const Ks_Type* types, ks_size count) {
    return static_cast<EventManager_Impl*>(em)->register_event(name, types, count);
}

KS_API Ks_Handle ks_event_manager_get_event_handle(Ks_EventManager em, ks_str name) {
    return static_cast<EventManager_Impl*>(em)->get_handle(name);
}

KS_API const Ks_Type* ks_event_manager_get_signature(Ks_EventManager em, Ks_Handle event, ks_size* out_count) {
    auto* sig = static_cast<EventManager_Impl*>(em)->get_signature(event);
    if (sig) {
        if (out_count) *out_count = sig->size();
        return sig->data();
    }
    if (out_count) *out_count = 0;
    return nullptr;
}

KS_API Ks_Handle ks_event_manager_subscribe(Ks_EventManager em, Ks_Handle event, ks_event_callback callback, ks_ptr user_data) {
    return static_cast<EventManager_Impl*>(em)->subscribe(event, callback, user_data);
}

KS_API ks_no_ret ks_event_manager_unsubscribe(Ks_EventManager em, Ks_Handle sub) {
    static_cast<EventManager_Impl*>(em)->unsubscribe(sub);
}

KS_API ks_no_ret ks_event_manager_publish(Ks_EventManager em, Ks_Handle event, ...) {
    va_list args;
    va_start(args, event);
    static_cast<EventManager_Impl*>(em)->publish_variadic(event, args);
    va_end(args);
}


ks_no_ret ks_event_manager_publish_direct(Ks_EventManager em, Ks_Handle event, Ks_Event_Payload pi) {
    static_cast<EventManager_Impl*>(em)->publish_direct(event, *(PayloadInternal*)pi);
}

#define GET_PAYLOAD(p) ((PayloadInternal*)p)
#define CHECK_IDX(idx, p) if (idx >= p->args.size())

KS_API ks_size ks_event_get_args_count(Ks_Event_Payload data) {
    auto* p = GET_PAYLOAD(data);
    return p->args.size();
}

KS_API Ks_Type ks_event_get_arg_type(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size()) return p->args[index].type;
    return KS_TYPE_UNKNOWN;
}

KS_API ks_bool ks_event_get_bool(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_BOOL) return p->args[index].b_val;
    return false;
}

KS_API ks_char ks_event_get_char(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_CHAR) return p->args[index].c_val;
    return 0;
}

KS_API ks_int ks_event_get_int(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_INT) return p->args[index].i_val;
    return 0;
}

KS_API ks_float ks_event_get_float(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_FLOAT) return p->args[index].f_val;
    return 0.0f;
}

KS_API ks_double ks_event_get_double(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_DOUBLE) return p->args[index].d_val;
    return 0.0f;
}

KS_API ks_str ks_event_get_cstring(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_CSTRING)
        return (const char*)p->args[index].buffer.data();
    return "";
}

KS_API Ks_LStr ks_event_get_lstring(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_LSTRING){
        Ks_LStr str;
        str.data = (ks_str)p->args[index].buffer.data();
        str.len = p->args[index].buffer.size();
        return str;
    }
    return Ks_LStr{0};
}

KS_API ks_ptr ks_event_get_ptr(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_PTR)
        return p->args[index].p_val;
    return nullptr;
}

KS_API Ks_UserData ks_event_get_userdata(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_USERDATA) {
        Ks_UserData ud;
        ud.data = (ks_ptr)p->args[index].buffer.data();
        ud.size = p->args[index].buffer.size();
        return ud;
    }
    return Ks_UserData{0};
}

KS_API Ks_Script_Table ks_event_get_script_table(Ks_Event_Payload data, ks_size index) {
    auto* p = GET_PAYLOAD(data);
    if (index < p->args.size() && p->args[index].type == KS_TYPE_SCRIPT_TABLE){
        return p->args[index].t_val;
    }
    return Ks_Script_Table{0};
}

KS_API Ks_Script_Ctx ks_event_get_script_ctx(Ks_Event_Payload data) {
    auto* p = GET_PAYLOAD(data);
    return p->script_ctx;
}

