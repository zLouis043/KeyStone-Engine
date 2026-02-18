#pragma once

#include "../core/types.h"
#include "../core/defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_Preprocessor;
typedef ks_ptr Ks_StringBuilder;

typedef enum Ks_AccessType {
    KS_ACCESS_DIRECT,   // symbol (es. hp)
    KS_ACCESS_DOT,      // symbol.member
    KS_ACCESS_BRACKET,  // symbol["key"] o symbol[expr]
    KS_ACCESS_COLON     // symbol:method()
} Ks_AccessType;

typedef struct Ks_Preproc_Ctx {
    ks_str symbol_name;
    ks_str decorator_name;

    ks_str* decorator_args;
    ks_str* decorator_arg_keys;
    ks_size decorator_args_count;

    ks_str expression;

    ks_str* function_args;
    ks_size function_args_count;
    ks_str function_body;

    ks_str assignment_value;

    ks_str* table_fields;
    ks_size table_fields_count;

    Ks_AccessType access_type; // Come viene acceduto il simbolo
    ks_str member_key;         // Il nome del membro ("hp") o il contenuto delle quadre ("'sword'") o del metodo

    ks_ptr lua_ctx;

    ks_bool is_local_def;
    ks_bool is_func_def;
    ks_bool is_table_def;
    ks_bool is_transformator_in_lua;

    ks_int decorator_index;
} Ks_Preproc_Ctx;

typedef ks_bool(*ks_preproc_transform_fn)(const Ks_Preproc_Ctx* ctx, Ks_StringBuilder* out);

KS_API ks_no_ret ks_sb_append(Ks_StringBuilder sb, ks_str text);
KS_API ks_no_ret ks_sb_appendf(Ks_StringBuilder sb, ks_str fmt, ...);

KS_API Ks_Preprocessor ks_preprocessor_create(ks_ptr lua_ctx);
KS_API ks_no_ret ks_preprocessor_destroy(Ks_Preprocessor pp);

KS_API ks_no_ret ks_preprocessor_register(
    Ks_Preprocessor pp,
    ks_str name,
    ks_preproc_transform_fn on_def,
    ks_preproc_transform_fn on_set,
    ks_preproc_transform_fn on_get,
    ks_preproc_transform_fn on_call
);

KS_API ks_str ks_preprocessor_process(Ks_Preprocessor pp, ks_str source_code);

#ifdef __cplusplus
}
#endif