#include "../../include/state/state_manager.h"
#include "../../include/state/state_binding.h"
#include "../../include/state/state.h"
#include "../../include/script/script_engine.h"
#include "../../include/core/log.h"

struct StateRef {
    Ks_StateManager sm;
    Ks_Handle handle;
};

ks_returns_count l_stateref_get(Ks_Script_Ctx ctx) {
    StateRef* self = (StateRef*)ks_script_get_self(ctx);
    if (!self || !self->sm) return 0;

    Ks_Type t = ks_state_get_type(self->sm, self->handle);

    switch (t) {
    case KS_TYPE_INT:
        ks_script_stack_push_integer(ctx, ks_state_get_int(self->sm, self->handle));
        break;
    case KS_TYPE_DOUBLE:
        ks_script_stack_push_number(ctx, ks_state_get_float(self->sm, self->handle));
        break;
    case KS_TYPE_BOOL:
        ks_script_stack_push_boolean(ctx, ks_state_get_bool(self->sm, self->handle));
        break;
    case KS_TYPE_CSTRING:
        ks_script_stack_push_string(ctx, ks_state_get_string(self->sm, self->handle));
        break;
    case KS_TYPE_USERDATA: {
        const char* type_name;
        void* ptr = ks_state_get_usertype_info(self->sm, self->handle, &type_name, nullptr);
        if (ptr && type_name) {
            ks_script_stack_push_obj(ctx, ks_script_create_usertype_ref(ctx, type_name, ptr));
        }
        else {
            ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
        }
        break;
    }
    default: ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
    }
    return 1;
}

ks_returns_count l_stateref_set(Ks_Script_Ctx ctx) {
    StateRef* self = (StateRef*)ks_script_get_self(ctx);
    if (!self || !self->sm) return 0;

    Ks_Script_Object arg = ks_script_get_arg(ctx, 1);

    Ks_Type expected = ks_state_get_type(self->sm, self->handle);

    if (!ks_script_obj_is(ctx, arg, expected)) {
        return 0;
    }

    bool success = false;

    switch (expected) {
    case KS_TYPE_INT:
        success = ks_state_set_int(self->sm, self->handle, ks_script_obj_as_integer(ctx, arg));
        break;

    case KS_TYPE_DOUBLE:
    case KS_TYPE_FLOAT:
        success = ks_state_set_float(self->sm, self->handle, ks_script_obj_as_number(ctx, arg));
        break;

    case KS_TYPE_BOOL:
        success = ks_state_set_bool(self->sm, self->handle, ks_script_obj_as_boolean(ctx, arg));
        break;

    case KS_TYPE_CSTRING:
        success = ks_state_set_string(self->sm, self->handle, ks_script_obj_as_cstring(ctx, arg));
        break;

    case KS_TYPE_USERDATA: {
        Ks_UserData ud = ks_script_usertype_get_body(ctx, arg);
        const char* type_name = ks_script_obj_get_usertype_name(ctx, arg);

        if (!type_name) {
            success = false;
        }
        else if (ud.size == 0) {
            success = false;
        }
        else {
            success = ks_state_set_usertype(self->sm, self->handle, ud, type_name);
        }
        break;
    }
    default:
        break;
    }

    return 0;
}
ks_returns_count l_state_global(Ks_Script_Ctx ctx) {
    Ks_Script_Object up = ks_script_func_get_upvalue(ctx, 1);
    Ks_StateManager sm = (Ks_StateManager)ks_script_lightuserdata_get_ptr(ctx, up);

    const char* name = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    Ks_Script_Object val_obj = ks_script_get_arg(ctx, 2);

    Ks_Handle h = KS_INVALID_HANDLE;

    if (ks_script_obj_is_valid(ctx, val_obj)) {
        switch (val_obj.type) {
        case KS_TYPE_INT:
            h = ks_state_manager_new_int(sm, name, ks_script_obj_as_integer(ctx, val_obj)); break;
        case KS_TYPE_DOUBLE:
            h = ks_state_manager_new_float(sm, name, ks_script_obj_as_number(ctx, val_obj)); break;
        case KS_TYPE_BOOL:
            h = ks_state_manager_new_bool(sm, name, ks_script_obj_as_boolean(ctx, val_obj)); break;
        case KS_TYPE_CSTRING:
            h = ks_state_manager_new_string(sm, name, ks_script_obj_as_cstring(ctx, val_obj)); break;
        case KS_TYPE_USERDATA: {
            Ks_UserData ud = ks_script_usertype_get_body(ctx, val_obj);
            const char* tname = ks_script_obj_get_usertype_name(ctx, val_obj);

            if (!tname) {
                return 0;
            }
            if (ud.size == 0) {
                return 0;
            }

            h = ks_state_manager_new_usertype(sm, name, ud, tname);
            break;
        }
        default: break;
        }
    }
    else {
        h = ks_state_manager_get_handle(sm, name);
    }

    if (h == KS_INVALID_HANDLE) {
        ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
        return 1;
    }

    Ks_Script_Userdata u = ks_script_create_usertype_instance(ctx, "StateRef");
    StateRef* ref = (StateRef*)ks_script_usertype_get_ptr(ctx, u);
    if (ref) {
        ref->sm = sm;
        ref->handle = h;
    }
    ks_script_stack_push_obj(ctx, u);
    return 1;
}

KS_API ks_no_ret ks_state_manager_lua_bind(Ks_StateManager sm, Ks_Script_Ctx ctx) {
    auto b = ks_script_usertype_begin(ctx, "StateRef", sizeof(StateRef));
    ks_script_usertype_add_method(b, "get", KS_SCRIPT_FUNC_VOID(l_stateref_get));
    ks_script_usertype_add_method(b, "set", KS_SCRIPT_FUNC(l_stateref_set, KS_TYPE_SCRIPT_ANY));
    ks_script_usertype_end(b);

    Ks_Script_Object sm_ptr = ks_script_create_lightuserdata(ctx, sm);
    ks_script_stack_push_obj(ctx, sm_ptr);

    Ks_Script_Function func = ks_script_create_cfunc_with_upvalues(ctx,
        KS_SCRIPT_OVERLOAD(
            KS_SCRIPT_SIG_DEF(l_state_global, KS_TYPE_CSTRING),              
            KS_SCRIPT_SIG_DEF(l_state_global, KS_TYPE_CSTRING, KS_TYPE_SCRIPT_ANY)
        ),
    1);

    ks_script_set_global(ctx, "state", func);
}