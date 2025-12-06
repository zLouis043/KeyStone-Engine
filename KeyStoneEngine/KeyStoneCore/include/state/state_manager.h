#pragma once

#include "../core/types.h"
#include "../core/handle.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_StateManager;

KS_API Ks_StateManager ks_state_manager_create();
KS_API ks_no_ret ks_state_manager_destroy(Ks_StateManager sm);

KS_API Ks_Handle ks_state_manager_new_int(Ks_StateManager sm, ks_str name, ks_int64 value);
KS_API Ks_Handle ks_state_manager_new_float(Ks_StateManager sm, ks_str name, ks_double value);
KS_API Ks_Handle ks_state_manager_new_bool(Ks_StateManager sm, ks_str name, ks_bool value);
KS_API Ks_Handle ks_state_manager_new_string(Ks_StateManager sm, ks_str name, ks_str value);
KS_API Ks_Handle ks_state_manager_new_usertype(Ks_StateManager sm, ks_str name, Ks_UserData ud, ks_str type_name);

KS_API Ks_Handle ks_state_manager_get_handle(Ks_StateManager sm, ks_str name);
KS_API ks_bool ks_state_manager_has(Ks_StateManager sm, ks_str name);

#ifdef __cplusplus
}
#endif