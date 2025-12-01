#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>

#include "../../include/core/handle.h"
#include "../../include/core/log.h"

#define KS_HANDLE_TYPE_SHIFT 24
#define KS_HANDLE_INDEX_MASK 0x00FFFFFF

class HandleTypeRegistry {
public:
	static HandleTypeRegistry& get() {
		static HandleTypeRegistry instance;
		return instance;
	}

    ks_uint8 register_type(const char* name) {
        std::lock_guard<std::mutex> lock(mtx);

        for (const auto& pair : type_map) {
            if (pair.second == name) return pair.first;
        }

        if (next_type_id > 255) {
            KS_LOG_ERROR("Max handle types limit reached (255)!");
            return KS_INVALID_ID;
        }

        ks_uint8 id = (ks_uint8)next_type_id++;
        type_map[id] = name;

        if (counters.size() <= id) {
            counters.resize(id + 1, 1);
        }

        return id;
    }

    ks_uint32 make_handle(ks_uint8 id) {
        std::lock_guard<std::mutex> lock(mtx);

        if (id >= counters.size()) return KS_INVALID_HANDLE;

        ks_uint32 index = counters[id];

        if (index > KS_HANDLE_INDEX_MASK) {
            KS_LOG_ERROR("Handle index overflow for type %d!", id);
            return KS_INVALID_HANDLE;
        }

        counters[id]++;

        return ((ks_uint32)id << KS_HANDLE_TYPE_SHIFT) | (index & KS_HANDLE_INDEX_MASK);
    }

    const char* get_name(ks_uint8 id) {
        std::lock_guard<std::mutex> lock(mtx);
        auto type = type_map.find(id);
        if (type == type_map.end()) return nullptr;
        return type->second.c_str();
    }

    ks_uint8 get_id(ks_str name) {
        for (const auto& pair : type_map) {
            if (pair.second == name) return pair.first;
        }
        return KS_INVALID_ID;
    }

private:
	HandleTypeRegistry() = default;
	std::mutex mtx;
	int next_type_id = 1;
    std::unordered_map<ks_uint8, std::string> type_map;
    std::vector<ks_uint32> counters;
};

KS_API Ks_Handle_Id ks_handle_register(ks_str handle_type) {
    return HandleTypeRegistry::get().register_type(handle_type);
}

KS_API Ks_Handle_Id ks_handle_get_id(ks_str handle_type) {
    return HandleTypeRegistry::get().get_id(handle_type);
}

KS_API ks_str ks_handle_get_id_name(Ks_Handle_Id id) {
    return HandleTypeRegistry::get().get_name(id);
}

KS_API Ks_Handle ks_handle_make(Ks_Handle_Id id) {
    return HandleTypeRegistry::get().make_handle(id);
}

KS_API ks_bool ks_handle_is_type(Ks_Handle handle, Ks_Handle_Id type_id) {
    ks_uint8 type = (handle >> 24) & 0xFF;
    return type == type_id;
}