/**
 * @file events_manager.h
 * @brief Publish/Subscribe event system.
 * Supports typed events, variadic payloads, and Lua integration.
 * @ingroup Events
 */
#pragma once

#include "event.h"

#ifdef __cplusplus
#include <initializer_list>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef	ks_ptr Ks_EventManager;

typedef const void* Ks_EventData;

typedef void (*Ks_EventCallback)(Ks_EventData data, void* user_data);

typedef void (*Ks_UserDataFreeCallback)(void* user_data);

typedef struct Ks_Signal { char _unused; } Ks_Signal;

/**
 * @brief Creates an Event Manager.
 */
KS_API Ks_EventManager ks_event_manager_create();
KS_API ks_no_ret ks_event_manager_destroy(Ks_EventManager em);

KS_API Ks_Handle ks_event_manager_register_type(Ks_EventManager em, const char* type_name);
KS_API Ks_Handle ks_event_manager_register_signal(Ks_EventManager em, const char* signal_name);

/**
 * @brief Retrieves the handle for a registered event.
 */
KS_API Ks_Handle ks_event_manager_get_event_handle(Ks_EventManager em, ks_str name);

/**
 * @brief Retrieves the registered name of an event given its handle.
 * Useful for debugging or reflection lookups.
 * @param event_handle The event handle.
 * @return The event name string, or NULL if invalid. The string is owned by the manager.
 */
KS_API const char* ks_event_manager_get_event_name(Ks_EventManager em, Ks_Handle event_handle);

/**
 * @brief Subscribes a callback to an event.
 * * @param event Handle of the event to observe.
 * @param callback Function to call when event is triggered.
 * @param user_data Context pointer passed to the callback.
 * @return Subscription handle (used to unsubscribe).
 */
KS_API Ks_Handle ks_event_manager_subscribe(Ks_EventManager em, Ks_Handle event_handle, Ks_EventCallback callback, void* user_data);

KS_API Ks_Handle ks_event_manager_subscribe_ex(
    Ks_EventManager em,
    Ks_Handle event_handle,
    Ks_EventCallback callback,
    void* user_data,
    Ks_UserDataFreeCallback free_cb
);


/**
 * @brief Unsubscribes from an event.
 */
KS_API ks_no_ret ks_event_manager_unsubscribe(Ks_EventManager em, Ks_Handle subscription);

KS_API void ks_event_manager_publish(
    Ks_EventManager em,
    Ks_Handle event_handle,
    const void* data_ptr
);

KS_API void ks_event_manager_emit(Ks_EventManager em, Ks_Handle signal_handle);

#ifdef __cplusplus
}
#endif