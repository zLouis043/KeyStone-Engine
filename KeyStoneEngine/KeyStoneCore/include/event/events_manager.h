#pragma once

#include "event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef	ks_ptr Ks_EventManager;

KS_API Ks_EventManager ks_event_manager_create();
KS_API ks_no_ret ks_event_manager_destroy(Ks_EventManager em);

#ifdef __cplusplus
#include <initializer_list>
#define ks_event_manager_register(em, name, ...) \
    ks_event_manager_register_impl(em, name, \
    std::initializer_list<Ks_Type>{__VA_ARGS__}.begin(), \
    std::initializer_list<Ks_Type>{__VA_ARGS__}.size())
#else

#define ks_event_manager_register(event_manager, event_name, ...) \
	ks_event_manager_register_impl(event_manager, event_name, (const Ks_Type[]){__VA_ARGS__}, (sizeof((const Ks_Type[]){__VA_ARGS__})) / sizeof(Ks_Type) )	
#endif

KS_API Ks_Handle ks_event_manager_register_impl(Ks_EventManager em, ks_str name, const Ks_Type* types, ks_size count);

KS_API Ks_Handle ks_event_manager_get_event_handle(Ks_EventManager em, ks_str name);

KS_API const Ks_Type* ks_event_manager_get_signature(Ks_EventManager em, Ks_Handle event, ks_size* out_count);

KS_API Ks_Handle ks_event_manager_subscribe(Ks_EventManager em, Ks_Handle event, ks_event_callback callback, ks_ptr user_data);

KS_API ks_no_ret ks_event_manager_unsubscribe(Ks_EventManager em, Ks_Handle subscription);

KS_API ks_no_ret ks_event_manager_publish(Ks_EventManager em, Ks_Handle event, ...);

ks_no_ret ks_event_manager_publish_direct(Ks_EventManager em, Ks_Handle event, Ks_Event_Payload pi);

#ifdef __cplusplus
}
#endif