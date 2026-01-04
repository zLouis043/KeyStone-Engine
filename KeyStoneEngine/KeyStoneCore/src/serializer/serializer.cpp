#include "../../include/serialization/serializer.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"
#include "../../include/core/reflection.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>

#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

using namespace rapidjson;

struct Serializer_Impl {
    Document doc;
    std::deque<Value> node_pool;
    std::string dump_buffer;

    Serializer_Impl() {
        doc.SetObject();
    }
};

static Serializer_Impl* impl(Ks_Serializer s) { return (Serializer_Impl*)s; }
static Value* val(Ks_Json j) { return (Value*)j; }

// --- Utils ---

static std::string clean_type_names(const char* raw_name) {
    if (!raw_name) return "";
    std::string s = raw_name;
    const char* kws[] = { "const", "volatile", "struct", "enum", "union", "_Atomic" };
    for (const char* kw : kws) {
        size_t pos;
        while ((pos = s.find(kw)) != std::string::npos) s.erase(pos, strlen(kw));
    }
    s.erase(std::remove(s.begin(), s.end(), '*'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '&'), s.end());
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    return s;
}

static size_t get_primitive_size(Ks_Type type) {
    switch (type) {
    case KS_TYPE_BOOL:   return sizeof(bool);
    case KS_TYPE_CHAR:   return sizeof(char);
    case KS_TYPE_INT:    return sizeof(int);
    case KS_TYPE_UINT:   return sizeof(unsigned int);
    case KS_TYPE_FLOAT:  return sizeof(float);
    case KS_TYPE_DOUBLE: return sizeof(double);
    case KS_TYPE_PTR:             return sizeof(void*);
    case KS_TYPE_LIGHTUSERDATA:   return sizeof(void*);
    case KS_TYPE_CSTRING:         return sizeof(const char*);
    default: return 0;
    }
}

// --- Forward Declarations ---
static Ks_Json serialize_type_recursive(Ks_Serializer ser, const void* instance, const Ks_Type_Info* info);
static bool deserialize_type_recursive(Ks_Serializer ser, void* instance, const Ks_Type_Info* info, Ks_Json json);

// ============================================================================
// SERIALIZATION LOGIC
// ============================================================================

static Ks_Json serialize_primitive(Ks_Serializer ser, const void* addr, Ks_Type type, int ptr_depth) {
    // 1. Gestione Stringhe (Unici puntatori supportati)
    if ((type == KS_TYPE_CHAR && ptr_depth == 1) || type == KS_TYPE_CSTRING) {
        const char* str_val = *(const char**)addr;
        if (str_val) return ks_json_create_string(ser, str_val);
        return ks_json_create_null(ser);
    }

    // 2. Altri puntatori -> NULL (Evitiamo crash e complessità)
    if (ptr_depth > 0) return ks_json_create_null(ser);

    // 3. Valori
    switch (type) {
    case KS_TYPE_BOOL:   return ks_json_create_bool(ser, *(bool*)addr);
    case KS_TYPE_CHAR:   return ks_json_create_number(ser, (double)*(char*)addr);
    case KS_TYPE_INT:    return ks_json_create_number(ser, (double)*(int*)addr);
    case KS_TYPE_UINT:   return ks_json_create_number(ser, (double)*(unsigned int*)addr);
    case KS_TYPE_FLOAT:  return ks_json_create_number(ser, (double)*(float*)addr);
    case KS_TYPE_DOUBLE: return ks_json_create_number(ser, *(double*)addr);
    default:             return ks_json_create_null(ser);
    }
}

static Ks_Json serialize_array_recursive(Ks_Serializer ser, const void* base_addr, const Ks_Field_Info* field, int dim_index, size_t elem_size, const Ks_Type_Info* elem_info) {
    Ks_Json json_arr = ks_json_create_array(ser);
    size_t count = field->dims[dim_index];
    size_t stride = elem_size;

    for (size_t i = dim_index + 1; i < field->dim_count; ++i) stride *= field->dims[i];

    for (size_t i = 0; i < count; ++i) {
        const void* item_addr = (const char*)base_addr + (i * stride);
        Ks_Json item_val = nullptr;

        if (dim_index == field->dim_count - 1) { // Leaf
            if (field->type == KS_TYPE_USERDATA) {
                // Se è un puntatore a struct, lo ignoriamo (NULL) per evitare crash/cicli
                if (field->ptr_depth > 0) item_val = ks_json_create_null(ser);
                else item_val = serialize_type_recursive(ser, item_addr, elem_info);
            }
            else {
                item_val = serialize_primitive(ser, item_addr, field->type, field->ptr_depth);
            }
        }
        else {
            item_val = serialize_array_recursive(ser, item_addr, field, dim_index + 1, elem_size, elem_info);
        }
        ks_json_array_push(ser, json_arr, item_val);
    }
    return json_arr;
}

static Ks_Json serialize_type_recursive(Ks_Serializer ser, const void* instance, const Ks_Type_Info* info) {
    if (!info) return ks_json_create_null(ser);

    if (info->kind == KS_META_ENUM) {
        int val = *(int*)instance;
        return ks_json_create_number(ser, (double)val);
    }

    if (info->kind == KS_META_STRUCT || info->kind == KS_META_UNION) {
        Ks_Json obj = ks_json_create_object(ser);

        for (size_t i = 0; i < info->field_count; ++i) {
            const Ks_Field_Info* field = &info->fields[i];
            const void* field_addr = (const char*)instance + field->offset;
            Ks_Json field_val = nullptr;

            if (field->is_array) {
                size_t elem_size = 0;
                const Ks_Type_Info* sub_info = nullptr;

                if (field->type == KS_TYPE_USERDATA) {
                    std::string clean_name = clean_type_names(field->type_str);
                    sub_info = ks_reflection_get_type(clean_name.c_str());
                    if (sub_info) {
                        elem_size = (field->ptr_depth > 0) ? sizeof(void*) : sub_info->size;
                    }
                }
                else {
                    elem_size = (field->ptr_depth > 0) ? sizeof(void*) : get_primitive_size(field->type);
                }

                if (elem_size > 0) {
                    field_val = serialize_array_recursive(ser, field_addr, field, 0, elem_size, sub_info);
                }
                else {
                    field_val = ks_json_create_null(ser);
                }
            }
            else {
                if (field->type == KS_TYPE_USERDATA) {
                    std::string clean_name = clean_type_names(field->type_str);
                    const Ks_Type_Info* sub_info = ks_reflection_get_type(clean_name.c_str());

                    if (sub_info) {
                        // Puntatore a Struct -> NULL
                        if (field->ptr_depth > 0) {
                            field_val = ks_json_create_null(ser);
                        }
                        else {
                            field_val = serialize_type_recursive(ser, field_addr, sub_info);
                        }
                    }
                    else {
                        field_val = ks_json_create_null(ser);
                    }
                }
                else {
                    field_val = serialize_primitive(ser, field_addr, field->type, field->ptr_depth);
                }
            }
            ks_json_object_add(ser, obj, field->name, field_val);
        }
        return obj;
    }
    return ks_json_create_null(ser);
}

// ============================================================================
// DESERIALIZATION LOGIC
// ============================================================================

static void deserialize_primitive(void* addr, Ks_Type type, int ptr_depth, Ks_Json json) {
    // 1. Gestione Stringhe
    if ((type == KS_TYPE_CHAR && ptr_depth == 1) || type == KS_TYPE_CSTRING) {
        ks_str json_str = ks_json_get_string(json);
        if (json_str) {
            size_t len = strlen(json_str) + 1;
            char* mem = (char*)ks_alloc(len, KS_LT_USER_MANAGED, KS_TAG_RESOURCE);
            memcpy(mem, json_str, len);
            *(char**)addr = mem;
        }
        else {
            *(char**)addr = nullptr;
        }
        return;
    }

    // 2. Altri puntatori -> Ignoriamo in lettura
    if (ptr_depth > 0) return;

    // 3. Valori
    switch (type) {
    case KS_TYPE_BOOL:   *(bool*)addr = ks_json_get_bool(json); break;
    case KS_TYPE_CHAR:   *(char*)addr = (char)ks_json_get_number(json); break;
    case KS_TYPE_INT:    *(int*)addr = (int)ks_json_get_number(json); break;
    case KS_TYPE_UINT:   *(unsigned int*)addr = (unsigned int)ks_json_get_number(json); break;
    case KS_TYPE_FLOAT:  *(float*)addr = (float)ks_json_get_number(json); break;
    case KS_TYPE_DOUBLE: *(double*)addr = ks_json_get_number(json); break;
    default: break;
    }
}

static void deserialize_array_recursive(Ks_Serializer ser, void* base_addr, const Ks_Field_Info* field, int dim_index, size_t elem_size, const Ks_Type_Info* elem_info, Ks_Json json_arr) {
    if (ks_json_get_type(json_arr) != KS_JSON_ARRAY) return;

    size_t count = field->dims[dim_index];
    size_t json_count = ks_json_array_size(json_arr);
    size_t limit = (json_count < count) ? json_count : count;

    size_t stride = elem_size;
    for (size_t i = dim_index + 1; i < field->dim_count; ++i) stride *= field->dims[i];

    for (size_t i = 0; i < limit; ++i) {
        void* item_addr = (char*)base_addr + (i * stride);
        Ks_Json item_json = ks_json_array_get(json_arr, i);

        if (dim_index == field->dim_count - 1) {
            if (field->type == KS_TYPE_USERDATA) {
                // Puntatori a struct ignorati
                if (field->ptr_depth == 0) {
                    deserialize_type_recursive(ser, item_addr, elem_info, item_json);
                }
            }
            else {
                deserialize_primitive(item_addr, field->type, field->ptr_depth, item_json);
            }
        }
        else {
            deserialize_array_recursive(ser, item_addr, field, dim_index + 1, elem_size, elem_info, item_json);
        }
    }
}

static bool deserialize_type_recursive(Ks_Serializer ser, void* instance, const Ks_Type_Info* info, Ks_Json json) {
    if (!info || !instance || ks_json_get_type(json) != KS_JSON_OBJECT) return false;

    if (info->kind == KS_META_ENUM) {
        *(int*)instance = (int)ks_json_get_number(json);
        return true;
    }

    if (info->kind == KS_META_STRUCT || info->kind == KS_META_UNION) {
        for (size_t i = 0; i < info->field_count; ++i) {
            const Ks_Field_Info* field = &info->fields[i];

            if (!ks_json_object_has(json, field->name)) continue;
            Ks_Json field_json = ks_json_object_get(json, field->name);
            void* field_addr = (char*)instance + field->offset;

            if (field->is_array) {
                size_t elem_size = 0;
                const Ks_Type_Info* sub_info = nullptr;

                if (field->type == KS_TYPE_USERDATA) {
                    std::string clean_name = clean_type_names(field->type_str);
                    sub_info = ks_reflection_get_type(clean_name.c_str());
                    if (sub_info) elem_size = (field->ptr_depth > 0) ? sizeof(void*) : sub_info->size;
                }
                else {
                    elem_size = (field->ptr_depth > 0) ? sizeof(void*) : get_primitive_size(field->type);
                }

                if (elem_size > 0) {
                    deserialize_array_recursive(ser, field_addr, field, 0, elem_size, sub_info, field_json);
                }
            }
            else {
                if (field->type == KS_TYPE_USERDATA) {
                    std::string clean_name = clean_type_names(field->type_str);
                    const Ks_Type_Info* sub_info = ks_reflection_get_type(clean_name.c_str());
                    if (sub_info) {
                        // Struct per valore (NO Puntatori)
                        if (field->ptr_depth == 0) {
                            deserialize_type_recursive(ser, field_addr, sub_info, field_json);
                        }
                    }
                }
                else {
                    deserialize_primitive(field_addr, field->type, field->ptr_depth, field_json);
                }
            }
        }
        return true;
    }
    return false;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

KS_API Ks_Serializer ks_serializer_create() {
    void* mem = ks_alloc(sizeof(Serializer_Impl), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA);
    return (Ks_Serializer)new(mem) Serializer_Impl();
}

KS_API ks_no_ret ks_serializer_destroy(Ks_Serializer ser) {
    if (ser) {
        Serializer_Impl* s = impl(ser);
        s->~Serializer_Impl();
        ks_dealloc(s);
    }
}

// ... [Funzioni IO e Access rimangono identiche e necessarie per compilare] ...
KS_API ks_bool ks_serializer_load_from_string(Ks_Serializer ser, ks_str json_string) {
    Serializer_Impl* s = impl(ser);
    s->node_pool.clear();
    s->doc.SetObject();
    if (s->doc.Parse(json_string).HasParseError()) {
        KS_LOG_ERROR("[Serializer] Parse error: %s (Offset: %u)", GetParseError_En(s->doc.GetParseError()), s->doc.GetErrorOffset());
        return ks_false;
    }
    return ks_true;
}

KS_API ks_bool ks_serializer_load_from_file(Ks_Serializer ser, ks_str path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        KS_LOG_ERROR("[Serializer] Failed to open file: %s", path);
        return ks_false;
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return ks_serializer_load_from_string(ser, buffer.str().c_str());
}

KS_API ks_str ks_serializer_dump_to_string(Ks_Serializer ser) {
    Serializer_Impl* s = impl(ser);
    StringBuffer sb;
    PrettyWriter<StringBuffer> writer(sb);
    s->doc.Accept(writer);
    s->dump_buffer = sb.GetString();
    return s->dump_buffer.c_str();
}

KS_API ks_bool ks_serializer_dump_to_file(Ks_Serializer ser, ks_str path) {
    ks_str content = ks_serializer_dump_to_string(ser);
    std::ofstream ofs(path);
    if (!ofs.is_open()) return ks_false;
    ofs << content;
    return ks_true;
}

KS_API Ks_Json ks_serializer_get_root(Ks_Serializer ser) {
    return (Ks_Json)&impl(ser)->doc;
}

static Ks_Json make_val(Serializer_Impl* s, Type type) {
    s->node_pool.emplace_back(type);
    return (Ks_Json)&s->node_pool.back();
}
KS_API Ks_Json ks_json_create_object(Ks_Serializer ser) { return make_val(impl(ser), kObjectType); }
KS_API Ks_Json ks_json_create_array(Ks_Serializer ser) { return make_val(impl(ser), kArrayType); }
KS_API Ks_Json ks_json_create_null(Ks_Serializer ser) { return make_val(impl(ser), kNullType); }
KS_API Ks_Json ks_json_create_bool(Ks_Serializer ser, ks_bool val) {
    Serializer_Impl* s = impl(ser);
    s->node_pool.emplace_back(val ? true : false);
    return (Ks_Json)&s->node_pool.back();
}
KS_API Ks_Json ks_json_create_number(Ks_Serializer ser, ks_double val) {
    Serializer_Impl* s = impl(ser);
    s->node_pool.emplace_back(val);
    return (Ks_Json)&s->node_pool.back();
}
KS_API Ks_Json ks_json_create_string(Ks_Serializer ser, ks_str val) {
    Serializer_Impl* s = impl(ser);
    s->node_pool.emplace_back();
    Value& v = s->node_pool.back();
    v.SetString(val, s->doc.GetAllocator());
    return (Ks_Json)&v;
}
KS_API ks_no_ret ks_json_object_add(Ks_Serializer ser, Ks_Json obj, ks_str key, Ks_Json value) {
    if (!ser || !obj || !value) return;
    Serializer_Impl* s = impl(ser);
    Value* o = val(obj);
    Value* v = val(value);
    if (o->IsObject()) {
        Value k;
        k.SetString(key, s->doc.GetAllocator());
        o->AddMember(k, *v, s->doc.GetAllocator());
    }
}
KS_API ks_no_ret ks_json_array_push(Ks_Serializer ser, Ks_Json arr, Ks_Json value) {
    if (!ser || !arr || !value) return;
    Serializer_Impl* s = impl(ser);
    Value* a = val(arr);
    Value* v = val(value);
    if (a->IsArray()) a->PushBack(*v, s->doc.GetAllocator());
}
KS_API Ks_JsonType ks_json_get_type(Ks_Json json) {
    if (!json) return KS_JSON_NULL;
    Value* v = val(json);
    if (v->IsNull()) return KS_JSON_NULL;
    if (v->IsFalse()) return KS_JSON_BOOLEAN;
    if (v->IsTrue()) return KS_JSON_BOOLEAN;
    if (v->IsObject()) return KS_JSON_OBJECT;
    if (v->IsArray()) return KS_JSON_ARRAY;
    if (v->IsString()) return KS_JSON_STRING;
    if (v->IsNumber()) return KS_JSON_NUMBER;
    return KS_JSON_NULL;
}
KS_API ks_bool ks_json_object_has(Ks_Json obj, ks_str key) {
    Value* o = val(obj);
    return (o && o->IsObject() && o->HasMember(key));
}
KS_API Ks_Json ks_json_object_get(Ks_Json obj, ks_str key) {
    Value* o = val(obj);
    if (o && o->IsObject() && o->HasMember(key)) return (Ks_Json) & ((*o)[key]);
    return nullptr;
}
KS_API ks_size ks_json_array_size(Ks_Json arr) {
    Value* a = val(arr);
    return (a && a->IsArray()) ? a->Size() : 0;
}
KS_API Ks_Json ks_json_array_get(Ks_Json arr, ks_size index) {
    Value* a = val(arr);
    if (a && a->IsArray() && index < (ks_size)a->Size()) return (Ks_Json) & ((*a)[index]);
    return nullptr;
}
KS_API ks_double ks_json_get_number(Ks_Json json) {
    Value* v = val(json);
    return (v && v->IsNumber()) ? v->GetDouble() : 0.0;
}
KS_API ks_bool ks_json_get_bool(Ks_Json json) {
    Value* v = val(json);
    return (v && v->IsBool()) ? v->GetBool() : false;
}
KS_API ks_str ks_json_get_string(Ks_Json json) {
    Value* v = val(json);
    return (v && v->IsString()) ? v->GetString() : "";
}

KS_API ks_no_ret ks_json_object_foreach(Ks_Json obj, ks_json_foreach_cb cb, void* user_data) {
    if (!obj || !cb) return;
    Value* o = val(obj);
    if (o && o->IsObject()) {
        for (auto& m : o->GetObject()) {
            cb(m.name.GetString(), (Ks_Json)&m.value, user_data);
        }
    }
}

// --- Reflection Entry Points (SIMPLE & SAFE) ---

KS_API Ks_Json ks_json_serialize(Ks_Serializer ser, const void* instance, ks_str type_name) {
    if (!ser || !instance || !type_name) return nullptr;

    const Ks_Type_Info* info = ks_reflection_get_type(type_name);
    if (!info) {
        KS_LOG_ERROR("[Serializer] Serialize failed: Type '%s' not found.", type_name);
        return ks_json_create_null(ser);
    }

    Ks_Json node = serialize_type_recursive(ser, instance, info);

    if (ks_json_get_type(node) == KS_JSON_OBJECT) {
        if (!ks_json_object_has(node, "$type")) {
            ks_json_object_add(ser, node, "$type", ks_json_create_string(ser, type_name));
        }
    }

    return node;
}

KS_API ks_bool ks_json_deserialize(Ks_Serializer ser, void* instance, ks_str type_name, Ks_Json json_node) {
    if (!ser || !instance || !type_name || !json_node) return false;
    const Ks_Type_Info* info = ks_reflection_get_type(type_name);
    if (!info) {
        KS_LOG_ERROR("[Serializer] Deserialize failed: Type '%s' not found.", type_name);
        return false;
    }
    return deserialize_type_recursive(ser, instance, info, json_node);
}