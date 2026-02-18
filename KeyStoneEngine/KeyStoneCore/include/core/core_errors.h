#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum Ks_Core_Error {
	KS_ERROR_INVALID_ARGUMENT,
	KS_ERROR_NULL_POINTER,
	KS_ERROR_ZERO_SIZE,
	KS_ERROR_TYPE_MISMATCH,
	KS_ERROR_INVALID_HANDLE
};

#ifdef __cplusplus
}
#endif