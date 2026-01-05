#pragma once

#include "types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#include <initializer_list> 
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

    typedef enum {
        KS_FUNC_METHOD,
        KS_FUNC_STATIC,
        KS_FUNC_CONSTRUCTOR,
        KS_FUNC_DESTRUCTOR
    } Ks_Func_Kind;

    typedef struct {
        const char* type;
        const char* name;
    } Ks_Arg_Def;

#ifdef __cplusplus
}
#include <stdio.h>

struct Ks_Args_View {
    const Ks_Arg_Def* args;
    ks_size count;

    Ks_Args_View(std::initializer_list<Ks_Arg_Def> l)
        : args(l.begin()), count((ks_size)l.size()) {
    }

    Ks_Args_View() : args(nullptr), count(0) {}
};
extern "C" {
#else
    typedef struct {
        const Ks_Arg_Def* args;
        ks_size count;
    } Ks_Args_View;
#endif

    typedef struct {
        const char* name;
        Ks_Type type;
        const char* type_str;
    } Ks_Func_Arg;

    typedef struct {
        const char* name;
        void* func_ptr;
        Ks_Func_Kind kind;
        Ks_Type return_type;
        const char* return_type_str;
        const Ks_Func_Arg* args;
        ks_size arg_count;
    } Ks_VTable_Entry;

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
        const Ks_VTable_Entry* vtable;
        ks_size vtable_count;
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
    KS_API Ks_Reflection_Builder ks_reflection_builder_begin_func(ks_str name, Ks_Meta_Kind kind, ks_str ret_type, Ks_Args_View args);
    KS_API ks_no_ret ks_reflection_builder_add_field(Ks_Reflection_Builder builder, ks_str name, ks_str type_str, ks_str suffix, ks_size offset, ks_size size);

    KS_API ks_no_ret ks_reflection_builder_add_func_ptr_field(Ks_Reflection_Builder builder, ks_str name, ks_str ret_type, ks_size offset, ks_size size, Ks_Args_View args);
    KS_API ks_no_ret ks_reflection_builder_add_vtable_entry(Ks_Reflection_Builder builder, ks_str name, void* func_ptr, Ks_Func_Kind kind, ks_str ret_type, Ks_Args_View args);

    KS_API ks_no_ret ks_reflection_builder_add_enum_value(Ks_Reflection_Builder builder, ks_str name, int64_t value);
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
#define KS_GET_MACRO_3(_1, _2, _3, name, ...) name

#define ks_arg(type, name) { #type, #name }

#ifdef __cplusplus
#define ks_args(...) Ks_Args_View{ __VA_ARGS__ }
#define ks_no_args() Ks_Args_View()
#else
#define ks_args(...) (Ks_Args_View){ (Ks_Arg_Def[]){ __VA_ARGS__ }, sizeof((Ks_Arg_Def[]){ __VA_ARGS__ }) / sizeof(Ks_Arg_Def) }
#define ks_no_args() (Ks_Args_View){ 0, 0 }
#endif
#define ks_reflect_typedef(existing, alias) ks_reflection_register_typedef(#existing, #alias)

#define ks_reflect_enum(enum_name, ...) \
    do { \
        Ks_Reflection_Builder _b = ks_reflection_builder_begin(#enum_name, KS_META_ENUM, sizeof(enum_name), sizeof(int)); \
        __VA_ARGS__; \
        ks_reflection_builder_end(_b); \
    } while(0)

#define ks_enum_value(value) ks_reflection_builder_add_enum_value(_b, #value, (int64_t)value)

#define ks_reflect_field_2(Type, name) \
    ks_reflection_builder_add_field(_b, #name, #Type, (const char*)0, \
                                    offsetof(_Ks_Reflect_Context, name), \
                                    sizeof(((_Ks_Reflect_Context*)0)->name))

#define ks_reflect_field_3(Type, name, Suffix) \
    ks_reflection_builder_add_field(_b, #name, #Type, #Suffix, \
                                    offsetof(_Ks_Reflect_Context, name), \
                                    sizeof(((_Ks_Reflect_Context*)0)->name))

#define ks_reflect_field(...) KS_EXPAND(KS_GET_MACRO_3(__VA_ARGS__, ks_reflect_field_3, ks_reflect_field_2)(__VA_ARGS__))

#define ks_reflect_bitfield(Type, name, BitOffset, BitWidth) \
    ks_reflection_builder_add_bitfield(_b, #name, #Type, BitOffset, BitWidth)

#define ks_reflect_func_ptr(name, ret_type, ...) \
    (ks_reflection_builder_add_func_ptr_field(_b, #name, #ret_type, \
            offsetof(_Ks_Reflect_Context, name), sizeof(void*), \
            __VA_ARGS__), 0)

#define ks_reflect_arg(Type, name) ks_reflection_builder_add_arg(_b, #Type, #name)

#define ks_reflect_struct(Structname, ...) \
    do { \
        typedef Structname _Ks_Reflect_Context; \
        Ks_Reflection_Builder _b = ks_reflection_builder_begin(#Structname, KS_META_STRUCT, sizeof(Structname), KS_ALIGNOF(Structname)); \
        __VA_ARGS__; \
        ks_reflection_builder_end(_b); \
    } while(0)

#define ks_reflect_union(union_name, ...) \
    do { \
        typedef union_name _Ks_Reflect_Context; \
        Ks_Reflection_Builder _b = ks_reflection_builder_begin(#union_name, KS_META_UNION, sizeof(union_name), KS_ALIGNOF(union_name)); \
        __VA_ARGS__; \
        ks_reflection_builder_end(_b); \
    } while(0)

#define ks_reflect_function(Funcname, ret_type, ...) \
    do { \
        Ks_Reflection_Builder _b = ks_reflection_builder_begin_func(#Funcname, KS_META_FUNCTION, #ret_type, __VA_ARGS__); \
        ks_reflection_builder_end(_b); \
    } while(0)

#define ks_reflect_vtable_begin(Type) 0
#define ks_reflect_vtable_end() 0

#define ks_reflect_method(func_ptr, ret_type, ...) \
    (ks_reflection_builder_add_vtable_entry(_b, #func_ptr, (void*)func_ptr, KS_FUNC_METHOD, #ret_type, __VA_ARGS__), 0)

#define ks_reflect_static_method(func_ptr, ret_type, ...) \
    (ks_reflection_builder_add_vtable_entry(_b, #func_ptr, (void*)func_ptr, KS_FUNC_STATIC, #ret_type, __VA_ARGS__), 0)

#define ks_reflect_method_named(name, func_ptr, ret_type, ...) \
    (ks_reflection_builder_add_vtable_entry(_b, name, (void*)func_ptr, KS_FUNC_METHOD, #ret_type, __VA_ARGS__), 0)

#define ks_reflect_static_method_named(name, func_ptr, ret_type, ...) \
    (ks_reflection_builder_add_vtable_entry(_b, name, (void*)func_ptr, KS_FUNC_STATIC, #ret_type, __VA_ARGS__), 0)

#define ks_reflect_constructor(func_ptr, ...) \
    (ks_reflection_builder_add_vtable_entry(_b, NULL, (void*)func_ptr, KS_FUNC_CONSTRUCTOR, "void", __VA_ARGS__), 0)

#define ks_reflect_destructor(func_ptr) \
    (ks_reflection_builder_add_vtable_entry(_b, NULL, (void*)func_ptr, KS_FUNC_DESTRUCTOR, "void", ks_no_args()), 0)

#ifdef __cplusplus
}
#endif