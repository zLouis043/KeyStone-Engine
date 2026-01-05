#include "../../include/core/reflection.h"
#include "../../include/memory/memory.h"

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
    {"int64_t", KS_TYPE_INT}, {"uint64_t", KS_TYPE_UINT},
    {"short", KS_TYPE_INT}, {"long", KS_TYPE_INT}
};

static const char* KsStrDup(const std::string& str) {
    if (str.empty()) return nullptr;
    size_t len = str.length() + 1;
    char* mem = (char*)ks_alloc(len, KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA);
    memcpy(mem, str.c_str(), len);
    return mem;
}

template<typename T>
T* KsAllocArray(size_t count) {
    if (count == 0) return nullptr;
    return (T*)ks_alloc(count * sizeof(T), KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA);
}

struct TypeParseResult {
    std::string base_name;
    uint32_t modifiers;
    int ptr_depth;
};

static TypeParseResult ParseTypeString(const char* type_str) {
    TypeParseResult res = { "", KS_MOD_NONE, 0 };
    if (!type_str) return res;
    std::string s = type_str;

    if (s.find("const") != std::string::npos)      res.modifiers |= KS_MOD_CONST;
    if (s.find("volatile") != std::string::npos)   res.modifiers |= KS_MOD_VOLATILE;
    if (s.find("static") != std::string::npos)     res.modifiers |= KS_MOD_STATIC;

    const char* kws[] = { "const", "volatile", "_Atomic", "atomic", "struct", "enum", "union", "static" };
    for (auto kw : kws) {
        size_t pos;
        while ((pos = s.find(kw)) != std::string::npos) s.erase(pos, strlen(kw));
    }

    res.ptr_depth = (int)std::count(s.begin(), s.end(), '*');

    s.erase(std::remove(s.begin(), s.end(), '*'), s.end());
    s.erase(std::remove(s.begin(), s.end(), '&'), s.end());

    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());

    res.base_name = s;
    return res;
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
    return KS_TYPE_USERDATA;
}

struct ReflectionBuilderInternal {
    std::string name;
    Ks_Meta_Kind kind;
    size_t size; size_t alignment;

    struct FieldPending {
        std::string name; std::string type_str; std::string suffix;
        size_t offset; size_t size;
        uint32_t modifiers;
        bool is_func_ptr = false;
        std::string return_type_str;
        std::vector<Ks_Func_Arg> args;
        bool is_bitfield = false;
        uint32_t bit_offset = 0; uint32_t bit_width = 0;
    };
    std::vector<FieldPending> fields;

    struct EnumPending { std::string name; int64_t value; };
    std::vector<EnumPending> enum_items;

    struct VTablePending {
        std::string name;
        void* func_ptr;
        Ks_Func_Kind kind;
        std::string return_type_str;
        std::vector<Ks_Func_Arg> args;
    };
    std::vector<VTablePending> vtable;

    std::string global_ret_type_str;
    std::vector<Ks_Func_Arg> global_args;

    ReflectionBuilderInternal(const char* n, Ks_Meta_Kind k, size_t s, size_t a)
        : name(n ? n : ""), kind(k), size(s), alignment(a) {}
};

static void ConvertArgs(const Ks_Arg_Def* src, ks_size count, std::vector<Ks_Func_Arg>& out) {
    for (ks_size i = 0; i < count; ++i) {
        Ks_Func_Arg arg;
        arg.name = KsStrDup(src[i].name ? src[i].name : "");
        arg.type_str = KsStrDup(src[i].type ? src[i].type : "");

        TypeParseResult pr = ParseTypeString(src[i].type);
        arg.type = ResolveBaseType(pr.base_name);

        if (arg.type == KS_TYPE_CHAR && pr.ptr_depth > 0) {
            arg.type = KS_TYPE_CSTRING;
        }

        out.push_back(arg);
    }
}

KS_API ks_no_ret ks_reflection_init(void) {}

KS_API ks_no_ret ks_reflection_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_reflection_mutex);
    g_registry.clear();
    g_typedefs.clear();
}

KS_API Ks_Reflection_Builder ks_reflection_builder_begin(ks_str name, Ks_Meta_Kind kind, ks_size size, ks_size alignment) {
    void* mem = ks_alloc_debug(sizeof(ReflectionBuilderInternal), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA, "KsReflectionBuilder");
    return (Ks_Reflection_Builder)new(mem) ReflectionBuilderInternal(name, kind, size, alignment);
}

KS_API Ks_Reflection_Builder ks_reflection_builder_begin_func(ks_str name, Ks_Meta_Kind kind, ks_str ret_type, Ks_Args_View args) {
    void* mem = ks_alloc_debug(sizeof(ReflectionBuilderInternal), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA, "KsReflectionBuilder");
    auto* b = new(mem) ReflectionBuilderInternal(name, kind, sizeof(void*), sizeof(void*));

    b->global_ret_type_str = ret_type ? ret_type : "void";

    if (args.args && args.count > 0) {
        ConvertArgs(args.args, args.count, b->global_args);
    }

    return (Ks_Reflection_Builder)b;
}

KS_API ks_no_ret ks_reflection_builder_add_field(Ks_Reflection_Builder builder, ks_str name, ks_str type_str, ks_str suffix, ks_size offset, ks_size size) {
    auto* b = (ReflectionBuilderInternal*)builder;
    ReflectionBuilderInternal::FieldPending f;
    f.name = name ? name : "";
    f.type_str = type_str ? type_str : "";
    f.suffix = suffix ? suffix : "";
    f.offset = offset; f.size = size;
    TypeParseResult pr = ParseTypeString(f.type_str.c_str());
    f.modifiers = pr.modifiers;
    b->fields.push_back(f);
}

KS_API ks_no_ret ks_reflection_builder_add_func_ptr_field(Ks_Reflection_Builder builder, ks_str name, ks_str ret_type, ks_size offset, ks_size size, Ks_Args_View args) {
    auto* b = (ReflectionBuilderInternal*)builder;
    ReflectionBuilderInternal::FieldPending f;
    f.name = name ? name : "";
    f.offset = offset; f.size = size;
    f.type_str = "void*";
    f.is_func_ptr = true;
    f.return_type_str = ret_type ? ret_type : "void";

    if (args.args && args.count > 0) {
        ConvertArgs(args.args, args.count, f.args);
    }

    b->fields.push_back(f);
}

KS_API ks_no_ret ks_reflection_builder_add_bitfield(Ks_Reflection_Builder builder, ks_str name, ks_str type_str, uint32_t bit_offset, uint32_t bit_width) {
    auto* b = (ReflectionBuilderInternal*)builder;
    ReflectionBuilderInternal::FieldPending f;
    f.name = name ? name : "";
    f.type_str = type_str ? type_str : "";
    f.is_bitfield = true; f.bit_offset = bit_offset; f.bit_width = bit_width;
    f.offset = 0; f.size = 0;
    TypeParseResult pr = ParseTypeString(f.type_str.c_str());
    f.modifiers = pr.modifiers;
    b->fields.push_back(f);
}

KS_API ks_no_ret ks_reflection_builder_add_enum_value(Ks_Reflection_Builder builder, ks_str name, int64_t value) {
    auto* b = (ReflectionBuilderInternal*)builder;
    b->enum_items.push_back({ name ? name : "", value });
}

KS_API ks_no_ret ks_reflection_builder_set_return(Ks_Reflection_Builder builder, ks_str type_str) {}
KS_API ks_no_ret ks_reflection_builder_add_arg(Ks_Reflection_Builder builder, ks_str type_str, ks_str arg_name) {}

KS_API ks_no_ret ks_reflection_builder_add_vtable_entry(Ks_Reflection_Builder builder, ks_str name, void* func_ptr, Ks_Func_Kind kind, ks_str ret_type, Ks_Args_View args) {
    auto* b = (ReflectionBuilderInternal*)builder;
    ReflectionBuilderInternal::VTablePending v;
    v.name = name ? name : "";
    v.func_ptr = func_ptr;
    v.kind = kind;
    v.return_type_str = ret_type ? ret_type : "void";

    if (args.args && args.count > 0) {
        ConvertArgs(args.args, args.count, v.args);
    }

    b->vtable.push_back(v);
}

static void ParseArrayDims(const std::string& suffix, Ks_Field_Info* info) {
    info->is_array = false; info->dim_count = 0; info->total_element_count = 1;
    memset(info->dims, 0, sizeof(info->dims));
    if (suffix.empty()) return;
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
        info->dim_count++; info->is_array = true; pos = end + 1;
    }
}

static void CopyArgsToPersistent(const std::vector<Ks_Func_Arg>& src, const Ks_Func_Arg** dst_ptr, ks_size* dst_count) {
    if (src.empty()) { *dst_ptr = nullptr; *dst_count = 0; return; }
    Ks_Func_Arg* arr = KsAllocArray<Ks_Func_Arg>(src.size());
    memcpy(arr, src.data(), src.size() * sizeof(Ks_Func_Arg));
    *dst_ptr = arr;
    *dst_count = src.size();
}

KS_API ks_no_ret ks_reflection_builder_end(Ks_Reflection_Builder builder) {
    auto* b = (ReflectionBuilderInternal*)builder;
    std::lock_guard<std::mutex> lock(g_reflection_mutex);

    Ks_Type_Info* info = (Ks_Type_Info*)ks_alloc_debug(sizeof(Ks_Type_Info), KS_LT_PERMANENT, KS_TAG_INTERNAL_DATA, "KsTypeInfo");
    memset(info, 0, sizeof(Ks_Type_Info));

    info->name = KsStrDup(b->name);
    info->kind = b->kind;
    info->size = b->size;
    info->alignment = b->alignment;

    if (!b->fields.empty()) {
        Ks_Field_Info* arr = KsAllocArray<Ks_Field_Info>(b->fields.size());
        for (size_t i = 0; i < b->fields.size(); ++i) {
            auto& src = b->fields[i];
            auto& dst = arr[i];
            dst.name = KsStrDup(src.name);
            dst.type_str = KsStrDup(src.type_str);
            TypeParseResult pr = ParseTypeString(src.type_str.c_str());
            dst.type = ResolveBaseType(pr.base_name);

            if (dst.type == KS_TYPE_CHAR && pr.ptr_depth > 0) dst.type = KS_TYPE_CSTRING;

            dst.offset = src.offset; dst.size = src.size;
            dst.modifiers = src.modifiers; dst.ptr_depth = pr.ptr_depth;
            ParseArrayDims(src.suffix, &dst);
            dst.is_bitfield = src.is_bitfield;
            dst.bit_offset = src.bit_offset; dst.bit_width = src.bit_width;

            dst.is_function_ptr = src.is_func_ptr;
            if (src.is_func_ptr) {
                dst.return_type_str = KsStrDup(src.return_type_str);
                TypeParseResult rpr = ParseTypeString(src.return_type_str.c_str());
                dst.return_type = ResolveBaseType(rpr.base_name);

                if (dst.return_type == KS_TYPE_CHAR && rpr.ptr_depth > 0) dst.return_type = KS_TYPE_CSTRING;

                CopyArgsToPersistent(src.args, &dst.args, &dst.arg_count);
            }
            else {
                dst.return_type = KS_TYPE_VOID; dst.return_type_str = nullptr;
                dst.args = nullptr; dst.arg_count = 0;
            }
        }
        info->fields = arr;
        info->field_count = b->fields.size();
    }

    if (!b->vtable.empty()) {
        Ks_VTable_Entry* arr = KsAllocArray<Ks_VTable_Entry>(b->vtable.size());
        for (size_t i = 0; i < b->vtable.size(); ++i) {
            auto& src = b->vtable[i];
            auto& dst = arr[i];
            dst.name = src.name.empty() ? nullptr : KsStrDup(src.name);
            dst.func_ptr = src.func_ptr;
            dst.kind = src.kind;
            dst.return_type_str = KsStrDup(src.return_type_str);
            TypeParseResult rpr = ParseTypeString(src.return_type_str.c_str());
            dst.return_type = ResolveBaseType(rpr.base_name);

            if (dst.return_type == KS_TYPE_CHAR && rpr.ptr_depth > 0) dst.return_type = KS_TYPE_CSTRING;

            CopyArgsToPersistent(src.args, (const Ks_Func_Arg**)&dst.args, &dst.arg_count);
        }
        info->vtable = arr;
        info->vtable_count = b->vtable.size();
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
        info->return_type_str = KsStrDup(b->global_ret_type_str.c_str());
        TypeParseResult rpr = ParseTypeString(b->global_ret_type_str.c_str());
        info->return_type = ResolveBaseType(rpr.base_name);

        if (info->return_type == KS_TYPE_CHAR && rpr.ptr_depth > 0) info->return_type = KS_TYPE_CSTRING;

        CopyArgsToPersistent(b->global_args, &info->args, &info->arg_count);
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
    std::lock_guard<std::mutex> lock(g_reflection_mutex);
    if (!name) return nullptr;
    auto it = g_registry.find(name);
    return (it != g_registry.end()) ? it->second : nullptr;
}