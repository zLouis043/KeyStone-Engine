#pragma once

#include "../core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_Script_Ctx;
typedef ks_int Ks_Script_Ref;
typedef ks_int ks_returns_count;
typedef ks_size ks_stack_idx;
typedef ks_size ks_upvalue_idx;
typedef ks_returns_count (*ks_cfunc)(Ks_Script_Ctx ctx);
typedef ks_no_ret (*ks_deallocator)(ks_ptr data, ks_size size);

#define KS_SCRIPT_INVALID_REF (-1)
#define KS_SCRIPT_NO_REF (-2)

typedef enum {
  KS_SCRIPT_OBJECT_VALID,
  KS_SCRIPT_OBJECT_INVALID,
  KS_SCRIPT_OBJECT_MOVED,
  KS_SCRIPT_OBJECT_DESTROYED
} Ks_Object_State;

typedef enum {
  KS_SCRIPT_OWNER_USER, // TODO: dont like the naming convention. Needs to be
                        // changed
  KS_SCRIPT_OWNER_SCRIPT,
  KS_SCRIPT_OWNER_SHARED
} Ks_Script_Ownership;

typedef struct {
  Ks_Script_Ownership ownership_type;
  ks_size ref_count;
  ks_deallocator deallocator;
} Ks_Script_Ownership_Info;

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
    struct {
      ks_str data;
      ks_size len;
      ks_size capacity;
      Ks_Script_Ownership_Info owner_info;
    } string;
    ks_double number;
    ks_bool boolean;
    Ks_Script_Ref table;
    Ks_Script_Ref function;
    Ks_Script_Ref coroutine;
    struct {
      ks_ptr data;
      ks_size size;
      Ks_Script_Ownership_Info owner_info;
    } userdata;
    ks_ptr lightuserdata;
  } val;
  Ks_Script_Object_Type type;
  Ks_Object_State state;
} Ks_Script_Object;

typedef Ks_Script_Object Ks_Script_Table;
typedef Ks_Script_Object Ks_Script_Function;
typedef Ks_Script_Object Ks_Script_Coroutine;
typedef Ks_Script_Object Ks_Script_Userdata;
typedef Ks_Script_Object Ks_Script_LightUserdata;
typedef Ks_Script_Object Ks_Script_Function_Call_Result;

typedef struct {
  ks_str name;
  ks_size overload_count;
  ks_cfunc *overloads;
  ks_size *args_count;
  ks_str **arg_types;
} Ks_Script_Usertype_Method;

typedef struct {
  ks_str name;
  ks_cfunc getter;
  ks_cfunc setter;
  ks_str type_name;
} Ks_Script_Usertype_Property;

typedef enum {
  KS_SCRIPT_USERTYPE_METAMETHOD_INDEX,
  KS_SCRIPT_USERTYPE_METAMETHOD_NEWINDEX,
  KS_SCRIPT_USERTYPE_METAMETHOD_CALL,
  KS_SCRIPT_USERTYPE_METAMETHOD_ADD,
  KS_SCRIPT_USERTYPE_METAMETHOD_SUB,
  KS_SCRIPT_USERTYPE_METAMETHOD_MUL,
  KS_SCRIPT_USERTYPE_METAMETHOD_DIV,
  KS_SCRIPT_USERTYPE_METAMETHOD_EQ,
  KS_SCRIPT_USERTYPE_METAMETHOD_LT,
  KS_SCRIPT_USERTYPE_METAMETHOD_LE,
  KS_SCRIPT_USERTYPE_METAMETHOD_TOSTRING,
  KS_SCRIPT_USERYUPE_METAMETHOD_GC
} Ks_Script_Usertype_Metamethods;

typedef struct {
  ks_str type_name;
  Ks_Script_Ref metatable;
  ks_size methods_count;
  Ks_Script_Usertype_Method *methods;
  ks_size properties_count;
  Ks_Script_Usertype_Property *properties;
  ks_cfunc constructor;
  ks_cfunc destructor;
} Ks_Script_Usertype_Info;

typedef struct {
  Ks_Script_Ref table_ref;
  ks_int iter_state;
  ks_bool valid;
} Ks_Script_Table_Iterator;

typedef enum {
  KS_SCRIPT_ERROR_NONE,
  KS_SCRIPT_ERROR_MEMORY,
  KS_SCRIPT_ERROR_RUNTIME,
  KS_SCRIPT_ERROR_STACK_OVERFLOW,
  KS_SCRIPT_ERROR_INVALID_OPERATION,
  KS_SCRIPT_ERROR_INVALID_ARGUMENT,
  KS_SCRIPT_ERROR_INVALID_OBJECT,
  KS_SCRIPT_ERROR_SYMBOL_NOT_FOUND,
  KS_SCRIPT_ERROR_INVALID_OWNERSHIP,
  KS_SCRIPT_ERROR_DOUBLE_FREE,
  KS_SCRIPT_ERROR_OWNERSHIP_VIOLATON
} Ks_Script_Error;

typedef struct {
  Ks_Script_Error error;
  ks_str message;
  ks_str file;
  ks_size line;
} Ks_Script_Error_Info;

typedef enum {
  KS_SCRIPT_COROUTINE_NORMAL,
  KS_SCRIPT_COROUTINE_SUSPENDED,
  KS_SCRIPT_COROUTINE_RUNNING,
  KS_SCRIPT_COROUTINE_DEAD
} Ks_Script_Coroutine_Status;

Ks_Script_Ctx ks_script_create_ctx();
ks_no_ret ks_script_destroy_ctx(Ks_Script_Ctx ctx);

Ks_Script_Object ks_script_create_number(Ks_Script_Ctx ctx, ks_double val);
Ks_Script_Object ks_script_create_boolean(Ks_Script_Ctx ctx, ks_bool val);
Ks_Script_Object ks_script_create_nil(Ks_Script_Ctx ctx);
Ks_Script_Object ks_script_create_invalid_obj(Ks_Script_Ctx ctx);

Ks_Script_Function ks_script_create_cfunc(Ks_Script_Ctx ctx, ks_cfunc f);
Ks_Script_Function ks_script_create_cfunc_with_upvalues(Ks_Script_Ctx ctx,
                                                        ks_cfunc f,
                                                        ks_size n_upvalues);

Ks_Script_Table ks_script_create_table(Ks_Script_Ctx ctx);
Ks_Script_Table ks_script_create_table_with_capacity(Ks_Script_Ctx ctx,
                                                     ks_size array_sz,
                                                     ks_size hash_sz);
Ks_Script_Table ks_script_create_named_table(Ks_Script_Ctx ctx, ks_str name);

Ks_Script_Object ks_script_create_cstring(Ks_Script_Ctx ctx, ks_str val);
Ks_Script_Object ks_script_create_lstring(Ks_Script_Ctx ctx, ks_str str,
                                          ks_size len);
Ks_Script_Object ks_script_create_vstring(Ks_Script_Ctx ctx, ks_str str,
                                          ks_va_list vargs);
Ks_Script_Object ks_script_create_fstring(Ks_Script_Ctx ctx, ks_str fmt, ...);

ks_no_ret ks_script_destroy_obj(Ks_Script_Ctx ctx, Ks_Script_Object object);

Ks_Script_Error ks_script_get_last_error(Ks_Script_Ctx ctx);
ks_str ks_script_get_last_error_str(Ks_Script_Ctx ctx);
Ks_Script_Error_Info ks_script_get_last_error_info(Ks_Script_Ctx ctx);
ks_no_ret ks_script_clear_error(Ks_Script_Ctx ctx);

ks_no_ret ks_script_set_global(Ks_Script_Ctx ctx, ks_str name,
                               Ks_Script_Object val);
Ks_Script_Object ks_script_get_global(Ks_Script_Ctx ctx, ks_str name);

Ks_Script_Function ks_script_load_string(Ks_Script_Ctx ctx, ks_str string);
Ks_Script_Function ks_script_load_file(Ks_Script_Ctx ctx, ks_str file_path);

Ks_Script_Function_Call_Result ks_script_do_string(Ks_Script_Ctx ctx,
                                                   ks_str string);
Ks_Script_Function_Call_Result ks_script_do_file(Ks_Script_Ctx ctx,
                                                 ks_str file_path);

Ks_Script_Object ks_script_require(Ks_Script_Ctx ctx, ks_str module_name);
ks_no_ret ks_script_register_module(Ks_Script_Ctx ctx, ks_str name,
                                    Ks_Script_Table module);
ks_no_ret ks_script_add_package_path(Ks_Script_Ctx ctx, ks_str path);

ks_no_ret ks_script_gc_collect(Ks_Script_Ctx ctx);
ks_no_ret ks_script_gc_stop(Ks_Script_Ctx ctx);
ks_no_ret ks_script_gc_restart(Ks_Script_Ctx ctx);
ks_no_ret ks_script_get_mem_used(Ks_Script_Ctx ctx);

ks_no_ret ks_script_dump_registry(Ks_Script_Ctx ctx);

ks_no_ret ks_script_stack_push_number(Ks_Script_Ctx ctx, ks_double val);
ks_no_ret ks_script_stack_push_boolean(Ks_Script_Ctx ctx, ks_bool val);
ks_no_ret ks_script_stack_push_string(Ks_Script_Ctx ctx, ks_str val);
ks_no_ret ks_script_stack_push_obj(Ks_Script_Ctx ctx, Ks_Script_Object val);

ks_double ks_script_stack_pop_number(Ks_Script_Ctx ctx);
ks_bool ks_script_stack_pop_boolean(Ks_Script_Ctx ctx);
ks_str ks_script_stack_pop_string(Ks_Script_Ctx ctx);
Ks_Script_Object ks_script_stack_pop_obj(Ks_Script_Ctx ctx);
Ks_Script_Object ks_script_stack_get_top(Ks_Script_Ctx ctx);
Ks_Script_Object ks_script_stack_get(Ks_Script_Ctx ctx, ks_stack_idx i);

ks_size ks_script_stack_size(Ks_Script_Ctx ctx);
ks_no_ret ks_script_stack_clear(Ks_Script_Ctx ctx);
ks_no_ret ks_script_stack_remove(Ks_Script_Ctx ctx, ks_stack_idx i);
ks_no_ret ks_script_stack_insert(Ks_Script_Ctx ctx, ks_stack_idx i);
ks_no_ret ks_script_stack_replace(Ks_Script_Ctx ctx, ks_stack_idx i);
ks_no_ret ks_script_stack_copy(Ks_Script_Ctx ctx, ks_stack_idx i);

ks_no_ret ks_script_stack_dump(Ks_Script_Ctx ctx);

Ks_Script_Object_Type ks_script_obj_type(Ks_Script_Object obj);
ks_bool ks_script_obj_is(Ks_Script_Object obj);
ks_double ks_script_obj_as_number(Ks_Script_Object obj);
ks_bool ks_script_obj_as_boolean(Ks_Script_Object obj);
ks_str ks_script_obj_as_str(Ks_Script_Object obj);
Ks_Script_Table ks_script_obj_as_table(Ks_Script_Object obj);
Ks_Script_Function ks_script_obj_as_function(Ks_Script_Object obj);
Ks_Script_Coroutine ks_script_obj_as_coroutine(Ks_Script_Object obj);

ks_double ks_script_obj_as_number_or(Ks_Script_Object obj, ks_double def);
ks_bool ks_script_obj_as_boolean_or(Ks_Script_Object obj, ks_bool def);
ks_str ks_script_obj_as_str_or(Ks_Script_Object obj, ks_str def);
Ks_Script_Table ks_script_obj_as_table_or(Ks_Script_Object obj,
                                          Ks_Script_Table def);
Ks_Script_Function ks_script_as_function_or(Ks_Script_Object obj,
                                            Ks_Script_Function def);
Ks_Script_Coroutine ks_script_obj_as_coroutine_or(Ks_Script_Object obj,
                                                  Ks_Script_Coroutine def);

ks_bool ks_script_obj_try_as_number(Ks_Script_Object obj, ks_double *out);
ks_bool ks_script_obj_try_as_boolean(Ks_Script_Object obj, ks_bool *out);
ks_bool ks_script_obj_try_as_string(Ks_Script_Object obj, ks_str *out);
ks_bool ks_script_obj_try_as_table(Ks_Script_Object obj, Ks_Script_Table *out);
ks_bool ks_script_obj_try_as_function(Ks_Script_Object obj,
                                      Ks_Script_Function *out);
ks_bool ks_script_obj_try_as_coroutine(Ks_Script_Object obj,
                                       Ks_Script_Coroutine *out);

ks_bool ks_script_obj_has_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj);
Ks_Script_Table ks_script_obj_get_metatable(Ks_Script_Ctx ctx,
                                            Ks_Script_Object obj);
ks_no_ret ks_script_obj_set_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj,
                                      Ks_Script_Table mt);

ks_bool ks_script_obj_is_callable(Ks_Script_Object obj);
ks_bool ks_script_obj_is_iterable(Ks_Script_Object obj);

ks_no_ret ks_script_obj_dump(Ks_Script_Object obj);
ks_str ks_script_obj_to_string(Ks_Script_Ctx ctx, Ks_Script_Object obj);

ks_bool ks_script_table_has(Ks_Script_Ctx ctx, Ks_Script_Table tbl,
                            Ks_Script_Object key);
ks_no_ret ks_script_table_set(Ks_Script_Ctx, Ks_Script_Table tbl,
                              Ks_Script_Object key, Ks_Script_Object value);
Ks_Script_Object ks_script_table_get(Ks_Script_Ctx ctx, Ks_Script_Table tbl,
                                     Ks_Script_Object key);
ks_size ks_script_table_array_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl);
ks_size ks_script_table_total_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl);
Ks_Script_Table_Iterator ks_script_table_iterate(Ks_Script_Ctx ctx,
                                                 Ks_Script_Table tbl);

ks_bool ks_script_iterator_has_next(Ks_Script_Ctx ctx,
                                    Ks_Script_Table_Iterator iterator);
ks_no_ret ks_script_iterator_next(Ks_Script_Ctx ctx,
                                  Ks_Script_Table_Iterator *iterator,
                                  Ks_Script_Object *key,
                                  Ks_Script_Object *value);
ks_no_ret ks_script_iterator_destroy(Ks_Script_Ctx ctx,
                                     Ks_Script_Table_Iterator *iterator);
ks_no_ret ks_script_iterator_reset(Ks_Script_Ctx ctx,
                                   Ks_Script_Table_Iterator *iterator);
Ks_Script_Table_Iterator
ks_script_iterator_clone(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator *iterator);

ks_no_ret ks_script_func_call(Ks_Script_Ctx ctx, Ks_Script_Function f,
                              ks_size n_args, ks_size n_rets);

#define ks_script_func_callv(ctx, func, ...)                                   \
  ks_script_func_callv_impl(ctx, func, __VA_ARGS__,                            \
                            ks_script_create_invalid_objet(ctx));
Ks_Script_Function_Call_Result
ks_script_func_callv_impl(Ks_Script_Ctx ctx, Ks_Script_Function f, ...);

Ks_Script_Object ks_script_func_get_upvalue(Ks_Script_Ctx ctx,
                                            ks_upvalue_idx i);

ks_bool ks_script_call_succeded(Ks_Script_Function_Call_Result res);
Ks_Script_Object ks_script_call_get_return(Ks_Script_Function_Call_Result res);
ks_size ks_script_call_get_returns_count(Ks_Script_Function_Call_Result res);
Ks_Script_Object
ks_script_call_get_return_at(Ks_Script_Function_Call_Result res, ks_size idx);

#ifdef __cplusplus
}
#endif
