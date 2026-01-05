#include "../../include/script/script_engine.h"
#include "../../include/core/types.h"
#include "../../include/serialization/serializer_binding.h"
#include "../../include/serialization/serializer.h"
#include "../../include/core/log.h"

#include <string.h>
#include <stdio.h>

struct SerializerUserData {
    Ks_Serializer handle;
};

struct JsonUserData {
    Ks_Json handle;
    Ks_Serializer owner;
};

static Ks_Json lua_value_to_json(Ks_Script_Ctx ctx, Ks_Serializer ser, Ks_Script_Object obj);
static Ks_Script_Object json_to_lua_value(Ks_Script_Ctx ctx, Ks_Serializer ser, Ks_Json json);

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

static bool is_lua_array(Ks_Script_Ctx ctx, Ks_Script_Table tbl) {
    ks_size arr_size = ks_script_table_array_size(ctx, tbl);
    if (arr_size == 0) return false;
    return true;
}

struct JsonPopulateContext {
    Ks_Script_Ctx ctx;
    Ks_Serializer ser;
    Ks_Script_Object tbl;
};

static void json_object_iterator_cb(ks_str key, Ks_Json val, void* user_data) {
    JsonPopulateContext* p = (JsonPopulateContext*)user_data;

    Ks_Script_Object key_obj = ks_script_create_cstring(p->ctx, key);
    Ks_Script_Object val_obj = json_to_lua_value(p->ctx, p->ser, val);

    ks_script_table_set(p->ctx, p->tbl, key_obj, val_obj);

    ks_script_free_obj(p->ctx, key_obj);
    ks_script_free_obj(p->ctx, val_obj);
}

static Ks_Json lua_value_to_json(Ks_Script_Ctx ctx, Ks_Serializer ser, Ks_Script_Object obj) {
    Ks_Type type = ks_script_obj_type(ctx, obj);

    switch (type) {
    case KS_TYPE_NIL:
        return ks_json_create_null(ser);

    case KS_TYPE_BOOL:
        return ks_json_create_bool(ser, ks_script_obj_as_boolean(ctx, obj));

    case KS_TYPE_INT:
    case KS_TYPE_UINT:
    case KS_TYPE_DOUBLE:
    case KS_TYPE_FLOAT:
        return ks_json_create_number(ser, ks_script_obj_as_number(ctx, obj));

    case KS_TYPE_CSTRING:
        return ks_json_create_string(ser, ks_script_obj_as_cstring(ctx, obj));

    case KS_TYPE_SCRIPT_TABLE: {
        Ks_Script_Table tbl = ks_script_obj_as_table(ctx, obj);

        if (is_lua_array(ctx, tbl)) {
            // Serializza come Array
            Ks_Json arr = ks_json_create_array(ser);
            ks_size size = ks_script_table_array_size(ctx, tbl);
            for (ks_size i = 1; i <= size; ++i) {
                Ks_Script_Object item = ks_script_table_get(ctx, tbl, ks_script_create_integer(ctx, (ks_int64)i));
                ks_json_array_push(ser, arr, lua_value_to_json(ctx, ser, item));
                ks_script_free_obj(ctx, item);
            }
            return arr;
        }
        else {
            Ks_Json jobj = ks_json_create_object(ser);
            Ks_Script_Table_Iterator iter = ks_script_table_iterate(ctx, tbl);
            Ks_Script_Object key, val;

            while (ks_script_iterator_next(ctx, &iter, &key, &val)) {
                const char* key_str = nullptr;
                char key_buf[64];

                Ks_Type k_type = ks_script_obj_type(ctx, key);

                if (k_type == KS_TYPE_CSTRING) {
                    key_str = ks_script_obj_as_cstring(ctx, key);
                }
                else if (k_type == KS_TYPE_INT || k_type == KS_TYPE_UINT) {
                    long long v = (long long)ks_script_obj_as_integer(ctx, key);
                    snprintf(key_buf, sizeof(key_buf), "%lld", v);
                    key_str = key_buf;
                }
                else if (k_type == KS_TYPE_DOUBLE || k_type == KS_TYPE_FLOAT) {
                    double v = ks_script_obj_as_number(ctx, key);
                    snprintf(key_buf, sizeof(key_buf), "%.14g", v);
                    key_str = key_buf;
                }
                else {
                    key_str = ks_script_obj_to_string(ctx, key);
                }

                if (key_str) {
                    ks_json_object_add(ser, jobj, key_str, lua_value_to_json(ctx, ser, val));
                }
                ks_script_free_obj(ctx, val);
            }
            ks_script_iterator_destroy(ctx, &iter);
            return jobj;
        }
    }

    case KS_TYPE_USERDATA: {
        ks_str type_name = ks_script_obj_get_usertype_name(ctx, obj);
        if (!type_name) return ks_json_create_null(ser);

        void* instance = ks_script_usertype_get_ptr(ctx, obj);

        Ks_Json jobj = ks_json_serialize(ser, instance, type_name);

        if (ks_json_get_type(jobj) != KS_JSON_OBJECT) {
            jobj = ks_json_create_object(ser);
            ks_json_object_add(ser, jobj, "$type", ks_json_create_string(ser, type_name));
        }


        return jobj;
    }

    default:
        return ks_json_create_null(ser);
    }
}

static Ks_Script_Object json_to_lua_value(Ks_Script_Ctx ctx, Ks_Serializer ser, Ks_Json json) {
    Ks_JsonType type = ks_json_get_type(json);

    switch (type) {
    case KS_JSON_NULL:
        return ks_script_create_nil(ctx);

    case KS_JSON_BOOLEAN:
        return ks_script_create_boolean(ctx, ks_json_get_bool(json));

    case KS_JSON_NUMBER: {
        ks_double d = ks_json_get_number(json);
        if (d == (ks_int64)d) return ks_script_create_integer(ctx, (ks_int64)d);
        return ks_script_create_number(ctx, d);
    }

    case KS_JSON_STRING:
        return ks_script_create_cstring(ctx, ks_json_get_string(json));

    case KS_JSON_ARRAY: {
        Ks_Script_Table tbl = ks_script_create_table(ctx);
        ks_size size = ks_json_array_size(json);
        for (ks_size i = 0; i < size; ++i) {
            Ks_Script_Object val = json_to_lua_value(ctx, ser, ks_json_array_get(json, i));
            ks_script_table_set(ctx, tbl, ks_script_create_integer(ctx, (ks_int64)(i + 1)), val);
            ks_script_free_obj(ctx, val);
        }
        return tbl;
    }

    case KS_JSON_OBJECT: {
        Ks_Script_Object result_obj;
        bool is_usertype = false;

        if (ks_json_object_has(json, "$type")) {
            Ks_Json type_node = ks_json_object_get(json, "$type");
            ks_str type_name = ks_json_get_string(type_node);

            if (type_name && strlen(type_name) > 0) {
                result_obj = ks_script_create_usertype_instance(ctx, type_name);
                if (ks_script_obj_is_valid(ctx, result_obj)) {
                    is_usertype = true;
                }
            }
        }

        if (is_usertype) {
            ks_str type_name = ks_script_obj_get_usertype_name(ctx, result_obj);
            void* instance = ks_script_usertype_get_ptr(ctx, result_obj);
            ks_json_deserialize(ser, instance, type_name, json);
            return result_obj;
        }
        else {
            result_obj = ks_script_create_table(ctx);

            JsonPopulateContext pctx = { ctx, ser, result_obj };
            ks_json_object_foreach(json, json_object_iterator_cb, &pctx);

            return result_obj;
        }
    }
    }
    return ks_script_create_nil(ctx);
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
    ks_script_stack_push_cstring(ctx, res);
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
    ks_script_stack_push_cstring(ctx, ks_json_get_string(self->handle));
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

static ks_returns_count l_serializer_serialize(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);

    Ks_Script_Object obj = ks_script_get_arg(ctx, 1);

    Ks_Json result_json = lua_value_to_json(ctx, self->handle, obj);

    push_json_userdata(ctx, self->handle, result_json);
    return 1;
}

static ks_returns_count l_serializer_deserialize(Ks_Script_Ctx ctx) {
    auto* self = (SerializerUserData*)ks_script_get_self(ctx);
    Ks_Script_Object json_arg = ks_script_get_arg(ctx, 1);

    Ks_Json json_node = nullptr;
    if (ks_script_obj_type(ctx, json_arg) == KS_TYPE_USERDATA) {
        auto* j_ud = (JsonUserData*)ks_script_usertype_get_ptr(ctx, json_arg);
        if (j_ud) json_node = j_ud->handle;
    }

    if (!json_node) {
        KS_LOG_ERROR("[Lua Serializer] Invalid JSON node passed to deserialize");
        ks_script_stack_push_obj(ctx, ks_script_create_nil(ctx));
        return 1;
    }

    Ks_Script_Object res = json_to_lua_value(ctx, self->handle, json_node);
    ks_script_stack_push_obj(ctx, res);
    ks_script_free_obj(ctx, res);
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

    ks_script_usertype_add_method(b_ser, "serialize", KS_SCRIPT_FUNC(l_serializer_serialize, KS_TYPE_SCRIPT_ANY));

    ks_script_usertype_add_method(b_ser, "deserialize", KS_SCRIPT_FUNC(l_serializer_deserialize, KS_TYPE_USERDATA));

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