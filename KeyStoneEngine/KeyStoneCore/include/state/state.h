#pragma once

#include "state_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

KS_API Ks_Type ks_state_get_type(Ks_StateManager sm, Ks_Handle handle);

KS_API ks_int64  ks_state_get_int(Ks_StateManager sm, Ks_Handle handle);
KS_API ks_double ks_state_get_float(Ks_StateManager sm, Ks_Handle handle);
KS_API ks_bool   ks_state_get_bool(Ks_StateManager sm, Ks_Handle handle);
KS_API ks_str    ks_state_get_string(Ks_StateManager sm, Ks_Handle handle);
KS_API ks_ptr    ks_state_get_ptr(Ks_StateManager sm, Ks_Handle handle);

KS_API ks_int64  ks_state_get_int_or(Ks_StateManager sm, Ks_Handle handle, ks_int64 def);
KS_API ks_double ks_state_get_float_or(Ks_StateManager sm, Ks_Handle handle, ks_double def);
KS_API ks_bool   ks_state_get_bool_or(Ks_StateManager sm, Ks_Handle handle, ks_bool def);
KS_API ks_str    ks_state_get_string_or(Ks_StateManager sm, Ks_Handle handle, ks_str def);
KS_API ks_ptr    ks_state_get_ptr_or(Ks_StateManager sm, Ks_Handle handle, ks_ptr def);

KS_API ks_ptr    ks_state_get_usertype_info(Ks_StateManager sm, Ks_Handle handle, ks_str* out_type_name, ks_size* out_size);

KS_API ks_bool ks_state_set_int(Ks_StateManager sm, Ks_Handle handle, ks_int64 value);
KS_API ks_bool ks_state_set_float(Ks_StateManager sm, Ks_Handle handle, ks_double value);
KS_API ks_bool ks_state_set_bool(Ks_StateManager sm, Ks_Handle handle, ks_bool value);
KS_API ks_bool ks_state_set_string(Ks_StateManager sm, Ks_Handle handle, ks_str value);
KS_API ks_bool ks_state_set_usertype(Ks_StateManager sm, Ks_Handle handle, Ks_UserData ud, ks_str type_name);

#ifdef __cplusplus
}
#endif