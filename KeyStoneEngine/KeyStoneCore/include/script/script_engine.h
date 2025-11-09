#pragma once

#include "../core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_Script_Ctx;
typedef ks_ptr Ks_Script_Userytype_Builder;
typedef ks_int Ks_Script_Ref;
typedef ks_int ks_returns_count;
typedef ks_size ks_stack_idx;
typedef ks_size ks_upvalue_idx;
typedef ks_returns_count (*ks_script_cfunc)(Ks_Script_Ctx ctx);
typedef ks_no_ret (*ks_script_deallocator)(ks_ptr data, ks_size size);

#define KS_SCRIPT_INVALID_REF (-1)
#define KS_SCRIPT_NO_REF (-2)

typedef enum {
  KS_SCRIPT_OBJECT_VALID,
  KS_SCRIPT_OBJECT_INVALID,
  KS_SCRIPT_OBJECT_MOVED,
  KS_SCRIPT_OBJECT_DESTROYED
} Ks_Script_Object_State;

typedef enum {
  KS_SCRIPT_OBJECT_TYPE_UNKNOWN,
  KS_SCRIPT_OBJECT_TYPE_NIL,
  KS_SCRIPT_OBJECT_TYPE_STRING,
  KS_SCRIPT_OBJECT_TYPE_NUMBER,
  KS_SCRIPT_OBJECT_TYPE_BOOLEAN,
  KS_SCRIPT_OBJECT_TYPE_TABLE,
  KS_SCRIPT_OBJECT_TYPE_FUNCTION,
  KS_SCRIPT_OBJECT_TYPE_COROUTINE,
  KS_SCRIPT_OBJECT_TYPE_USERDATA,
  KS_SCRIPT_OBJECT_TYPE_LIGHTUSERDATA
} Ks_Script_Object_Type;

typedef struct {
  union {
    ks_double number;
    ks_bool boolean;
    ks_ptr lightuserdata;
    Ks_Script_Ref string_ref;
    Ks_Script_Ref table_ref;
    Ks_Script_Ref function_ref;
    Ks_Script_Ref coroutine_ref;
    Ks_Script_Ref userdata_ref;
    Ks_Script_Ref generic_ref;
  } val;
  Ks_Script_Object_State state;
  Ks_Script_Object_Type type;
} Ks_Script_Object;

typedef Ks_Script_Object Ks_Script_Table;
typedef Ks_Script_Object Ks_Script_Function;
typedef Ks_Script_Object Ks_Script_Coroutine;
typedef Ks_Script_Object Ks_Script_Userdata;
typedef Ks_Script_Object Ks_Script_LightUserdata;
typedef Ks_Script_Object Ks_Script_Function_Call_Result;

typedef enum {
  KS_SCRIPT_MT_INDEX,
  KS_SCRIPT_MT_NEWINDEX,
  KS_SCRIPT_MT_CALL,
  KS_SCRIPT_MT_ADD,
  KS_SCRIPT_MT_SUB,
  KS_SCRIPT_MT_MUL,
  KS_SCRIPT_MT_IDIV,
  KS_SCRIPT_MT_DIV,
  KS_SCRIPT_MT_MOD,
  KS_SCRIPT_MT_POW,
  KS_SCRIPT_MT_UNM,
  KS_SCRIPT_MT_BNOT,
  KS_SCRIPT_MT_BAND,
  KS_SCRIPT_MT_BOR,
  KS_SCRIPT_MT_BXOR,
  KS_SCRIPT_MT_SHL,
  KS_SCRIPT_MT_SHR,
  KS_SCRIPT_MT_EQ,
  KS_SCRIPT_MT_LT,
  KS_SCRIPT_MT_LE,
  KS_SCRIPT_MT_CONCAT,
  KS_SCRIPT_MT_LEN,
  KS_SCRIPT_MT_TOSTRING,
  KS_SCRIPT_MT_GC,
  KS_SCRIPT_MT_CLOSE
} Ks_Script_Metamethod;

typedef struct {
  Ks_Script_Ref table_ref;
  Ks_Script_Ref current_key_ref;
  ks_bool iter_started;
  ks_bool valid;
} Ks_Script_Table_Iterator;

typedef enum {
  KS_SCRIPT_ERROR_NONE,
  KS_SCRIPT_ERROR_CTX_NOT_CREATED,
  KS_SCRIPT_ERROR_MEMORY,
  KS_SCRIPT_ERROR_RUNTIME,
  KS_SCRIPT_ERROR_STACK_OVERFLOW,
  KS_SCRIPT_ERROR_INVALID_OPERATION,
  KS_SCRIPT_ERROR_INVALID_ARGUMENT,
  KS_SCRIPT_ERROR_INVALID_OBJECT,
  KS_SCRIPT_ERROR_SYMBOL_NOT_FOUND,
} Ks_Script_Error;

typedef struct {
  Ks_Script_Error error;
  ks_str message;
} Ks_Script_Error_Info;

typedef enum {
  KS_SCRIPT_COROUTINE_NORMAL,
  KS_SCRIPT_COROUTINE_SUSPENDED,
  KS_SCRIPT_COROUTINE_RUNNING,
  KS_SCRIPT_COROUTINE_DEAD,
  KS_SCRIPT_COROUTINE_ERROR
} Ks_Script_Coroutine_Status;

KS_API Ks_Script_Ctx ks_script_create_ctx();
KS_API ks_no_ret ks_script_destroy_ctx(Ks_Script_Ctx ctx);

KS_API ks_no_ret ks_script_begin_scope(Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_script_end_scope(Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_script_promote(Ks_Script_Ctx ctx, Ks_Script_Object obj);

KS_API Ks_Script_Object ks_script_create_number(Ks_Script_Ctx ctx, ks_double val);
KS_API Ks_Script_Object ks_script_create_boolean(Ks_Script_Ctx ctx, ks_bool val);
KS_API Ks_Script_Object ks_script_create_nil(Ks_Script_Ctx ctx);
KS_API Ks_Script_Object ks_script_create_invalid_obj(Ks_Script_Ctx ctx);

KS_API Ks_Script_Function ks_script_create_cfunc(Ks_Script_Ctx ctx, ks_script_cfunc f);
KS_API Ks_Script_Function ks_script_create_cfunc_with_upvalues(Ks_Script_Ctx ctx,
                                                        ks_script_cfunc f,
                                                        ks_size n_upvalues);

KS_API Ks_Script_Table ks_script_create_table(Ks_Script_Ctx ctx);
KS_API Ks_Script_Table ks_script_create_table_with_capacity(Ks_Script_Ctx ctx,
                                                     ks_size array_sz,
                                                     ks_size hash_sz);
KS_API Ks_Script_Table ks_script_create_named_table(Ks_Script_Ctx ctx, ks_str name);

KS_API Ks_Script_Object ks_script_create_cstring(Ks_Script_Ctx ctx, ks_str val);
KS_API Ks_Script_Object ks_script_create_lstring(Ks_Script_Ctx ctx, ks_str str,
                                          ks_size len);
KS_API Ks_Script_Userdata ks_script_create_userdata(Ks_Script_Ctx ctx, ks_size size);

KS_API ks_no_ret ks_script_free_obj(Ks_Script_Ctx ctx, Ks_Script_Object obj);

KS_API Ks_Script_Error ks_script_get_last_error(Ks_Script_Ctx ctx);
KS_API ks_str ks_script_get_last_error_str(Ks_Script_Ctx ctx);
KS_API Ks_Script_Error_Info ks_script_get_last_error_info(Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_script_clear_error(Ks_Script_Ctx ctx);

KS_API ks_no_ret ks_script_set_global(Ks_Script_Ctx ctx, ks_str name,
                               Ks_Script_Object val);
KS_API Ks_Script_Object ks_script_get_global(Ks_Script_Ctx ctx, ks_str name);

KS_API Ks_Script_Function ks_script_load_string(Ks_Script_Ctx ctx, ks_str string);
KS_API Ks_Script_Function ks_script_load_file(Ks_Script_Ctx ctx, ks_str file_path);

KS_API Ks_Script_Function_Call_Result ks_script_do_string(Ks_Script_Ctx ctx,
                                                   ks_str string);
KS_API Ks_Script_Function_Call_Result ks_script_do_file(Ks_Script_Ctx ctx,
                                                 ks_str file_path);

KS_API Ks_Script_Object ks_script_require(Ks_Script_Ctx ctx, ks_str module_name);
KS_API ks_no_ret ks_script_register_module(Ks_Script_Ctx ctx, ks_str name,
                                    Ks_Script_Table module);
KS_API ks_no_ret ks_script_add_package_path(Ks_Script_Ctx ctx, ks_str path);

KS_API ks_no_ret ks_script_gc_collect(Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_script_gc_stop(Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_script_gc_restart(Ks_Script_Ctx ctx);
KS_API ks_size ks_script_get_mem_used(Ks_Script_Ctx ctx);

KS_API ks_no_ret ks_script_dump_registry(Ks_Script_Ctx ctx);

KS_API ks_no_ret ks_script_stack_push_number(Ks_Script_Ctx ctx, ks_double val);
KS_API ks_no_ret ks_script_stack_push_boolean(Ks_Script_Ctx ctx, ks_bool val);
KS_API ks_no_ret ks_script_stack_push_string(Ks_Script_Ctx ctx, ks_str val);
KS_API ks_no_ret ks_script_stack_push_obj(Ks_Script_Ctx ctx, Ks_Script_Object val);

KS_API ks_double ks_script_stack_pop_number(Ks_Script_Ctx ctx);
KS_API ks_bool ks_script_stack_pop_boolean(Ks_Script_Ctx ctx);
KS_API ks_str ks_script_stack_pop_string(Ks_Script_Ctx ctx);
KS_API Ks_Script_Object ks_script_stack_pop_obj(Ks_Script_Ctx ctx);
KS_API Ks_Script_Object ks_script_stack_get_top(Ks_Script_Ctx ctx);
KS_API Ks_Script_Object ks_script_stack_peek(Ks_Script_Ctx ctx, ks_stack_idx i);
KS_API Ks_Script_Object ks_script_stack_get(Ks_Script_Ctx ctx, ks_stack_idx i);

KS_API ks_size ks_script_stack_size(Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_script_stack_clear(Ks_Script_Ctx ctx);
KS_API ks_no_ret ks_script_stack_remove(Ks_Script_Ctx ctx, ks_stack_idx i);
KS_API ks_no_ret ks_script_stack_insert(Ks_Script_Ctx ctx, ks_stack_idx i);
KS_API ks_no_ret ks_script_stack_replace(Ks_Script_Ctx ctx, ks_stack_idx i);
KS_API ks_no_ret ks_script_stack_copy(Ks_Script_Ctx ctx, ks_stack_idx from, ks_stack_idx to);

KS_API ks_no_ret ks_script_stack_dump(Ks_Script_Ctx ctx);

KS_API Ks_Script_Object_Type ks_script_obj_type(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_bool ks_script_obj_is(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Object_Type type);
KS_API ks_double ks_script_obj_as_number(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_bool ks_script_obj_as_boolean(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_str ks_script_obj_as_str(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API Ks_Script_Table ks_script_obj_as_table(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API Ks_Script_Function ks_script_obj_as_function(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine(Ks_Script_Ctx ctx, Ks_Script_Object obj);

KS_API ks_double ks_script_obj_as_number_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_double def);
KS_API ks_bool ks_script_obj_as_boolean_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_bool def);
KS_API ks_str ks_script_obj_as_str_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str def);
KS_API Ks_Script_Table ks_script_obj_as_table_or(Ks_Script_Ctx ctx, Ks_Script_Object obj,
                                          Ks_Script_Table def);
KS_API Ks_Script_Function ks_script_as_function_or(Ks_Script_Ctx ctx, Ks_Script_Object obj,
                                            Ks_Script_Function def);
KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine_or(Ks_Script_Ctx ctx, Ks_Script_Object obj,
                                                  Ks_Script_Coroutine def);

KS_API ks_bool ks_script_obj_try_as_number(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_double *out);
KS_API ks_bool ks_script_obj_try_as_boolean(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_bool *out);
KS_API ks_bool ks_script_obj_try_as_string(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str *out);
KS_API ks_bool ks_script_obj_try_as_table(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table *out);
KS_API ks_bool ks_script_obj_try_as_function(Ks_Script_Ctx ctx, Ks_Script_Object obj,
                                      Ks_Script_Function *out);
KS_API ks_bool ks_script_obj_try_as_coroutine(Ks_Script_Ctx ctx, Ks_Script_Object obj,
                                       Ks_Script_Coroutine *out);

KS_API ks_bool ks_script_obj_has_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API Ks_Script_Table ks_script_obj_get_metatable(Ks_Script_Ctx ctx,
                                            Ks_Script_Object obj);
KS_API ks_no_ret ks_script_obj_set_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj,
                                      Ks_Script_Table mt);

KS_API ks_bool ks_script_obj_is_callable(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_bool ks_script_obj_is_iterable(Ks_Script_Ctx ctx, Ks_Script_Object obj);

KS_API ks_no_ret ks_script_obj_dump(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_str ks_script_obj_to_string(Ks_Script_Ctx ctx, Ks_Script_Object obj);

KS_API ks_bool ks_script_table_has(Ks_Script_Ctx ctx, Ks_Script_Table tbl,
                            Ks_Script_Object key);
KS_API ks_no_ret ks_script_table_set(Ks_Script_Ctx ctx, Ks_Script_Table tbl,
                              Ks_Script_Object key, Ks_Script_Object value);
KS_API Ks_Script_Object ks_script_table_get(Ks_Script_Ctx ctx, Ks_Script_Table tbl,
                                     Ks_Script_Object key);
KS_API ks_size ks_script_table_array_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl);
KS_API ks_size ks_script_table_total_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl);
KS_API Ks_Script_Table_Iterator ks_script_table_iterate(Ks_Script_Ctx ctx,
                                                 Ks_Script_Table tbl);

KS_API ks_bool ks_script_iterator_has_next(Ks_Script_Ctx ctx,
                                    Ks_Script_Table_Iterator* iterator);
KS_API ks_bool ks_script_iterator_next(Ks_Script_Ctx ctx,
                                  Ks_Script_Table_Iterator *iterator,
                                  Ks_Script_Object *key,
                                  Ks_Script_Object *value);
KS_API ks_no_ret ks_script_iterator_destroy(Ks_Script_Ctx ctx,
                                     Ks_Script_Table_Iterator *iterator);
KS_API ks_no_ret ks_script_iterator_reset(Ks_Script_Ctx ctx,
                                   Ks_Script_Table_Iterator *iterator);
KS_API Ks_Script_Table_Iterator
KS_API ks_script_iterator_clone(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator *iterator);

KS_API ks_no_ret ks_script_func_call(Ks_Script_Ctx ctx, Ks_Script_Function f,
                              ks_size n_args, ks_size n_rets);

#define ks_script_func_callv(ctx, func, ...)                                   \
  ks_script_func_callv_impl(ctx, func, __VA_ARGS__,                            \
                            ks_script_create_invalid_obj(ctx));
KS_API Ks_Script_Function_Call_Result
ks_script_func_callv_impl(Ks_Script_Ctx ctx, Ks_Script_Function f, ...);

KS_API Ks_Script_Object ks_script_func_get_upvalue(Ks_Script_Ctx ctx,
                                            ks_upvalue_idx i);

KS_API ks_bool ks_script_call_succeded(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res);
KS_API Ks_Script_Object ks_script_call_get_return(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res);
KS_API ks_size ks_script_call_get_returns_count(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res);
KS_API Ks_Script_Object
ks_script_call_get_return_at(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res, ks_size idx);

KS_API Ks_Script_Coroutine ks_script_create_coroutine(Ks_Script_Ctx ctx, Ks_Script_Function func);
KS_API Ks_Script_Coroutine_Status ks_script_coroutine_status(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine);

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_resume(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine, ks_size n_args);

#define ks_script_coroutine_resumev(ctx, coroutine, ...) \
    ks_script_coroutine_resumev_impl(ctx, coroutine, __VA_ARGS__, ks_script_create_invalid_obj(ctx))

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_resumev_impl(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine, ...);

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_yield(Ks_Script_Ctx ctx, ks_size n_results);

#define ks_script_coroutine_yieldv(ctx, ...) \
    ks_script_coroutine_yieldv_impl(ctx, __VA_ARGS__, ks_script_create_invalid_obj(ctx))

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_yieldv_impl(Ks_Script_Ctx ctx, ...);

KS_API Ks_Script_LightUserdata ks_script_create_lightuserdata(Ks_Script_Ctx ctx, ks_ptr ptr);
KS_API ks_ptr ks_script_lightuserdata_get_ptr(Ks_Script_Ctx ctx, Ks_Script_LightUserdata lud);

KS_API ks_ptr ks_script_userdata_get_ptr(Ks_Script_Ctx ctx, Ks_Script_Userdata ud);

KS_API ks_no_ret ks_script_set_type_name(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str type_name);

KS_API Ks_Script_Userytype_Builder ks_script_usertype_begin(Ks_Script_Ctx ctx, ks_str type_name);

KS_API ks_no_ret ks_script_usertype_inherits_from(Ks_Script_Userytype_Builder builder, ks_str base_type_name);

KS_API ks_no_ret ks_script_usertype_set_constructor(Ks_Script_Userytype_Builder builder, ks_script_cfunc ctor);
KS_API ks_no_ret ks_script_usertype_set_destructor(Ks_Script_Userytype_Builder builder, ks_script_deallocator dtor);

KS_API ks_no_ret ks_script_usertype_add_method(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc func);
KS_API ks_no_ret ks_script_usertype_add_overload(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc func, Ks_Script_Object_Type* args, ks_size num_args);
KS_API ks_no_ret ks_script_usertype_add_static_method(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc func);

KS_API ks_no_ret ks_script_usertype_add_property(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc getter, ks_script_cfunc setter);

KS_API ks_no_ret ks_script_usertype_add_metamethod(Ks_Script_Userytype_Builder builder, Ks_Script_Metamethod mt, ks_script_cfunc func);

KS_API ks_no_ret ks_script_usertype_end(Ks_Script_Userytype_Builder builder);

#ifdef __cplusplus
}
#endif
