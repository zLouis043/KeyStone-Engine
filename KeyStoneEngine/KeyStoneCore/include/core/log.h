/**
 * @file log.h
 * @brief Loggin system of the engine
 *
 * Provides macros and functions for logging to console or into files.
 *
 * @defgroup Core Core System
 * @brief Core functionalities and base types of the engine.
 * @{
 */
#pragma once

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Defines the severity levels for logging messages.
 */
enum Ks_Log_Level {
  KS_LOG_LVL_TRACE,    ///< Detailed trace information, typically for debugging flow.
  KS_LOG_LVL_DEBUG,    ///< Debugging information, less verbose than trace.
  KS_LOG_LVL_INFO,     ///< General informational messages about application state.
  KS_LOG_LVL_WARN,     ///< Warnings indicating potential issues that don't stop execution.
  KS_LOG_LVL_ERROR,    ///< Errors indicating a failure in a specific operation.
  KS_LOG_LVL_CRITICAL  ///< Critical errors causing application crash or severe failure.
};

/**
 * @brief Logs a formatted message with a specific severity level.
 *
 * @param level The severity level of the log message.
 * @param fmt The format string (printf-style).
 * @param ... Additional arguments corresponding to the format string.
 */
KS_API void ks_logf(Ks_Log_Level level, const char *fmt, ...);

/**
 * @brief Logs a simple string message with a specific severity level.
 *
 * @param level The severity level of the log message.
 * @param str The message string to log.
 */
KS_API void ks_log(Ks_Log_Level level, const char *str);

/**
 * @brief Enables logging output to a file.
 *
 * @param filename The path to the log file. If it exists, it will be appended/overwritten based on implementation.
 */
KS_API void ks_log_enable_file_sink(const char *filename);

/**
 * @brief Sets the formatting pattern for log messages.
 *
 * This typically follows the spdlog pattern syntax (e.g., "[%H:%M:%S] [%l] %v").
 *
 * @param pattern The format pattern string.
 */
KS_API void ks_log_set_pattern(const char *pattern);

/**
 * @brief Sets the minimum logging level. Messages below this level will be ignored.
 *
 * @param level The minimum severity level to output.
 */
KS_API void ks_log_set_level(Ks_Log_Level level);

/**
 * @brief Retrieves the current minimum logging level.
 *
 * @return The current active logging level.
 */
KS_API Ks_Log_Level ks_log_get_level();

#ifdef __cplusplus
} // extern "C"
#endif

// Convenience macros for logging
#define KS_LOG_TRACE(...) ks_logf(KS_LOG_LVL_TRACE, __VA_ARGS__)
#define KS_LOG_DEBUG(...) ks_logf(KS_LOG_LVL_DEBUG, __VA_ARGS__)
#define KS_LOG_INFO(...) ks_logf(KS_LOG_LVL_INFO, __VA_ARGS__)
#define KS_LOG_WARN(...) ks_logf(KS_LOG_LVL_WARN, __VA_ARGS__)
#define KS_LOG_ERROR(...) ks_logf(KS_LOG_LVL_ERROR, __VA_ARGS__)
#define KS_LOG_CRITICAL(...) ks_logf(KS_LOG_LVL_CRITICAL, __VA_ARGS__)
/** @} */