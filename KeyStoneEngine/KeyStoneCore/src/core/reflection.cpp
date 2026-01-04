#include "core/reflection.h"
#include "memory/memory.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <mutex>

static std::mutex g_reflection_mutex;

static std::unordered_map<std::string, Ks_Type_Info*> g_registry;
static std::unordered_map<std::string, std::string> g_typedefs;

static const std::unordered_map<std::string, Ks_Type> g_primitives = {
    {"float", KS_TYPE_FLOAT}, {"double", KS_TYPE_DOUBLE},
    {"int", KS_TYPE_INT}, {"char", KS_TYPE_CHAR},
    {"bool", KS_TYPE_BOOL}, {"_Bool", KS_TYPE_BOOL},
    {"void", KS_TYPE_VOID},
    {"size_t", KS_TYPE_UINT}, {"uint8_t", KS_TYPE_UINT},
    {"int32_t", KS_TYPE_INT}, {"uint32_t", KS_TYPE_UINT},
    {"int64_t", KS_TYPE_INT}, {"uint64_t", KS_TYPE_UINT}
};

static const char* KsStrDup(const std::string& str) {
    if (str.empty()) return nullptr;
    size_t len = str.length() + 1;
    char* mem = (char*)ks_alloc(len, KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA);
    memcpy(mem, str.c_str(), len);
    return mem;
}

template<typename T>
static T* KsAllocArray(size_t count) {
    if (count == 0) return nullptr;
    return (T*)ks_alloc(sizeof(T) * count, KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA);
}

KS_API ks_no_ret ks_reflection_init(void) {
}

KS_API ks_no_ret ks_reflection_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_reflection_mutex);

    for (auto& pair : g_registry) {
        Ks_Type_Info* info = pair.second;
        if (!info) continue;

        if (info->fields) {
            for (size_t i = 0; i < info->field_count; ++i) {
  
                ks_dealloc((void*)info->fields[i].name);
                ks_dealloc((void*)info->fields[i].type_str);
                if (info->fields[i].return_type_str) ks_dealloc((void*)info->fields[i].return_type_str);

                if (info->fields[i].args) {
                    for (size_t k = 0; k < info->fields[i].arg_count; ++k) {
                        ks_dealloc((void*)info->fields[i].args[k].name);
                        ks_dealloc((void*)info->fields[i].args[k].type_str);
                    }
                    ks_dealloc((void*)info->fields[i].args);
                }
            }
            ks_dealloc((void*)info->fields);
        }

        if (info->enum_items) {
            for (size_t i = 0; i < info->enum_count; ++i) {
                ks_dealloc((void*)info->enum_items[i].name);
            }
            ks_dealloc((void*)info->enum_items);
        }

        if (info->args) {
            for (size_t i = 0; i < info->arg_count; ++i) {
                ks_dealloc((void*)info->args[i].name);
                ks_dealloc((void*)info->args[i].type_str);
            }
            ks_dealloc((void*)info->args);
        }

        if (info->return_type_str) ks_dealloc((void*)info->return_type_str);
        ks_dealloc((void*)info->name);

        ks_dealloc(info);
    }

    g_registry.clear();
    g_typedefs.clear();
}

struct TypeParseResult {
    std::string base_name;
    uint32_t modifiers;
    int ptr_depth;
};

static TypeParseResult ParseTypeString(std::string str) {
    TypeParseResult res = { "", KS_MOD_NONE, 0 };
    if (str.find("const") != std::string::npos)     res.modifiers |= KS_MOD_CONST;
    if (str.find("volatile") != std::string::npos)  res.modifiers |= KS_MOD_VOLATILE;
    if (str.find("atomic") != std::string::npos)    res.modifiers |= KS_MOD_ATOMIC;
    const char* kws[] = { "const", "volatile", "_Atomic", "atomic", "struct", "enum", "union" };
    for (auto kw : kws) {
        size_t pos;
        while ((pos = str.find(kw)) != std::string::npos) str.erase(pos, strlen(kw));
    }
    res.ptr_depth = (int)std::count(str.begin(), str.end(), '*');
    str.erase(std::remove(str.begin(), str.end(), '*'), str.end());
    str.erase(std::remove(str.begin(), str.end(), '&'), str.end());
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    res.base_name = str;
    return res;
}

static void ParseArrayDims(const char* suffix, Ks_Field_Info* info) {
    info->is_array = false; info->dim_count = 0; info->total_element_count = 1;
    memset(info->dims, 0, sizeof(info->dims));
    if (!suffix || !*suffix) return;
    std::string s = suffix;
    size_t pos = 0;
    while ((pos = s.find('[', pos)) != std::string::npos && info->dim_count < KS_REFLECT_MAX_DIMS) {
        size_t end = s.find(']', pos);
        if (end == std::string::npos) break;
        std::string num_str = s.substr(pos + 1, end - pos - 1);
        size_t val = 0;
        if (!num_str.empty()) try { val = std::stoull(num_str); }
        catch (...) {}
        info->dims[info->dim_count] = val;
        if (val > 0) info->total_element_count *= val;
        else if (info->total_element_count == 1) info->total_element_count = 0;
        info->dim_count++; info->is_array = true; pos = end + 1;
    }
}

static std::string ResolveAlias(const std::string& name) {
    std::string current = name;
    int guards = 0;
    while (g_typedefs.find(current) != g_typedefs.end() && guards < 16) {
        current = g_typedefs[current];
        guards++;
    }
    return current;
}

static Ks_Type ResolveBaseType(const std::string& name) {
    std::string resolved = ResolveAlias(name);
    auto it = g_primitives.find(resolved);
    if (it != g_primitives.end()) return it->second;
    if (g_registry.find(resolved) != g_registry.end()) return KS_TYPE_USERDATA;
    return KS_TYPE_UNKNOWN;
}

struct ReflectionBuilderInternal {
    std::string name;
    Ks_Meta_Kind kind;
    size_t size; size_t alignment;

    struct FieldPending {
        std::string name; std::string type_str; std::string suffix;
        size_t offset; size_t size;
        bool is_func_ptr = false;
        std::string return_type_str;
        struct Arg { std::string type; std::string name; };
        std::vector<Arg> args;
        bool is_bitfield = false;
        uint32_t bit_offset = 0; uint32_t bit_width = 0;
    };
    std::vector<FieldPending> fields;
    int current_field_index = -1;

    struct EnumPending { std::string name; int64_t value; };
    std::vector<EnumPending> enum_items;

    std::string return_type_str;
    struct FuncArg { std::string type_str; std::string name; };
    std::vector<FuncArg> func_args;

    ReflectionBuilderInternal(const char* n, Ks_Meta_Kind k, size_t s, size_t a)
        : name(n), kind(k), size(s), alignment(a) {}
};

KS_API Ks_Reflection_Builder ks_reflection_builder_begin(ks_str name, Ks_Meta_Kind kind, ks_size size, ks_size alignment) {

    void* builder = ks_alloc_debug(sizeof(ReflectionBuilderInternal), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA, "KsReflectionBuilder");

    return (Ks_Reflection_Builder)new(builder) ReflectionBuilderInternal(name, kind, size, alignment);
}

KS_API ks_no_ret ks_reflection_builder_add_field(Ks_Reflection_Builder builder, ks_str name, ks_str type_str, ks_str suffix, ks_size offset, ks_size size) {
    auto* b = (ReflectionBuilderInternal*)builder;
    std::string s_suf = suffix ? suffix : "";
    b->fields.push_back({ name, type_str, s_suf, offset, size });
    b->current_field_index = -1;
}

KS_API ks_no_ret ks_reflection_builder_add_bitfield(Ks_Reflection_Builder builder, ks_str name, ks_str type_str, uint32_t bit_offset, uint32_t bit_width) {
    auto* b = (ReflectionBuilderInternal*)builder;
    ReflectionBuilderInternal::FieldPending f = {};
    f.name = name; f.type_str = type_str;
    f.is_bitfield = true; f.bit_offset = bit_offset; f.bit_width = bit_width;
    f.offset = 0; f.size = 0;
    b->fields.push_back(f);
    b->current_field_index = -1;
}

KS_API ks_no_ret ks_reflection_builder_add_func_ptr_field(Ks_Reflection_Builder builder, ks_str name, ks_str ret_type, ks_size offset, ks_size size) {
    auto* b = (ReflectionBuilderInternal*)builder;
    ReflectionBuilderInternal::FieldPending f = {};
    f.name = name; f.type_str = "function_ptr"; f.offset = offset; f.size = size;
    f.is_func_ptr = true; f.return_type_str = ret_type;
    b->fields.push_back(f);
    b->current_field_index = (int)b->fields.size() - 1;
}

KS_API ks_no_ret ks_reflection_builder_add_enum_value(Ks_Reflection_Builder builder, ks_str name, int64_t value) {
    auto* b = (ReflectionBuilderInternal*)builder;
    b->enum_items.push_back({ name, value });
}

KS_API ks_no_ret ks_reflection_builder_set_return(Ks_Reflection_Builder builder, ks_str type_str) {
    auto* b = (ReflectionBuilderInternal*)builder;
    b->return_type_str = type_str;
}

KS_API ks_no_ret ks_reflection_builder_add_arg(Ks_Reflection_Builder builder, ks_str type_str, ks_str arg_name) {
    auto* b = (ReflectionBuilderInternal*)builder;
    if (b->kind == KS_META_FUNCTION) {
        b->func_args.push_back({ type_str, arg_name });
    }
    else if (b->current_field_index >= 0) {
        auto& f = b->fields[b->current_field_index];
        f.args.push_back({ type_str, arg_name });
    }
}

static void FillArgs(const std::vector<ReflectionBuilderInternal::FieldPending::Arg>& src_args, Ks_Func_Arg** out_ptr, ks_size* out_count) {
    if (src_args.empty()) { *out_ptr = nullptr; *out_count = 0; return; }
    *out_ptr = KsAllocArray<Ks_Func_Arg>(src_args.size());
    *out_count = src_args.size();
    for (size_t i = 0; i < src_args.size(); ++i) {
        (*out_ptr)[i].name = KsStrDup(src_args[i].name);
        (*out_ptr)[i].type_str = KsStrDup(src_args[i].type);
        TypeParseResult res = ParseTypeString(src_args[i].type);
        (*out_ptr)[i].type = ResolveBaseType(res.base_name);
    }
}

KS_API ks_no_ret ks_reflection_builder_end(Ks_Reflection_Builder builder) {
    auto* b = (ReflectionBuilderInternal*)builder;

    std::lock_guard<std::mutex> lock(g_reflection_mutex);

    Ks_Type_Info* info = (Ks_Type_Info*)ks_alloc_debug(sizeof(Ks_Type_Info), KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA, "KsTypeInfo");

    info->name = KsStrDup(b->name);
    info->kind = b->kind;
    info->size = b->size;
    info->alignment = b->alignment;
    info->fields = nullptr; info->field_count = 0;
    info->enum_items = nullptr; info->enum_count = 0;
    info->args = nullptr; info->arg_count = 0; info->return_type = KS_TYPE_VOID;
    info->return_type_str = nullptr;

    if (!b->fields.empty()) {
        Ks_Field_Info* arr = KsAllocArray<Ks_Field_Info>(b->fields.size());
        for (size_t i = 0; i < b->fields.size(); ++i) {
            auto& dst = arr[i]; auto& src = b->fields[i];

            dst.name = KsStrDup(src.name);
            dst.type_str = KsStrDup(src.type_str);
            dst.offset = src.offset;
            dst.size = src.size;

            TypeParseResult res = ParseTypeString(src.type_str);
            dst.type = ResolveBaseType(res.base_name);
            dst.modifiers = res.modifiers;
            dst.ptr_depth = res.ptr_depth;

            ParseArrayDims(src.suffix.c_str(), &dst);

            dst.is_bitfield = src.is_bitfield;
            dst.bit_offset = src.bit_offset;
            dst.bit_width = src.bit_width;

            dst.is_function_ptr = src.is_func_ptr;
            if (src.is_func_ptr) {
                dst.return_type_str = KsStrDup(src.return_type_str);
                TypeParseResult ret_res = ParseTypeString(src.return_type_str);
                dst.return_type = ResolveBaseType(ret_res.base_name);
                FillArgs(src.args, (Ks_Func_Arg**)&dst.args, &dst.arg_count);
            }
            else {
                dst.args = nullptr; dst.arg_count = 0; dst.return_type_str = nullptr;
            }
        }
        info->fields = arr;
        info->field_count = b->fields.size();
    }

    if (!b->enum_items.empty()) {
        Ks_Enum_Item* arr = KsAllocArray<Ks_Enum_Item>(b->enum_items.size());
        for (size_t i = 0; i < b->enum_items.size(); ++i) {
            arr[i].name = KsStrDup(b->enum_items[i].name);
            arr[i].value = b->enum_items[i].value;
        }
        info->enum_items = arr;
        info->enum_count = b->enum_items.size();
    }

    if (b->kind == KS_META_FUNCTION) {
        info->return_type_str = KsStrDup(b->return_type_str);
        TypeParseResult ret_res = ParseTypeString(b->return_type_str);
        info->return_type = ResolveBaseType(ret_res.base_name);

        std::vector<ReflectionBuilderInternal::FieldPending::Arg> tmp_args;
        for (auto& fa : b->func_args) tmp_args.push_back({ fa.type_str, fa.name });
        FillArgs(tmp_args, (Ks_Func_Arg**)&info->args, &info->arg_count);
    }

    g_registry[b->name] = info;
    b->~ReflectionBuilderInternal();
    ks_dealloc(b);
}

KS_API ks_no_ret ks_reflection_register_typedef(ks_str existing, ks_str alias) {
    if (!existing || !alias) return;
    std::lock_guard<std::mutex> lock(g_reflection_mutex);
    g_typedefs[alias] = existing;
}

KS_API const Ks_Type_Info* ks_reflection_get_type(ks_str name) {
    auto it = g_registry.find(name);
    return (it != g_registry.end()) ? it->second : nullptr;
}