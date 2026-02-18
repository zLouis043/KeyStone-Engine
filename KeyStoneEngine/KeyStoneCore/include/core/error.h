#pragma once

#include "../core/defines.h"
#include "../core/types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Ks_Error_Level {
	KS_ERROR_LEVEL_NONE = 0,
	KS_ERROR_LEVEL_WARNING,
	KS_ERROR_LEVEL_BASE,
	KS_ERROR_LEVEL_CRITICAL,
};

struct Ks_Error_Info {
	ks_uint64 code;
	ks_uint64 timestamp;
	ks_str message;
	ks_str file;
	ks_size line;
};

#define KS_ERR_GET_OWNER(code)  ((ks_uint16)(((ks_uint64)(code) >> 48) & 0xFFFF))
#define KS_ERR_GET_SOURCE(code) ((ks_uint16)(((ks_uint64)(code) >> 32) & 0xFFFF))
#define KS_ERR_GET_LOCAL(code)  ((ks_uint32)((ks_uint64)(code) & 0x00FFFFFFULL))
#define KS_ERR_GET_LEVEL(code)  ((Ks_Error_Level)(((ks_uint64)(code) >> 24) & 0xFF))

#define ks_epush(lvl, owner_prefix, src_prefix, local_code, msg) \
	ks_error_push(lvl, __FILE__, __LINE__, owner_prefix, src_prefix, local_code, msg)

#define ks_epush_fmt(lvl, owner_prefix, src_prefix, local_code, msg, ...)\
	ks_error_push_fmt(lvl, __FILE__, __LINE__, owner_prefix, src_prefix, local_code, msg, __VA_ARGS__)
	
#define ks_epush_s(lvl, owner_prefix, local_code, msg) \
	ks_error_push(lvl, __FILE__, __LINE__, owner_prefix, owner_prefix, local_code, msg)

#define ks_epush_s_fmt(lvl, owner_prefix, local_code, msg, ...)\
	ks_error_push_fmt(lvl, __FILE__, __LINE__, owner_prefix, owner_prefix, local_code, msg, __VA_ARGS__)

KS_API ks_uint16 ks_error_make_module_prefix(ks_str module_name);
KS_API ks_uint16 ks_error_get_module_prefix(ks_str module_name);
KS_API ks_str    ks_error_get_module_prefix_str(ks_uint16 id);

KS_API ks_no_ret ks_error_set_code_info(ks_str module_name, ks_uint64 local_code, ks_str title, ks_str description);
KS_API ks_str    ks_error_get_code_info_desc(ks_uint64 full_code);
KS_API ks_str    ks_error_get_code_info_name(ks_uint64 full_code);

KS_API ks_no_ret ks_error_push(Ks_Error_Level lvl, ks_str file, ks_size line, ks_str owner_prefix, ks_str source_prefix, ks_uint64 local_code, ks_str message);
KS_API ks_no_ret ks_error_push_fmt(Ks_Error_Level lvl, ks_str file, ks_size line, ks_str owner_prefix, ks_str source_prefix, ks_uint64 local_code, ks_str message, ...);
KS_API ks_no_ret ks_error_pop_last();

KS_API ks_size ks_error_count();

KS_API Ks_Error_Info ks_error_get_last_error();

#ifdef __cplusplus
}
#endif