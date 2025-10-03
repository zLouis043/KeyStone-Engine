#pragma once

#include "core/defines.h"

#ifdef __cplusplus
extern "C" {
#endif

enum Ks_Log_Level {
    KS_LOG_LVL_TRACE,
    KS_LOG_LVL_DEBUG,
    KS_LOG_LVL_INFO,
    KS_LOG_LVL_WARN,
    KS_LOG_LVL_ERROR,
    KS_LOG_LVL_CRITICAL
};

KS_API void ks_logf(Ks_Log_Level level, const char* fmt, ...);
KS_API void ks_log(Ks_Log_Level level, const char* str);
KS_API void ks_log_enable_file_sink(const char* filename); 
KS_API void ks_log_set_pattern(const char* pattern);
KS_API void ks_log_set_level(Ks_Log_Level level);
KS_API Ks_Log_Level ks_log_get_level();

#ifdef __cplusplus
} // extern "C"
#endif

#define KS_LOG_TRACE(...) ks_logf(KS_LOG_LVL_TRACE, __VA_ARGS__)
#define KS_LOG_DEBUG(...) ks_logf(KS_LOG_LVL_INFO, __VA_ARGS__)
#define KS_LOG_INFO(...) ks_logf(KS_LOG_LVL_INFO, __VA_ARGS__)
#define KS_LOG_WARN(...) ks_logf(KS_LOG_LVL_INFO, __VA_ARGS__)
#define KS_LOG_ERROR(...) ks_logf(KS_LOG_LVL_INFO, __VA_ARGS__)
#define KS_LOG_CRITICAL(...) ks_logf(KS_LOG_LVL_CRITICAL, __VA_ARGS__)

