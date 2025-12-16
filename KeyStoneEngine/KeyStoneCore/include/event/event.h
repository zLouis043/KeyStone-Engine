#pragma once

#include "../core/types.h"
#include "../core/handle.h"
#include "../core/cb.h"
#include "../script/script_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_Event_Payload;

typedef ks_bool(*ks_event_callback)(Ks_Event_Payload data, Ks_Payload user_data);

KS_API ks_size ks_event_get_args_count(Ks_Event_Payload data);
KS_API Ks_Type ks_event_get_arg_type(Ks_Event_Payload data, ks_size index);

KS_API ks_bool ks_event_get_bool(Ks_Event_Payload data, ks_size index);
KS_API ks_char ks_event_get_char(Ks_Event_Payload data, ks_size index);
KS_API ks_int ks_event_get_int(Ks_Event_Payload data, ks_size index);
KS_API ks_float ks_event_get_float(Ks_Event_Payload data, ks_size index);
KS_API ks_double ks_event_get_double(Ks_Event_Payload data, ks_size index);
KS_API ks_str ks_event_get_cstring(Ks_Event_Payload data, ks_size index);
KS_API ks_ptr ks_event_get_ptr(Ks_Event_Payload data, ks_size index);
KS_API Ks_UserData ks_event_get_userdata(Ks_Event_Payload data, ks_size index);
KS_API Ks_Script_Table ks_event_get_script_table(Ks_Event_Payload data, ks_size index);
KS_API Ks_Script_Ctx ks_event_get_script_ctx(Ks_Event_Payload data);

#ifdef __cplusplus
}
#endif