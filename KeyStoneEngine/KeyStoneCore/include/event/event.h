#pragma once

#include "../core/types.h"
#include "../core/handle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_Event_Payload;

typedef ks_bool(*ks_event_callback)(Ks_Event_Payload data);

KS_API ks_bool ks_event_get_bool(Ks_Event_Payload data, ks_size index);
KS_API ks_char ks_event_get_char(Ks_Event_Payload data, ks_size index);
KS_API ks_int ks_event_get_int(Ks_Event_Payload data, ks_size index);
KS_API ks_float ks_event_get_float(Ks_Event_Payload data, ks_size index);
KS_API ks_double ks_event_get_double(Ks_Event_Payload data, ks_size index);
KS_API ks_str ks_event_get_cstring(Ks_Event_Payload data, ks_size index);
KS_API ks_lstr ks_event_get_lstring(Ks_Event_Payload data, ks_size index);
KS_API ks_ptr ks_event_get_ptr(Ks_Event_Payload data, ks_size index);
KS_API ks_userdata ks_event_get_userdata(Ks_Event_Payload data, ks_size index);

#ifdef __cplusplus
}
#endif