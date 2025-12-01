#pragma once

#include "event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef	ks_ptr Ks_EventManager;

KS_API Ks_EventManager ks_event_manager_create();
KS_API ks_no_ret ks_event_manager_destroy(Ks_EventManager em);

KS_API Ks_Handle ks_event_manager_register(Ks_EventManager em, ks_str name, const Ks_Type* types, ks_size count);

KS_API Ks_Handle ks_event_manager_get_event_handle(Ks_EventManager em, ks_str name);

KS_API Ks_Handle ks_event_manager_subscribe(Ks_EventManager em, Ks_Handle event, ks_event_callback callback);

KS_API ks_no_ret ks_event_manager_unsubscribe(Ks_EventManager em, Ks_Handle subscription);

KS_API ks_no_ret ks_event_manager_publish(Ks_EventManager em, Ks_Handle event, ...);

#ifdef __cplusplus
}
#endif