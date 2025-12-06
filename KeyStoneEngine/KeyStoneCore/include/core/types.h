/**
 * @file types.h
 * @brief Primitive types definitions
 * @ingroup Core
 */
#pragma once

#include "defines.h"

#include <cstdarg>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Alias for C-style strings (const char*). */
typedef const char *ks_str;

typedef void ks_void;
typedef void ks_no_ret; ///< Return type for functions that do not return a value.
typedef void *ks_ptr;   ///< Generic pointer type (void*).

typedef uint8_t ks_bool; ///< Boolean type (0 = false, non-zero = true).
typedef uint8_t ks_byte; ///< Byte type (unsigned 8-bit).
typedef char ks_char;
typedef short ks_short;
typedef int ks_int;
typedef long ks_long;
typedef long long ks_long_long;
typedef size_t ks_size;  ///< Size type (platform dependent, usually size_t).
typedef float ks_float;
typedef double ks_double;

// Fixed width unsigned integers
typedef uint8_t ks_uint8;
typedef uint16_t ks_uint16;
typedef uint32_t ks_uint32;
typedef uint64_t ks_uint64;

// Fixed width signed integers
typedef int8_t ks_int8;
typedef int16_t ks_int16;
typedef int32_t ks_int32;
typedef int64_t ks_int64;

// Floating point types
typedef float ks_float32;
typedef double ks_float64;

/** @brief Platform-independent definition for variable argument lists. */
typedef va_list ks_va_list;

typedef enum Ks_Type {
	KS_TYPE_UNKNOWN,
	KS_TYPE_BOOL,
	KS_TYPE_CHAR,
	KS_TYPE_INT,
	KS_TYPE_UINT,
	KS_TYPE_FLOAT,
	KS_TYPE_DOUBLE,
	KS_TYPE_CSTRING,
	KS_TYPE_LSTRING,
	KS_TYPE_PTR,
	KS_TYPE_USERDATA,
	KS_TYPE_SCRIPT_TABLE,
	KS_TYPE_SCRIPT_FUNCTION,
	KS_TYPE_SCRIPT_COROUTINE,
	KS_TYPE_SCRIPT_ANY
} Ks_Type;

typedef struct Ks_LStr {
	ks_str data;
	ks_size len;
} Ks_LStr;

typedef struct Ks_UserData {
	ks_ptr data;
	ks_size size;
} Ks_UserData;

#define KS_LSTR(str, len) Ks_LStr { str, len }
#define KS_USERDATA(data) Ks_UserData { &data, sizeof(data) }
#define KS_USERDATA_SZ(data, size) Ks_UserData { &data, size }

