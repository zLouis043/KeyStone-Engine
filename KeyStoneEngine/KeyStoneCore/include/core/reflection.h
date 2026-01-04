#pragma once

#include "types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KS_REFLECT_MAX_DIMS 4

typedef enum {
    KS_META_STRUCT,
    KS_META_UNION,
    KS_META_ENUM,
    KS_META_FUNCTION
} Ks_Meta_Kind;

typedef enum {
    KS_MOD_NONE = 0,
    KS_MOD_CONST = 1 << 0,
    KS_MOD_VOLATILE = 1 << 1,
    KS_MOD_STATIC = 1 << 2,
    KS_MOD_ATOMIC = 1 << 3
} Ks_Modifier_Flags;

typedef struct {
    const char* name;
    Ks_Type type;
    const char* type_str;
} Ks_Func_Arg;

typedef struct {
    const char* name;
    Ks_Type type;
    const char* type_str;

    ks_size offset;
    ks_size size;

    uint32_t modifiers;
    int32_t ptr_depth;

    ks_bool is_array;
    ks_uint32 dim_count;
    ks_size dims[KS_REFLECT_MAX_DIMS];
    ks_size total_element_count;

    ks_bool is_function_ptr;
    Ks_Type return_type;
    const char* return_type_str;
    const Ks_Func_Arg* args;
    ks_size arg_count;

    ks_bool is_bitfield;
    ks_uint32 bit_offset;
    ks_uint32 bit_width;

} Ks_Field_Info;

typedef struct {
    const char* name;
    int64_t value;
} Ks_Enum_Item;

typedef struct {
    const char* name;
    Ks_Meta_Kind kind;
    ks_size size;
    ks_size alignment;

    const Ks_Field_Info* fields;
    ks_size field_count;

    const Ks_Enum_Item* enum_items;
    ks_size enum_count;

    Ks_Type return_type;
    const char* return_type_str;
    const Ks_Func_Arg* args;
    ks_size arg_count;

} Ks_Type_Info;

typedef ks_ptr Ks_Reflection_Builder;

KS_API ks_no_ret ks_reflection_init(void);

KS_API ks_no_ret ks_reflection_shutdown(void);

KS_API Ks_Reflection_Builder ks_reflection_builder_begin(ks_str name, Ks_Meta_Kind kind, ks_size size, ks_size alignment);
KS_API ks_no_ret ks_reflection_builder_add_field(Ks_Reflection_Builder builder, ks_str name, ks_str type_str, ks_str suffix, ks_size offset, ks_size size);
KS_API ks_no_ret ks_reflection_builder_add_func_ptr_field(Ks_Reflection_Builder builder, ks_str name, ks_str ret_type, ks_size offset, ks_size size);
KS_API ks_no_ret ks_reflection_builder_add_enum_value(Ks_Reflection_Builder builder, ks_str name, int64_t value);
KS_API ks_no_ret ks_reflection_builder_set_return(Ks_Reflection_Builder builder, ks_str type_str);
KS_API ks_no_ret ks_reflection_builder_add_arg(Ks_Reflection_Builder builder, ks_str type_str, ks_str arg_name);
KS_API ks_no_ret ks_reflection_builder_add_bitfield(Ks_Reflection_Builder builder, ks_str name, ks_str type_str, uint32_t bit_offset, uint32_t bit_width);
KS_API ks_no_ret ks_reflection_builder_end(Ks_Reflection_Builder builder);

KS_API ks_no_ret ks_reflection_register_typedef(ks_str existing, ks_str alias);
KS_API const Ks_Type_Info* ks_reflection_get_type(ks_str name);

#ifdef __cplusplus
#define KS_ALIGNOF(T) alignof(T)
#else
#ifdef _MSC_VER
#define KS_ALIGNOF(T) __alignof(T)
#else
#define KS_ALIGNOF(T) _Alignof(T)
#endif
#endif

#define KS_EXPAND(x) x
#define KS_GET_MACRO_3(_1, _2, _3, NAME, ...) NAME

#define ks_reflect_typedef(Existing, Alias) ks_reflection_register_typedef(#Existing, #Alias)

#define ks_reflect_enum(EnumName, ...) \
    do { \
        Ks_Reflection_Builder _b = ks_reflection_builder_begin(#EnumName, KS_META_ENUM, sizeof(EnumName), sizeof(int)); \
        __VA_ARGS__; \
        ks_reflection_builder_end(_b); \
    } while(0)

#define ks_enum_value(Value) ks_reflection_builder_add_enum_value(_b, #Value, (int64_t)Value)

#define ks_reflect_field_2(Type, Name) \
    ks_reflection_builder_add_field(_b, #Name, #Type, (const char*)0, \
                                    offsetof(_Ks_Reflect_Context, Name), \
                                    sizeof(((_Ks_Reflect_Context*)0)->Name))

#define ks_reflect_field_3(Type, Name, Suffix) \
    ks_reflection_builder_add_field(_b, #Name, #Type, #Suffix, \
                                    offsetof(_Ks_Reflect_Context, Name), \
                                    sizeof(((_Ks_Reflect_Context*)0)->Name))

#define ks_reflect_field(...) KS_EXPAND(KS_GET_MACRO_3(__VA_ARGS__, ks_reflect_field_3, ks_reflect_field_2)(__VA_ARGS__))

#define ks_reflect_bitfield(Type, Name, BitOffset, BitWidth) \
    ks_reflection_builder_add_bitfield(_b, #Name, #Type, BitOffset, BitWidth)

#define ks_reflect_func_ptr(Name, RetType, ...) \
    (ks_reflection_builder_add_func_ptr_field(_b, #Name, #RetType, \
            offsetof(_Ks_Reflect_Context, Name), sizeof(void*)), \
    __VA_ARGS__)

#define ks_reflect_arg(Type, Name) ks_reflection_builder_add_arg(_b, #Type, #Name)

#define ks_reflect_struct(StructName, ...) \
    do { \
        typedef StructName _Ks_Reflect_Context; \
        Ks_Reflection_Builder _b = ks_reflection_builder_begin(#StructName, KS_META_STRUCT, sizeof(StructName), KS_ALIGNOF(StructName)); \
        __VA_ARGS__; \
        ks_reflection_builder_end(_b); \
    } while(0)

#define ks_reflect_union(UnionName, ...) \
    do { \
        typedef UnionName _Ks_Reflect_Context; \
        Ks_Reflection_Builder _b = ks_reflection_builder_begin(#UnionName, KS_META_UNION, sizeof(UnionName), KS_ALIGNOF(UnionName)); \
        __VA_ARGS__; \
        ks_reflection_builder_end(_b); \
    } while(0)

#define ks_reflect_function(FuncName, RetType, ...) \
    do { \
        Ks_Reflection_Builder _b = ks_reflection_builder_begin(#FuncName, KS_META_FUNCTION, sizeof(void*), sizeof(void*)); \
        ks_reflection_builder_set_return(_b, #RetType); \
        __VA_ARGS__; \
        ks_reflection_builder_end(_b); \
    } while(0)

#ifdef __cplusplus
}
#endif