#include "../../include/event/events_manager.h"
#include "../../include/core/reflection.h"
#include "../../include/core/handle.h"
#include "../../include/core/log.h"
#include "../../include/profiler/profiler.h"
#include "../../include/memory/memory.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <shared_mutex>
#include <mutex>

#define KS_HANDLE_INDEX_MASK 0x00FFFFFF

struct EventSubscriber {
    Ks_Handle sub_id;
    Ks_EventCallback callback;
    void* user_data;
    Ks_UserDataFreeCallback free_cb;
};

struct EventTypeData {
    std::string name;
    const Ks_Type_Info* type_info;
    std::vector<EventSubscriber> subscribers;
};

struct Ks_EventManager_Impl {
    std::mutex mutex;
    std::vector<EventTypeData*> event_types;
    std::unordered_map<std::string, uint32_t> name_to_id;
    std::unordered_map<Ks_Handle, uint32_t> sub_to_event_idx;
    Ks_Handle_Id h_type_event_def;
    Ks_Handle_Id h_type_sub;
};

static void ensure_signal_reflection() {
    if (!ks_reflection_get_type("Ks_Signal")) {
        ks_reflect_struct(Ks_Signal, ks_reflect_field(char, _unused));
    }
}

static uint32_t get_index_from_handle(Ks_Handle h) {
    return (h & KS_HANDLE_INDEX_MASK);
}

KS_API Ks_EventManager ks_event_manager_create() {
    auto* impl = new(ks_alloc_debug(sizeof(Ks_EventManager_Impl), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA, "KsEventManagerImpl")) Ks_EventManager_Impl();
    impl->h_type_event_def = ks_handle_register("EventType");
    impl->h_type_sub = ks_handle_register("EventSub");
    ensure_signal_reflection();
    impl->event_types.reserve(64);
    return (Ks_EventManager)impl;
}

static void cleanup_subscriber(EventSubscriber& sub) {
    if (sub.free_cb && sub.user_data) {
        sub.free_cb(sub.user_data);
        sub.user_data = nullptr;
    }
}

KS_API void ks_event_manager_destroy(Ks_EventManager em) {
    if (!em) return;
    auto* impl = (Ks_EventManager_Impl*)em;

    for (auto* type_data : impl->event_types) {
        if (!type_data) continue;
        for (auto& sub : type_data->subscribers) {
            cleanup_subscriber(sub);
        }
        type_data->~EventTypeData();
        ks_dealloc(type_data);
    }
    impl->~Ks_EventManager_Impl();
    ks_dealloc(impl);
}

KS_API Ks_Handle ks_event_manager_register_type(Ks_EventManager em, const char* type_name) {
    auto* impl = (Ks_EventManager_Impl*)em;
    if (!type_name) return KS_INVALID_HANDLE;

    std::lock_guard<std::mutex> lock(impl->mutex);

    auto it = impl->name_to_id.find(type_name);
    if (it != impl->name_to_id.end()) {
        return ((Ks_Handle)impl->h_type_event_def << 24) | (it->second & KS_HANDLE_INDEX_MASK);
    }

    const Ks_Type_Info* info = ks_reflection_get_type(type_name);
    if (!info) {
        KS_LOG_ERROR("Cannot register event '%s': Type not reflected", type_name);
        return KS_INVALID_HANDLE;
    }

    Ks_Handle new_handle = ks_handle_make(impl->h_type_event_def);
    if (new_handle == KS_INVALID_HANDLE) return KS_INVALID_HANDLE;

    uint32_t vector_idx = get_index_from_handle(new_handle);

    EventTypeData* data = new(ks_alloc_debug(sizeof(EventTypeData), KS_LT_USER_MANAGED, KS_TAG_RESOURCE, "EventTypeData")) EventTypeData();
    data->name = type_name;
    data->type_info = info;

    if (vector_idx >= impl->event_types.size()) {
        impl->event_types.resize(vector_idx + 1, nullptr);
    }
    impl->event_types[vector_idx] = data;

    impl->name_to_id[type_name] = vector_idx;

    return new_handle;
}

KS_API Ks_Handle ks_event_manager_register_signal(Ks_EventManager em, const char* signal_name) {
    ensure_signal_reflection();
    ks_reflection_register_typedef("Ks_Signal", signal_name);
    return ks_event_manager_register_type(em, signal_name);
}

KS_API Ks_Handle ks_event_manager_subscribe(Ks_EventManager em, Ks_Handle event_handle, Ks_EventCallback callback, void* user_data){
    return ks_event_manager_subscribe_ex(em, event_handle, callback, user_data, nullptr);
}

KS_API Ks_Handle ks_event_manager_get_event_handle(Ks_EventManager em, ks_str name)
{
    auto* impl = (Ks_EventManager_Impl*)em;
    std::lock_guard<std::mutex> lock(impl->mutex);

    auto it = impl->name_to_id.find(name);
    if (it != impl->name_to_id.end()) {
        return ((Ks_Handle)impl->h_type_event_def << 24) | (it->second & KS_HANDLE_INDEX_MASK);
    }
    return KS_INVALID_HANDLE;
}

const char* ks_event_manager_get_event_name(Ks_EventManager em, Ks_Handle event_handle)
{
    auto* impl = (Ks_EventManager_Impl*)em;
    uint32_t idx = get_index_from_handle(event_handle);

    std::lock_guard<std::mutex> lock(impl->mutex);

    if (idx >= impl->event_types.size() || !impl->event_types[idx]) {
        return nullptr;
    }

    return impl->event_types[idx]->name.c_str();
}

Ks_Handle ks_event_manager_subscribe_ex(Ks_EventManager em, Ks_Handle event_handle, Ks_EventCallback callback, void* user_data, Ks_UserDataFreeCallback free_cb) {
    auto* impl = (Ks_EventManager_Impl*)em;

    if (!ks_handle_is_type(event_handle, impl->h_type_event_def)) {
        KS_LOG_ERROR("Subscribe failed: Invalid event handle type");
        return KS_INVALID_HANDLE;
    }

    uint32_t evt_idx = get_index_from_handle(event_handle);

    std::lock_guard<std::mutex> lock(impl->mutex);

    if (evt_idx >= impl->event_types.size() || !impl->event_types[evt_idx]) {
        return KS_INVALID_HANDLE;
    }

    EventTypeData* data = impl->event_types[evt_idx];

    Ks_Handle sub_h = ks_handle_make(impl->h_type_sub);

    EventSubscriber sub;
    sub.sub_id = sub_h;
    sub.callback = callback;
    sub.user_data = user_data;
    sub.free_cb = free_cb;

    data->subscribers.push_back(sub);
    impl->sub_to_event_idx[sub_h] = evt_idx;

    return sub_h;
}

KS_API void ks_event_manager_unsubscribe(Ks_EventManager em, Ks_Handle sub_handle) {
    auto* impl = (Ks_EventManager_Impl*)em;

    if (!ks_handle_is_type(sub_handle, impl->h_type_sub)) return;

    std::lock_guard<std::mutex> lock(impl->mutex);

    auto map_it = impl->sub_to_event_idx.find(sub_handle);
    if (map_it == impl->sub_to_event_idx.end()) return;

    uint32_t evt_idx = map_it->second;
    if (evt_idx >= impl->event_types.size() || !impl->event_types[evt_idx]) return;

    EventTypeData* data = impl->event_types[evt_idx];
    auto& subs = data->subscribers;

    for (auto it = subs.begin(); it != subs.end(); ++it) {
        if (it->sub_id == sub_handle) {
            cleanup_subscriber(*it);
            subs.erase(it);
            break;
        }
    }
    impl->sub_to_event_idx.erase(map_it);
}

KS_API void ks_event_manager_publish(Ks_EventManager em, Ks_Handle event_handle, const void* data_ptr) {
    KS_PROFILE_SCOPE("EventManager::Publish");
    auto* impl = (Ks_EventManager_Impl*)em;

    uint32_t idx = event_handle & KS_HANDLE_INDEX_MASK;

    std::vector<EventSubscriber> safe_subs;
    {
        KS_PROFILE_SCOPE("EventManager::Lock&Copy");
        std::lock_guard<std::mutex> lock(impl->mutex);
        if (idx >= impl->event_types.size() || !impl->event_types[idx]) return;
        safe_subs = impl->event_types[idx]->subscribers;
    }
    {
        KS_PROFILE_SCOPE("EventManager::Callbacks");
        for (const auto& sub : safe_subs) {
            sub.callback(data_ptr, sub.user_data);
        }
    }
}

KS_API void ks_event_manager_emit(Ks_EventManager em, Ks_Handle signal_handle){
    ks_event_manager_publish(em, signal_handle, nullptr);
}