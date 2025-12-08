/**
 * @file log.h
 * @brief Thread-safe logging subsystem with support for console and file sinks.
 * @ingroup Core
 */
#pragma once

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief Defines the severity levels for log messages.
*/
typedef enum {
	KS_LOG_LVL_TRACE,    ///< Verbose debug information (lowest priority).
	KS_LOG_LVL_DEBUG,    ///< Information useful for debugging software defects.
	KS_LOG_LVL_INFO,     ///< General operational messages (startup, shutdown, etc.).
	KS_LOG_LVL_WARN,     ///< Warnings about potential issues that do not stop execution.
	KS_LOG_LVL_ERROR,    ///< Runtime errors that are recoverable.
	KS_LOG_LVL_CRITICAL  ///< Severe errors causing premature termination or instability.
} Ks_Log_Level;

/**
 * @brief Logs a formatted message to all active sinks.
 * @note This function is thread-safe.
 *
 * @param level Severity level. Messages below the current global level will be ignored.
 * @param fmt The printf-style format string.
 * @param ... Arguments for the format string.
 */
KS_API void ks_logf(Ks_Log_Level level, const char* fmt, ...);

/**
 * @brief Logs a raw string message.
 * @note Faster than ks_logf as it avoids formatting overhead.
 *
 * @param level Severity level.
 * @param str The null-terminated message string.
 */
KS_API void ks_log(Ks_Log_Level level, const char* str);

/**
 * @brief Adds a file sink to the logger.
 * logs will be written to both console and this file.
 *
 * @param filename Path to the log file. If it exists, new logs are appended.
 */
KS_API void ks_log_enable_file_sink(const char* filename);

/**
 * @brief Customizes the log output format.
 * Pattern syntax follows spdlog conventions:
 * - %v: The actual text to log
 * - %t: Thread ID
 * - %P: Process ID
 * - %n: Logger name
 * - %l: Log level
 * - %H: Hour (00-23)
 * - %M: Minute (00-59)
 * - %S: Second (00-59)
 *
 * @param pattern The format pattern string (e.g., "[%H:%M:%S] [%l] %v").
 */
KS_API void ks_log_set_pattern(const char* pattern);

/**
 * @brief Sets the global filtering level.
 * @param level The minimum severity level required for a message to be logged.
 */
KS_API void ks_log_set_level(Ks_Log_Level level);

/**
 * @brief Gets the current global logging level.
 * @return The active logging level.
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