#include "../../include/state/state_manager.h"
#include "../../include/state/state.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstring>

struct StateBlock {
    std::string name;
    Ks_Type type;

    union {
        ks_int64 i_val;
        double   d_val;
        bool     b_val;
    };

    std::string s_val;
    std::vector<uint8_t> user_data_storage;
    std::string user_type_name;
};

class StateManager_Impl {
public:
    StateManager_Impl() {
        h_id = ks_handle_register("State");
    }

    ~StateManager_Impl() {
        states.clear();
        handles.clear();
    }

    template <typename T>
    Ks_Handle create_or_update(const char* name, Ks_Type type, T val) {
        std::lock_guard<std::mutex> lock(mtx);

        auto it = handles.find(name);
        if (it != handles.end()) {
            StateBlock& block = states[it->second];
            if (block.type != type) {
                return KS_INVALID_HANDLE;
            }
            set_value_internal(block, val);
            return it->second;
        }

        Ks_Handle h = ks_handle_make(h_id);
        StateBlock block;
        block.name = name;
        block.type = type;

        memset(&block.i_val, 0, sizeof(block.i_val));


        set_value_internal(block, val);

        states[h] = block;
        handles[name] = h;
        return h;
    }

    Ks_Handle create_or_update_usertype(const char* name, Ks_UserData ud, const char* type_name) {
        std::lock_guard<std::mutex> lock(mtx);

        auto it = handles.find(name);
        if (it != handles.end()) {
            StateBlock& block = states[it->second];

            if (block.type != KS_TYPE_USERDATA) {
                return KS_INVALID_HANDLE;
            }
            if (block.user_type_name != type_name) {
                return KS_INVALID_HANDLE;
            }
 
            if (block.user_data_storage.size() != ud.size) block.user_data_storage.resize(ud.size);
            if (ud.data) memcpy(block.user_data_storage.data(), ud.data, ud.size);

            return it->second;
        }

        Ks_Handle h = ks_handle_make(h_id);
        StateBlock block;
        block.name = name;
        block.type = KS_TYPE_USERDATA;
        block.user_type_name = type_name;

        block.user_data_storage.resize(ud.size);
        if (ud.data) memcpy(block.user_data_storage.data(), ud.data, ud.size);

        states[h] = block;
        handles[name] = h;
        return h;
    }

    template <typename T>
    bool set_value(Ks_Handle h, Ks_Type expected, T val) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = states.find(h);

        if (it == states.end()) {
            return false;
        }

        if (it->second.type != expected) {
            return false;
        }

        set_value_internal(it->second, val);
        return true;
    }

    bool set_usertype(Ks_Handle h, Ks_UserData ud, const char* type_name) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = states.find(h);

        if (it == states.end()) return false;

        StateBlock& block = it->second;

        if (block.type != KS_TYPE_USERDATA) {
            return false;
        }

        if (block.user_type_name != type_name) {

            return false;
        }

        if (block.user_data_storage.size() != ud.size) {
            return false;
        }

        if (ud.data) {
            memcpy(block.user_data_storage.data(), ud.data, ud.size);
        }
        return true;
    }

    Ks_Handle get_handle(const char* name) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = handles.find(name);
        return (it != handles.end()) ? it->second : KS_INVALID_HANDLE;
    }

    bool has(const char* name) {
        std::lock_guard<std::mutex> lock(mtx);
        return handles.find(name) != handles.end();
    }

    bool has(Ks_Handle handle) {
        std::lock_guard<std::mutex> lock(mtx);
        return states.find(handle) != states.end();
    }

    Ks_Type get_type(Ks_Handle h) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = states.find(h);
        return (it != states.end()) ? it->second.type : KS_TYPE_UNKNOWN;
    }

    template <typename T>
    T get_val_or(Ks_Handle h, Ks_Type expected_type, T def) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = states.find(h);
        if (it == states.end() || it->second.type != expected_type) return def;

        if constexpr (std::is_same_v<T, ks_int64>) return it->second.i_val;
        else if constexpr (std::is_same_v<T, double>) return it->second.d_val;
        else if constexpr (std::is_same_v<T, bool>) return it->second.b_val;
        else if constexpr (std::is_same_v<T, ks_bool>) return (T)it->second.b_val;
        else if constexpr (std::is_same_v<T, const char*>) return it->second.s_val.c_str();
        else return def;
    }

    void* get_usertype_info(Ks_Handle h, const char** out_name, size_t* out_size) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = states.find(h);
        if (it != states.end() && it->second.type == KS_TYPE_USERDATA) {
            if (out_name) *out_name = it->second.user_type_name.c_str();
            if (out_size) *out_size = it->second.user_data_storage.size();
            return it->second.user_data_storage.data();
        }
        return nullptr;
    }

private:
    template <typename T>
    void set_value_internal(StateBlock& b, T val) {
        if constexpr (std::is_same_v<T, ks_int64>) b.i_val = val;
        else if constexpr (std::is_same_v<T, double>) b.d_val = val;
        else if constexpr (std::is_same_v<T, bool>) b.b_val = val;
        else if constexpr (std::is_same_v<T, ks_bool>) b.b_val = (bool)val;
        else if constexpr (std::is_same_v<T, const char*>) b.s_val = val;
    }

    Ks_Handle_Id h_id;
    std::unordered_map<Ks_Handle, StateBlock> states;
    std::unordered_map<std::string, Ks_Handle> handles;
    std::mutex mtx;
};

KS_API Ks_StateManager ks_state_manager_create() {
    return new (ks_alloc(sizeof(StateManager_Impl), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA)) StateManager_Impl();
}

KS_API ks_no_ret ks_state_manager_destroy(Ks_StateManager sm) {
    if (sm) {
        static_cast<StateManager_Impl*>(sm)->~StateManager_Impl();
        ks_dealloc(sm);
    }
}

KS_API Ks_Handle ks_state_manager_new_int(Ks_StateManager sm, ks_str name, ks_int64 value) {
    return ((StateManager_Impl*)sm)->create_or_update(name, KS_TYPE_INT, value);
}

KS_API Ks_Handle ks_state_manager_new_float(Ks_StateManager sm, ks_str name, ks_double value) {
    return ((StateManager_Impl*)sm)->create_or_update(name, KS_TYPE_DOUBLE, value);
}

KS_API Ks_Handle ks_state_manager_new_bool(Ks_StateManager sm, ks_str name, ks_bool value) {
    return ((StateManager_Impl*)sm)->create_or_update(name, KS_TYPE_BOOL, (bool)value);
}

KS_API Ks_Handle ks_state_manager_new_string(Ks_StateManager sm, ks_str name, ks_str value) {
    return ((StateManager_Impl*)sm)->create_or_update(name, KS_TYPE_CSTRING, value);
}

KS_API Ks_Handle ks_state_manager_new_usertype(Ks_StateManager sm, ks_str name, Ks_UserData ud, ks_str type_name) {
    return ((StateManager_Impl*)sm)->create_or_update_usertype(name, ud, type_name);
}

KS_API Ks_Handle ks_state_manager_get_handle(Ks_StateManager sm, ks_str name) {
    return ((StateManager_Impl*)sm)->get_handle(name);
}

KS_API ks_bool ks_state_manager_has(Ks_StateManager sm, ks_str name) {
    return ((StateManager_Impl*)sm)->has(name);
}

KS_API Ks_Type ks_state_get_type(Ks_StateManager sm, Ks_Handle handle) {
    return ((StateManager_Impl*)sm)->get_type(handle);
}

KS_API ks_int64 ks_state_get_int(Ks_StateManager sm, Ks_Handle handle) {
    return ((StateManager_Impl*)sm)->get_val_or(handle, KS_TYPE_INT, (ks_int64)0);
}

KS_API ks_double ks_state_get_float(Ks_StateManager sm, Ks_Handle handle) {
    return ((StateManager_Impl*)sm)->get_val_or(handle, KS_TYPE_DOUBLE, 0.0);
}

KS_API ks_bool ks_state_get_bool(Ks_StateManager sm, Ks_Handle handle) {
    return ((StateManager_Impl*)sm)->get_val_or(handle, KS_TYPE_BOOL, (bool)false);
}

KS_API ks_str ks_state_get_string(Ks_StateManager sm, Ks_Handle handle) {
    return ((StateManager_Impl*)sm)->get_val_or(handle, KS_TYPE_CSTRING, (ks_str)nullptr);
}

KS_API ks_ptr ks_state_get_ptr(Ks_StateManager sm, Ks_Handle handle) {
    return ((StateManager_Impl*)sm)->get_usertype_info(handle, nullptr, nullptr);
}

KS_API ks_int64 ks_state_get_int_or(Ks_StateManager sm, Ks_Handle handle, ks_int64 def) {
    return ((StateManager_Impl*)sm)->get_val_or(handle, KS_TYPE_INT, def);
}
KS_API ks_double ks_state_get_float_or(Ks_StateManager sm, Ks_Handle handle, ks_double def) {
    return ((StateManager_Impl*)sm)->get_val_or(handle, KS_TYPE_DOUBLE, def);
}
KS_API ks_bool ks_state_get_bool_or(Ks_StateManager sm, Ks_Handle handle, ks_bool def) {
    return ((StateManager_Impl*)sm)->get_val_or(handle, KS_TYPE_BOOL, (bool)def);
}
KS_API ks_str ks_state_get_string_or(Ks_StateManager sm, Ks_Handle handle, ks_str def) {
    return ((StateManager_Impl*)sm)->get_val_or(handle, KS_TYPE_CSTRING, def);
}
KS_API ks_ptr ks_state_get_ptr_or(Ks_StateManager sm, Ks_Handle handle, ks_ptr def) {
    void* ptr = ((StateManager_Impl*)sm)->get_usertype_info(handle, nullptr, nullptr);
    return ptr ? ptr : def;
}

KS_API ks_ptr ks_state_get_usertype_info(Ks_StateManager sm, Ks_Handle handle, ks_str* out_type_name, ks_size* out_size) {
    return ((StateManager_Impl*)sm)->get_usertype_info(handle, out_type_name, out_size);
}

KS_API ks_bool ks_state_set_int(Ks_StateManager sm, Ks_Handle handle, ks_int64 value) {
    return ((StateManager_Impl*)sm)->set_value(handle, KS_TYPE_INT, value);
}

KS_API ks_bool ks_state_set_float(Ks_StateManager sm, Ks_Handle handle, ks_double value) {
    return ((StateManager_Impl*)sm)->set_value(handle, KS_TYPE_DOUBLE, value);
}

KS_API ks_bool ks_state_set_bool(Ks_StateManager sm, Ks_Handle handle, ks_bool value) {
    return ((StateManager_Impl*)sm)->set_value(handle, KS_TYPE_BOOL, (bool)value);
}

KS_API ks_bool ks_state_set_string(Ks_StateManager sm, Ks_Handle handle, ks_str value) {
    return ((StateManager_Impl*)sm)->set_value(handle, KS_TYPE_CSTRING, value);
}

KS_API ks_bool ks_state_set_usertype(Ks_StateManager sm, Ks_Handle handle, Ks_UserData ud, ks_str type_name) {
    return ((StateManager_Impl*)sm)->set_usertype(handle, ud, type_name);
}