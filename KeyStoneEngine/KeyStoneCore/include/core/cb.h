#pragma once
#include "defines.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Ks_Payload {
    ks_ptr  data;
    ks_size size;
    ks_bool owns_data;
    ks_no_ret(*free_fn)(ks_ptr);
} Ks_Payload;

typedef void (*ks_callback)(Ks_Payload payload);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#define KS_PAYLOAD(...) Ks_Payload{__VA_ARGS__}
#define KS_NO_PAYLOAD Ks_Payload{.data = nullptr, .size = 0, .owns_data = false, .free_fn = nullptr}
#else
#define KS_PAYLOAD(...) (Ks_Payload){__VA_ARGS__}
#define KS_NO_PAYLOAD (Ks_Payload){.data = nullptr, .size = 0, .owns_data = false, .free_fn = nullptr}
#endif