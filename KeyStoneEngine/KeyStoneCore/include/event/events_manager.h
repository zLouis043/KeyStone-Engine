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

/**
 * @brief Creates an Event Manager.
 */
KS_API Ks_EventManager ks_event_manager_create();
KS_API ks_no_ret ks_event_manager_destroy(Ks_EventManager em);

#ifdef __cplusplus
#define ks_event_manager_register(em, name, ...) \
    ks_event_manager_register_impl(em, name, \
    std::initializer_list<Ks_Type>{__VA_ARGS__}.begin(), \
    std::initializer_list<Ks_Type>{__VA_ARGS__}.size())
#else

#define ks_event_manager_register(event_manager, event_name, ...) \
	ks_event_manager_register_impl(event_manager, event_name, (const Ks_Type[]){__VA_ARGS__}, (sizeof((const Ks_Type[]){__VA_ARGS__})) / sizeof(Ks_Type) )	
#endif

/**
 * @brief Registers a new event type signature.
 * * @param em The manager.
 * @param name Unique event name.
 * @param types Array of Ks_Type defining the payload signature.
 * @param count Number of arguments in the signature.
 * @return Handle to the event definition.
 */
KS_API Ks_Handle ks_event_manager_register_impl(Ks_EventManager em, ks_str name, const Ks_Type* types, ks_size count);

/**
 * @brief Retrieves the handle for a registered event.
 */
KS_API Ks_Handle ks_event_manager_get_event_handle(Ks_EventManager em, ks_str name);

KS_API const Ks_Type* ks_event_manager_get_signature(Ks_EventManager em, Ks_Handle event, ks_size* out_count);

/**
 * @brief Subscribes a callback to an event.
 * * @param event Handle of the event to observe.
 * @param callback Function to call when event is triggered.
 * @param user_data Context pointer passed to the callback.
 * @return Subscription handle (used to unsubscribe).
 */
KS_API Ks_Handle ks_event_manager_subscribe(Ks_EventManager em, Ks_Handle event, ks_event_callback callback, Ks_Payload user_data);

/**
 * @brief Unsubscribes from an event.
 */
KS_API ks_no_ret ks_event_manager_unsubscribe(Ks_EventManager em, Ks_Handle subscription);

/**
 * @brief Publishes an event immediately.
 * All subscribers are invoked synchronously.
 * * @param event Event handle.
 * @param ... Variadic arguments matching the registered signature.
 */
KS_API ks_no_ret ks_event_manager_publish(Ks_EventManager em, Ks_Handle event, ...);

ks_no_ret ks_event_manager_publish_direct(Ks_EventManager em, Ks_Handle event, Ks_Event_Payload pi);

#ifdef __cplusplus
}
#endif