#include "../../include/serialization/serializer.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/en.h>

#include <deque>
#include <fstream>
#include <sstream>
#include <string>

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

KS_API ks_bool ks_serializer_load_from_string(Ks_Serializer ser, ks_str json_string) {
    Serializer_Impl* s = impl(ser);
    s->node_pool.clear();
    s->doc.SetObject();

    if (s->doc.Parse(json_string).HasParseError()) {
        KS_LOG_ERROR("[Serializer] Parse error: %s (Offset: %u)",
            GetParseError_En(s->doc.GetParseError()), s->doc.GetErrorOffset());
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

    if (a->IsArray()) {
        a->PushBack(*v, s->doc.GetAllocator());
    }
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
    if (o && o->IsObject() && o->HasMember(key)) {
        return (Ks_Json) & ((*o)[key]);
    }
    return nullptr;
}

KS_API ks_size ks_json_array_size(Ks_Json arr) {
    Value* a = val(arr);
    return (a && a->IsArray()) ? a->Size() : 0;
}

KS_API Ks_Json ks_json_array_get(Ks_Json arr, ks_size index) {
    Value* a = val(arr);
    if (a && a->IsArray() && index < (ks_size)a->Size()) {
        return (Ks_Json) & ((*a)[index]);
    }
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