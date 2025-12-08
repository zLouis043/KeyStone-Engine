#include "../../include/script/script_engine.h"
#include "../../include/core/types.h"
#include "../../include/serialization/serializer_binding.h"
#include "../../include/serialization/serializer.h"
#include "../../include/core/log.h"

#include <string.h>

struct SerializerUserData {
    Ks_Serializer handle;
};

struct JsonUserData {
    Ks_Json handle;
    Ks_Serializer owner;
};

static void push_json_userdata(Ks_Script_Ctx ctx, Ks_Serializer owner, Ks_Json json) {
    if (!json) {
        ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
        return;
    }

    Ks_Script_Userdata ud = ks_script_create_usertype_instance(ctx, "Json");
    auto* data = (JsonUserData*)ks_script_usertype_get_ptr(ctx, ud);

    if (data) {
        data->handle = json;
        data->owner = owner;
    }

    ks_script_stack_push_obj(ctx, ud);
}

static ks_returns_count l_serializer_new(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    self->handle = ks_serializer_create();
    return 0;
}

static void l_serializer_gc(ks_ptr ptr, ks_size size) {
    auto* self = (SerializerUserData*)ptr;
    if (self && self->handle) {
        ks_serializer_destroy(self->handle);
        self->handle = nullptr;
    }
}

static ks_returns_count l_serializer_load_string(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    const char* str = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));

    bool res = ks_serializer_load_from_string(self->handle, str);
    ks_script_stack_push_boolean(ctx, res);
    return 1;
}

static ks_returns_count l_serializer_load_file(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    const char* path = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));

    bool res = ks_serializer_load_from_file(self->handle, path);
    ks_script_stack_push_boolean(ctx, res);
    return 1;
}

static ks_returns_count l_serializer_dump_string(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);

    ks_str res = ks_serializer_dump_to_string(self->handle);
    ks_script_stack_push_string(ctx, res);
    return 1;
}

static ks_returns_count l_serializer_dump_file(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    const char* path = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));

    bool res = ks_serializer_dump_to_file(self->handle, path);
    ks_script_stack_push_boolean(ctx, res);
    return 1;
}

static ks_returns_count l_serializer_get_root(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);

    Ks_Json root = ks_serializer_get_root(self->handle);
    push_json_userdata(ctx, self->handle, root);
    return 1;
}

static ks_returns_count l_serializer_create_object(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    push_json_userdata(ctx, self->handle, ks_json_create_object(self->handle));
    return 1;
}

static ks_returns_count l_serializer_create_array(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    push_json_userdata(ctx, self->handle, ks_json_create_array(self->handle));
    return 1;
}

static ks_returns_count l_serializer_create_string(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    const char* val = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    push_json_userdata(ctx, self->handle, ks_json_create_string(self->handle, val));
    return 1;
}

static ks_returns_count l_serializer_create_number(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    double val = ks_script_obj_as_number(ctx, ks_script_get_arg(ctx, 1));
    push_json_userdata(ctx, self->handle, ks_json_create_number(self->handle, val));
    return 1;
}

static ks_returns_count l_serializer_create_bool(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    bool val = ks_script_obj_as_boolean(ctx, ks_script_get_arg(ctx, 1));
    push_json_userdata(ctx, self->handle, ks_json_create_bool(self->handle, val));
    return 1;
}

static ks_returns_count l_serializer_create_null(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    push_json_userdata(ctx, self->handle, ks_json_create_null(self->handle));
    return 1;
}

static ks_returns_count l_json_get_type(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle) return 0;

    int t = (int)ks_json_get_type(self->handle);
    ks_script_stack_push_integer(ctx, t);
    return 1;
}

static ks_returns_count l_json_as_number(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle) return 0;
    ks_script_stack_push_number(ctx, ks_json_get_number(self->handle));
    return 1;
}

static ks_returns_count l_json_as_string(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle) return 0;
    ks_script_stack_push_string(ctx, ks_json_get_string(self->handle));
    return 1;
}

static ks_returns_count l_json_as_bool(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle) return 0;
    ks_script_stack_push_boolean(ctx, ks_json_get_bool(self->handle));
    return 1;
}

static ks_returns_count l_json_add(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle || !self->owner) return 0;

    const char* key = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    Ks_Script_Object val_obj = ks_script_get_arg(ctx, 2);

    if (ks_script_obj_type(ctx, val_obj) == KS_TYPE_USERDATA) {
        auto* val_data = (JsonUserData*)ks_script_usertype_get_ptr(ctx, val_obj);
        if (val_data && val_data->handle) {
            ks_json_object_add(self->owner, self->handle, key, val_data->handle);
        }
    }
    return 0;
}

static ks_returns_count l_json_get(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle) return 0;

    const char* key = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    Ks_Json child = ks_json_object_get(self->handle, key);

    push_json_userdata(ctx, self->owner, child);
    return 1;
}

static ks_returns_count l_json_has(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle) return 0;

    const char* key = ks_script_obj_as_cstring(ctx, ks_script_get_arg(ctx, 1));
    bool has = ks_json_object_has(self->handle, key);

    ks_script_stack_push_boolean(ctx, has);
    return 1;
}

static ks_returns_count l_json_push(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle || !self->owner) return 0;

    Ks_Script_Object val_obj = ks_script_get_arg(ctx, 1);

    if (ks_script_obj_type(ctx, val_obj) == KS_TYPE_USERDATA) {
        auto* val_data = (JsonUserData*)ks_script_usertype_get_ptr(ctx, val_obj);
        if (val_data && val_data->handle) {
            ks_json_array_push(self->owner, self->handle, val_data->handle);
        }
    }
    return 0;
}

static ks_returns_count l_json_at(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle) return 0;

    int index = (int)ks_script_obj_as_integer(ctx, ks_script_get_arg(ctx, 1));
    Ks_Json child = ks_json_array_get(self->handle, index);

    push_json_userdata(ctx, self->owner, child);
    return 1;
}

static ks_returns_count l_json_size(Ks_Script_Ctx ctx) {
    auto* self = (JsonUserData*)ks_script_get_self(ctx);
    if (!self->handle) return 0;

    ks_size sz = ks_json_array_size(self->handle);
    ks_script_stack_push_integer(ctx, (ks_int64)sz);
    return 1;
}

KS_API ks_no_ret ks_serializer_lua_bind(Ks_Script_Ctx ctx) {

    ks_script_register_enum(ctx, "json_type",
        KS_SCRIPT_ENUM_MEMBER("NULL", KS_JSON_NULL),
        KS_SCRIPT_ENUM_MEMBER("BOOLEAN", KS_JSON_BOOLEAN),
        KS_SCRIPT_ENUM_MEMBER("OBJECT", KS_JSON_OBJECT),
        KS_SCRIPT_ENUM_MEMBER("ARRAY", KS_JSON_ARRAY),
        KS_SCRIPT_ENUM_MEMBER("STRING", KS_JSON_STRING),
        KS_SCRIPT_ENUM_MEMBER("NUMBER", KS_JSON_NUMBER)
    );

    auto b_ser = ks_script_usertype_begin(ctx, "Serializer", sizeof(SerializerUserData));
    ks_script_usertype_add_constructor(b_ser, KS_SCRIPT_FUNC_VOID(l_serializer_new));
    ks_script_usertype_set_destructor(b_ser, l_serializer_gc);

    ks_script_usertype_add_method(b_ser, "load_string", KS_SCRIPT_FUNC(l_serializer_load_string, KS_TYPE_CSTRING));
    ks_script_usertype_add_method(b_ser, "load_file", KS_SCRIPT_FUNC(l_serializer_load_file, KS_TYPE_CSTRING));
    ks_script_usertype_add_method(b_ser, "dump_string", KS_SCRIPT_FUNC_VOID(l_serializer_dump_string));
    ks_script_usertype_add_method(b_ser, "dump_file", KS_SCRIPT_FUNC(l_serializer_dump_file, KS_TYPE_CSTRING));

    ks_script_usertype_add_method(b_ser, "root", KS_SCRIPT_FUNC_VOID(l_serializer_get_root));

    ks_script_usertype_add_method(b_ser, "object", KS_SCRIPT_FUNC_VOID(l_serializer_create_object));
    ks_script_usertype_add_method(b_ser, "array", KS_SCRIPT_FUNC_VOID(l_serializer_create_array));
    ks_script_usertype_add_method(b_ser, "string", KS_SCRIPT_FUNC(l_serializer_create_string, KS_TYPE_CSTRING));
    ks_script_usertype_add_method(b_ser, "number", KS_SCRIPT_FUNC(l_serializer_create_number, KS_TYPE_DOUBLE));
    ks_script_usertype_add_method(b_ser, "bool", KS_SCRIPT_FUNC(l_serializer_create_bool, KS_TYPE_BOOL));
    ks_script_usertype_add_method(b_ser, "null", KS_SCRIPT_FUNC_VOID(l_serializer_create_null));

    ks_script_usertype_end(b_ser);

    auto b_json = ks_script_usertype_begin(ctx, "Json", sizeof(JsonUserData));

    ks_script_usertype_add_method(b_json, "type", KS_SCRIPT_FUNC_VOID(l_json_get_type));
    ks_script_usertype_add_method(b_json, "as_number", KS_SCRIPT_FUNC_VOID(l_json_as_number));
    ks_script_usertype_add_method(b_json, "as_string", KS_SCRIPT_FUNC_VOID(l_json_as_string));
    ks_script_usertype_add_method(b_json, "as_bool", KS_SCRIPT_FUNC_VOID(l_json_as_bool));

    ks_script_usertype_add_method(b_json, "add", KS_SCRIPT_FUNC(l_json_add, KS_TYPE_CSTRING, KS_TYPE_USERDATA));
    ks_script_usertype_add_method(b_json, "get", KS_SCRIPT_FUNC(l_json_get, KS_TYPE_CSTRING));
    ks_script_usertype_add_method(b_json, "has", KS_SCRIPT_FUNC(l_json_has, KS_TYPE_CSTRING));

    ks_script_usertype_add_method(b_json, "push", KS_SCRIPT_FUNC(l_json_push, KS_TYPE_USERDATA));
    ks_script_usertype_add_method(b_json, "at", KS_SCRIPT_FUNC(l_json_at, KS_TYPE_INT));
    ks_script_usertype_add_method(b_json, "size", KS_SCRIPT_FUNC_VOID(l_json_size));

    ks_script_usertype_end(b_json);
}