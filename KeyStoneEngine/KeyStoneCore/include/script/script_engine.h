/**
 * @file script_engine.h
 * @brief Public C API for integrating lua scripting.
 *
 * This is the main interface used to interact with the lua VM.
 *
 * @defgroup Scripting Scripting Engine
 * @brief C interface used for managing the lua vm, bindings and coroutines.
 * @{
 */
#pragma once

#include "../core/types.h"

#ifdef __cplusplus
#include <initializer_list>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque pointer to the scripting context. */
typedef ks_ptr Ks_Script_Ctx;

/** @brief Opaque pointer to a Usertype Builder helper. */
typedef ks_ptr Ks_Script_Userytype_Builder;

/** @brief Integer reference to a script object stored in the registry. */
typedef ks_int Ks_Script_Ref;

/** @brief Number of return values from a C function call. */
typedef ks_int ks_returns_count;

/** @brief Index into the script engine stack (1-based). */
typedef ks_size ks_stack_idx;

/** @brief Index for accessing upvalues in C closures (1-based). */
typedef ks_size ks_upvalue_idx;

/** @brief Function pointer type for C functions callable from scripts. */
typedef ks_returns_count (*ks_script_cfunc)(Ks_Script_Ctx ctx);

/** @brief Callback for deallocating userdata memory. */
typedef ks_no_ret (*ks_script_deallocator)(ks_ptr data, ks_size size);

#define KS_SCRIPT_INVALID_REF (-1)
#define KS_SCRIPT_NO_REF (-2)

/**
 * @brief State of a script object handle.
 */
typedef enum {
  KS_SCRIPT_OBJECT_INVALID,   ///< The object is invalid or null.
  KS_SCRIPT_OBJECT_VALID,     ///< The object is valid and holds a reference.
  KS_SCRIPT_OBJECT_MOVED,     ///< The object ownership has been moved.
  KS_SCRIPT_OBJECT_DESTROYED  ///< The object has been destroyed.
} Ks_Script_Object_State;

/**
 * @brief Supported object types in the scripting system.
 */
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

/**
 * @brief Generic wrapper for script objects passed between C++ and the script engine.
 */
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

// Typedef aliases for semantic clarity
typedef Ks_Script_Object Ks_Script_Table;
typedef Ks_Script_Object Ks_Script_Function;
typedef Ks_Script_Object Ks_Script_Coroutine;
typedef Ks_Script_Object Ks_Script_Userdata;
typedef Ks_Script_Object Ks_Script_LightUserdata;
typedef Ks_Script_Object Ks_Script_Function_Call_Result;

/**
 * @brief Metamethods supported by Usertypes.
 */
typedef enum {
  KS_SCRIPT_MT_INDEX,     ///< __index
  KS_SCRIPT_MT_NEWINDEX,  ///< __newindex
  KS_SCRIPT_MT_CALL,      ///< __call
  KS_SCRIPT_MT_ADD,       ///< __add (+)
  KS_SCRIPT_MT_SUB,       ///< __sub (-)
  KS_SCRIPT_MT_MUL,       ///< __mul (*)
  KS_SCRIPT_MT_IDIV,      ///< __idiv (//)
  KS_SCRIPT_MT_DIV,       ///< __div (/)
  KS_SCRIPT_MT_MOD,       ///< __mod (%)
  KS_SCRIPT_MT_POW,       ///< __pow (^)
  KS_SCRIPT_MT_UNM,       ///< __unm (unary -)
  KS_SCRIPT_MT_BNOT,      ///< __bnot (~)
  KS_SCRIPT_MT_BAND,      ///< __band (&)
  KS_SCRIPT_MT_BOR,       ///< __bor (|)
  KS_SCRIPT_MT_BXOR,      ///< __bxor (~)
  KS_SCRIPT_MT_SHL,       ///< __shl (<<)
  KS_SCRIPT_MT_SHR,       ///< __shr (>>)
  KS_SCRIPT_MT_EQ,        ///< __eq (==)
  KS_SCRIPT_MT_LT,        ///< __lt (<)
  KS_SCRIPT_MT_LE,        ///< __le (<=)
  KS_SCRIPT_MT_CONCAT,    ///< __concat (..)
  KS_SCRIPT_MT_LEN,       ///< __len (#)
  KS_SCRIPT_MT_TOSTRING,  ///< __tostring
  KS_SCRIPT_MT_GC,        ///< __gc (garbage collection)
  KS_SCRIPT_MT_CLOSE      ///< __close
} Ks_Script_Metamethod;

/**
 * @brief Iterator state for traversing tables.
 */
typedef struct {
  Ks_Script_Ref table_ref;
  Ks_Script_Ref current_key_ref;
  ks_bool iter_started;
  ks_bool valid;
} Ks_Script_Table_Iterator;

typedef struct {
    ks_script_cfunc func;
    const Ks_Type* args;
    ks_size num_args;
} Ks_Script_Sig_Def;

#ifdef __cplusplus
#define KS_SCRIPT_SIG_DEF(f, ...) \
    Ks_Script_Sig_Def{ \
        f, \
        std::initializer_list<Ks_Type>{__VA_ARGS__}.begin(), \
        std::initializer_list<Ks_Type>{__VA_ARGS__}.size() \
    }

#define KS_SCRIPT_SIG_DEF_VOID(f) \
    Ks_Script_Sig_Def{ f, nullptr, 0 }

#define KS_SCRIPT_FUNC(f, ...) \
    std::initializer_list<Ks_Script_Sig_Def>{ KS_SCRIPT_SIG_DEF(f, __VA_ARGS__) }.begin(), 1

#define KS_SCRIPT_FUNC_VOID(f) \
    std::initializer_list<Ks_Script_Sig_Def>{ KS_SCRIPT_SIG_DEF_VOID(f) }.begin(), 1

#define KS_SCRIPT_OVERLOAD(...) \
    std::initializer_list<Ks_Script_Sig_Def>{__VA_ARGS__}.begin(), \
    std::initializer_list<Ks_Script_Sig_Def>{__VA_ARGS__}.size()

#else
#define KS_SCRIPT_SIG_DEF(f, ...) \
    (Ks_Script_Sig_Def){ .func = f, .args = (const Ks_Type[]){__VA_ARGS__}, .num_args = ... } 

#define KS_SCRIPT_SIG_DEF_VOID(f) \
    (Ks_Script_Sig_Def){ .func = f, .args = nullptr, .num_args = 0 }

#define KS_SCRIPT_FUNC(...) \
    (const Ks_Script_Sig_Def[]){ KS_SCRIPT_SIG_DEF(__VA_ARGS__) }, 1

#define KS_SCRIPT_FUNC_VOID(func) \
    (const Ks_Script_Sig_Def[]){ KS_SCRIPT_DEF_NO_ARGS(func) }, 1

#define KS_SCRIPT_OVERLOAD(...) \
    (const Ks_Script_Sig_Def[]){__VA_ARGS__}, \
    (sizeof((const Ks_Script_Sig_Def[]){__VA_ARGS__}) / sizeof(const Ks_Script_Sig_Def))

#endif

/**
 * @brief Script engine error codes.
 */
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

/**
 * @brief Detailed error information.
 */
typedef struct {
  Ks_Script_Error error;
  ks_str message;
} Ks_Script_Error_Info;

/**
 * @brief Status of a coroutine.
 */
typedef enum {
  KS_SCRIPT_COROUTINE_NORMAL,
  KS_SCRIPT_COROUTINE_SUSPENDED,
  KS_SCRIPT_COROUTINE_RUNNING,
  KS_SCRIPT_COROUTINE_DEAD,
  KS_SCRIPT_COROUTINE_ERROR
} Ks_Script_Coroutine_Status;

/**
 * @brief Creates a new script context (VM).
 * @return Handle to the created context.
 */
KS_API Ks_Script_Ctx ks_script_create_ctx();

/**
 * @brief Destroys a script context and frees all associated resources.
 * @param ctx The context to destroy.
 */
KS_API ks_no_ret ks_script_destroy_ctx(Ks_Script_Ctx ctx);

/**
 * @brief Begins a new object tracking scope.
 * Objects created within this scope will be automatically released when the scope ends.
 * @param ctx The script context.
 */
KS_API ks_no_ret ks_script_begin_scope(Ks_Script_Ctx ctx);

/**
 * @brief Ends the current object scope.
 * @param ctx The script context.
 */
KS_API ks_no_ret ks_script_end_scope(Ks_Script_Ctx ctx);

/**
 * @brief Promotes an object to the parent scope, preventing it from being freed when the current scope ends.
 * @param ctx The script context.
 * @param obj The object to promote.
 */
KS_API ks_no_ret ks_script_promote(Ks_Script_Ctx ctx, Ks_Script_Object obj);

/** @brief Creates a number object. */
KS_API Ks_Script_Object ks_script_create_number(Ks_Script_Ctx ctx, ks_double val);
/** @brief Creates a boolean object. */
KS_API Ks_Script_Object ks_script_create_boolean(Ks_Script_Ctx ctx, ks_bool val);
/** @brief Creates a nil object. */
KS_API Ks_Script_Object ks_script_create_nil(Ks_Script_Ctx ctx);
/** @brief Creates an invalid object placeholder. */
KS_API Ks_Script_Object ks_script_create_invalid_obj(Ks_Script_Ctx ctx);

/**
 * @brief Wraps a C function as a script function.
 * @param ctx The script context.
 * @param f The C function pointer.
 * @return A script function object.
 */
 //KS_API Ks_Script_Function ks_script_create_cfunc(Ks_Script_Ctx ctx, ks_script_cfunc f);
KS_API Ks_Script_Function ks_script_create_cfunc(Ks_Script_Ctx ctx, const Ks_Script_Sig_Def* sigs, ks_size count);
/**
 * @brief Creates a C function with associated upvalues (closure).
 * * The upvalues must be pushed onto the stack before calling this function.
 * @param ctx The script context.
 * @param f The C function.
 * @param n_upvalues Number of upvalues to associate.
 * @return A script function object.
 */       
KS_API Ks_Script_Function ks_script_create_cfunc_with_upvalues(Ks_Script_Ctx ctx, const Ks_Script_Sig_Def* sigs, ks_size count, ks_size n_upvalues);

/** @brief Creates a new empty table. */
KS_API Ks_Script_Table ks_script_create_table(Ks_Script_Ctx ctx);
/** @brief Creates a new table with pre-allocated capacity. */
KS_API Ks_Script_Table ks_script_create_table_with_capacity(Ks_Script_Ctx ctx,
                                                     ks_size array_sz,
                                                     ks_size hash_sz);
/** @brief Creates a named global table. */
KS_API Ks_Script_Table ks_script_create_named_table(Ks_Script_Ctx ctx, ks_str name);

/** @brief Creates a string object. */
KS_API Ks_Script_Object ks_script_create_cstring(Ks_Script_Ctx ctx, ks_str val);
/** @brief Creates a string object with explicit length. */
KS_API Ks_Script_Object ks_script_create_lstring(Ks_Script_Ctx ctx, ks_str str,
                                          ks_size len);
/** @brief Creates a full userdata object of specific size. */
KS_API Ks_Script_Userdata ks_script_create_userdata(Ks_Script_Ctx ctx, ks_size size);

/** @brief Creates an instance of a registered Usertype. */
KS_API Ks_Script_Userdata ks_script_create_usertype_instance(Ks_Script_Ctx ctx, ks_str type_name);

/** @brief Creates a reference to an existing C++ pointer as a userdata (non-owning). */
KS_API Ks_Script_Object ks_script_create_usertype_ref(Ks_Script_Ctx ctx, ks_str type_name, void* ptr);

/**
 * @brief Frees a script object, releasing its registry reference.
 * @param ctx The script context.
 * @param obj The object to free.
 */
KS_API ks_no_ret ks_script_free_obj(Ks_Script_Ctx ctx, Ks_Script_Object obj);

/** @brief Gets the last error code. */
KS_API Ks_Script_Error ks_script_get_last_error(Ks_Script_Ctx ctx);
/** @brief Gets the last error message string. */
KS_API ks_str ks_script_get_last_error_str(Ks_Script_Ctx ctx);
/** @brief Gets detailed error information. */
KS_API Ks_Script_Error_Info ks_script_get_last_error_info(Ks_Script_Ctx ctx);
/** @brief Clears the last error state. */
KS_API ks_no_ret ks_script_clear_error(Ks_Script_Ctx ctx);

/** @brief Sets a global variable. */
KS_API ks_no_ret ks_script_set_global(Ks_Script_Ctx ctx, ks_str name,
                               Ks_Script_Object val);
/** @brief Gets a global variable. */
KS_API Ks_Script_Object ks_script_get_global(Ks_Script_Ctx ctx, ks_str name);

/** @brief Loads a script from a string (compiles it). */
KS_API Ks_Script_Function ks_script_load_string(Ks_Script_Ctx ctx, ks_str string);
/** @brief Loads a script from a file (compiles it). */
KS_API Ks_Script_Function ks_script_load_file(Ks_Script_Ctx ctx, ks_str file_path);

/** @brief Executes a script string and returns the result(s). */
KS_API Ks_Script_Function_Call_Result ks_script_do_string(Ks_Script_Ctx ctx,
                                                   ks_str string);
/** @brief Executes a script file and returns the result(s). */
KS_API Ks_Script_Function_Call_Result ks_script_do_file(Ks_Script_Ctx ctx,
                                                 ks_str file_path);

/** @brief Requires a module (Lua require). */
KS_API Ks_Script_Object ks_script_require(Ks_Script_Ctx ctx, ks_str module_name);
/** @brief Registers a module table in package.loaded. */
KS_API ks_no_ret ks_script_register_module(Ks_Script_Ctx ctx, ks_str name,
                                    Ks_Script_Table module);
/** @brief Adds a path to package.path. */
KS_API ks_no_ret ks_script_add_package_path(Ks_Script_Ctx ctx, ks_str path);

/** @brief Forces garbage collection. */
KS_API ks_no_ret ks_script_gc_collect(Ks_Script_Ctx ctx);
/** @brief Stops the garbage collector. */
KS_API ks_no_ret ks_script_gc_stop(Ks_Script_Ctx ctx);
/** @brief Restarts the garbage collector. */
KS_API ks_no_ret ks_script_gc_restart(Ks_Script_Ctx ctx);
/** @brief Gets current memory usage in Kilobytes. */
KS_API ks_size ks_script_get_mem_used(Ks_Script_Ctx ctx);

/** @brief Dumps the entire registry to log (Debug). */
KS_API ks_no_ret ks_script_dump_registry(Ks_Script_Ctx ctx);

/* --- Stack Manipulation (1-based indexing) --- */
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

/* --- Type Checking & Conversions --- */
KS_API Ks_Script_Object_Type ks_script_obj_type(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_bool ks_script_obj_is_valid(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_bool ks_script_obj_is(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Object_Type type);
KS_API ks_double ks_script_obj_as_number(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_bool ks_script_obj_as_boolean(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_str ks_script_obj_as_str(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API Ks_Script_Table ks_script_obj_as_table(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API Ks_Script_Function ks_script_obj_as_function(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine(Ks_Script_Ctx ctx, Ks_Script_Object obj);

// Safe conversions with default values
KS_API ks_double ks_script_obj_as_number_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_double def);
KS_API ks_bool ks_script_obj_as_boolean_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_bool def);
KS_API ks_str ks_script_obj_as_str_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str def);
KS_API Ks_Script_Table ks_script_obj_as_table_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table def);
KS_API Ks_Script_Function ks_script_as_function_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Function def);
KS_API Ks_Script_Coroutine ks_script_obj_as_coroutine_or(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Coroutine def);

// Try-Get pattern
KS_API ks_bool ks_script_obj_try_as_number(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_double *out);
KS_API ks_bool ks_script_obj_try_as_boolean(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_bool *out);
KS_API ks_bool ks_script_obj_try_as_string(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str *out);
KS_API ks_bool ks_script_obj_try_as_table(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table *out);
KS_API ks_bool ks_script_obj_try_as_function(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Function *out);
KS_API ks_bool ks_script_obj_try_as_coroutine(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Coroutine *out);

/* --- Metatables --- */
KS_API ks_bool ks_script_obj_has_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API Ks_Script_Table ks_script_obj_get_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_no_ret ks_script_obj_set_metatable(Ks_Script_Ctx ctx, Ks_Script_Object obj, Ks_Script_Table mt);

KS_API ks_bool ks_script_obj_is_callable(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_bool ks_script_obj_is_iterable(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_no_ret ks_script_obj_dump(Ks_Script_Ctx ctx, Ks_Script_Object obj);
KS_API ks_str ks_script_obj_to_string(Ks_Script_Ctx ctx, Ks_Script_Object obj);

/* --- Table Manipulation --- */
KS_API ks_bool ks_script_table_has(Ks_Script_Ctx ctx, Ks_Script_Table tbl, Ks_Script_Object key);
KS_API ks_no_ret ks_script_table_set(Ks_Script_Ctx ctx, Ks_Script_Table tbl, Ks_Script_Object key, Ks_Script_Object value);
KS_API Ks_Script_Object ks_script_table_get(Ks_Script_Ctx ctx, Ks_Script_Table tbl, Ks_Script_Object key);
KS_API ks_size ks_script_table_array_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl);
KS_API ks_size ks_script_table_total_size(Ks_Script_Ctx ctx, Ks_Script_Table tbl);
KS_API Ks_Script_Table_Iterator ks_script_table_iterate(Ks_Script_Ctx ctx, Ks_Script_Table tbl);

KS_API ks_bool ks_script_iterator_has_next(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator* iterator);
KS_API ks_bool ks_script_iterator_next(Ks_Script_Ctx ctx,
                                  Ks_Script_Table_Iterator *iterator,
                                  Ks_Script_Object *key,
                                  Ks_Script_Object *value);
KS_API ks_no_ret ks_script_iterator_destroy(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator *iterator);
KS_API ks_no_ret ks_script_iterator_reset(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator *iterator);
KS_API Ks_Script_Table_Iterator ks_script_iterator_clone(Ks_Script_Ctx ctx, Ks_Script_Table_Iterator *iterator);

/**
 * @brief Gets the Nth argument passed to the C function.
 * @param n Index (1-based).
 */
KS_API Ks_Script_Object ks_script_get_arg(Ks_Script_Ctx ctx, ks_stack_idx n);
/**
 * @brief Gets the Nth upvalue associated with the current function.
 * @param n Index (1-based).
 */
KS_API Ks_Script_Object ks_script_get_upvalue(Ks_Script_Ctx ctx, ks_upvalue_idx n);

/**
 * @brief Calls a script function.
 * @param f Function to call.
 * @param n_args Number of arguments on stack.
 * @param n_rets Number of returns expected.
 */
KS_API ks_no_ret ks_script_func_call(Ks_Script_Ctx ctx, Ks_Script_Function f,
                              ks_size n_args, ks_size n_rets);

/**
 * @brief Variadic macro to call a script function with arguments.
 * * Automatically pushes arguments to the stack.
 */
#define ks_script_func_callv(ctx, func, ...)                                   \
  ks_script_func_callv_impl(ctx, func __VA_OPT__(,) __VA_ARGS__,                            \
                            ks_script_create_invalid_obj(ctx));
KS_API Ks_Script_Function_Call_Result
ks_script_func_callv_impl(Ks_Script_Ctx ctx, Ks_Script_Function f, ...);

KS_API Ks_Script_Object ks_script_func_get_upvalue(Ks_Script_Ctx ctx,
                                            ks_upvalue_idx i);

KS_API ks_bool ks_script_call_succeded(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res);
KS_API Ks_Script_Object ks_script_call_get_return(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res);
KS_API ks_size ks_script_call_get_returns_count(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res);
/**
 * @brief Gets the Nth return value from a function call result.
 * @param idx Index (1-based).
 */
KS_API Ks_Script_Object
ks_script_call_get_return_at(Ks_Script_Ctx ctx, Ks_Script_Function_Call_Result res, ks_size idx);

/* --- Coroutines --- */
KS_API Ks_Script_Coroutine ks_script_create_coroutine(Ks_Script_Ctx ctx, Ks_Script_Function func);
KS_API Ks_Script_Coroutine_Status ks_script_coroutine_status(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine);

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_resume(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine, ks_size n_args);

#define ks_script_coroutine_resumev(ctx, coroutine, ...) \
    ks_script_coroutine_resumev_impl(ctx, coroutine __VA_OPT__(,) __VA_ARGS__, ks_script_create_invalid_obj(ctx))

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_resumev_impl(Ks_Script_Ctx ctx, Ks_Script_Coroutine coroutine, ...);

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_yield(Ks_Script_Ctx ctx, ks_size n_results);

#define ks_script_coroutine_yieldv(ctx, ...) \
    ks_script_coroutine_yieldv_impl(ctx, __VA_ARGS__, ks_script_create_invalid_obj(ctx))

KS_API Ks_Script_Function_Call_Result ks_script_coroutine_yieldv_impl(Ks_Script_Ctx ctx, ...);

KS_API Ks_Script_LightUserdata ks_script_create_lightuserdata(Ks_Script_Ctx ctx, ks_ptr ptr);
KS_API ks_ptr ks_script_lightuserdata_get_ptr(Ks_Script_Ctx ctx, Ks_Script_LightUserdata lud);

KS_API ks_ptr ks_script_userdata_get_ptr(Ks_Script_Ctx ctx, Ks_Script_Userdata ud);
KS_API ks_size ks_script_userdata_get_size(Ks_Script_Ctx ctx, Ks_Script_Userdata ud);

KS_API ks_no_ret ks_script_set_type_name(Ks_Script_Ctx ctx, Ks_Script_Object obj, ks_str type_name);
KS_API ks_ptr ks_script_get_self(Ks_Script_Ctx ctx);

KS_API ks_ptr ks_script_usertype_get_ptr(Ks_Script_Ctx ctx, Ks_Script_Object obj);

/* --- Usertype Builder --- */
/**
 * @brief Begins definition of a new user type (class).
 */
KS_API Ks_Script_Userytype_Builder ks_script_usertype_begin(Ks_Script_Ctx ctx, ks_str type_name, ks_size instance_size);

KS_API ks_no_ret ks_script_usertype_inherits_from(Ks_Script_Userytype_Builder builder, ks_str base_type_name);

KS_API ks_no_ret ks_script_usertype_add_constructor(Ks_Script_Userytype_Builder builder, const Ks_Script_Sig_Def* sigs, ks_size count);
KS_API ks_no_ret ks_script_usertype_set_destructor(Ks_Script_Userytype_Builder builder, ks_script_deallocator dtor);

KS_API ks_no_ret ks_script_usertype_add_method(Ks_Script_Userytype_Builder builder, ks_str name, const Ks_Script_Sig_Def* sigs, ks_size count);
KS_API ks_no_ret ks_script_usertype_add_static_method(Ks_Script_Userytype_Builder builder, ks_str name, const Ks_Script_Sig_Def* sigs, ks_size count);
KS_API ks_no_ret ks_script_usertype_add_field(Ks_Script_Userytype_Builder builder, ks_str name, Ks_Type type, ks_size offset, ks_str type_alias);
KS_API ks_no_ret ks_script_usertype_add_property(Ks_Script_Userytype_Builder builder, ks_str name, ks_script_cfunc getter, ks_script_cfunc setter);

KS_API ks_no_ret ks_script_usertype_add_metamethod(Ks_Script_Userytype_Builder builder, Ks_Script_Metamethod mt, ks_script_cfunc func);

/**
 * @brief Finalizes and registers the user type.
 * @param builder The builder handle (invalidated after call).
 */
KS_API ks_no_ret ks_script_usertype_end(Ks_Script_Userytype_Builder builder);

#ifdef __cplusplus
}
#endif
/** @} */