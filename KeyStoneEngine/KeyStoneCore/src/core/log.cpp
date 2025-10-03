#include "core/log.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <memory>
#include <vector>
#include <cstdarg>
#include <string> 
#include <string_view>

static std::shared_ptr<spdlog::logger> s_logger = nullptr;

static const spdlog::level::level_enum ks_to_spdlog(Ks_Log_Level lvl){
    switch(lvl){
        case KS_LOG_LVL_TRACE: return spdlog::level::trace;
        case KS_LOG_LVL_DEBUG: return spdlog::level::debug;
        case KS_LOG_LVL_INFO:  return spdlog::level::info;
        case KS_LOG_LVL_WARN:  return spdlog::level::warn;
        case KS_LOG_LVL_ERROR: return spdlog::level::err;
        case KS_LOG_LVL_CRITICAL: return spdlog::level::critical;
        default: return spdlog::level::off;
    };
}

static std::shared_ptr<spdlog::logger>& get_logger() {
    if (!s_logger) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::trace);

        std::vector<spdlog::sink_ptr> sinks{ console_sink };
        s_logger = std::make_shared<spdlog::logger>("KSLOG", sinks.begin(), sinks.end());

        s_logger->set_level(spdlog::level::trace); 
        s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        spdlog::register_logger(s_logger);
    }
    return s_logger;
}

void ks_log_enable_file_sink(const char* filename){
    auto logger = get_logger(); 

    for (const auto& sink : logger->sinks()) {
        if (dynamic_cast<spdlog::sinks::basic_file_sink_mt*>(sink.get())) {
            return; 
        }
    }

    try {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
        file_sink->set_level(spdlog::level::trace);
        logger->sinks().push_back(file_sink);
    }
    catch (const spdlog::spdlog_ex& ex) {
        logger->error("Failed to add file sink: {}", ex.what());
    }
}

void ks_log_set_pattern(const char* pattern){
    auto logger = get_logger();
    logger->set_pattern(pattern);
}

void ks_log_set_level(Ks_Log_Level level){
    auto logger = get_logger();
    auto spd_level = ks_to_spdlog(level);

    logger->set_level(spd_level);
    for (const auto& sink : logger->sinks()) {
        sink->set_level(spd_level);
    }
}

void ks_logf(Ks_Log_Level level, const char* fmt, ...){

    auto logger = get_logger(); 

    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return;
    }

    std::string buffer(size + 1, '\0');
    std::vsnprintf(&buffer[0], buffer.size(), fmt, args);
    va_end(args);
    buffer.pop_back();

    switch (level) {
    case KS_LOG_LVL_TRACE:
        logger->trace(buffer);
        break;
    case KS_LOG_LVL_DEBUG:
        logger->debug(buffer);
        break;
    case KS_LOG_LVL_INFO:
        logger->info(buffer);
        break;
    case KS_LOG_LVL_WARN:
        logger->warn(buffer);
        break;
    case KS_LOG_LVL_ERROR:
        logger->error(buffer);
        break;
    case KS_LOG_LVL_CRITICAL:
        logger->critical(buffer);
        break;
    }
}

void ks_log(Ks_Log_Level level, const char* str){

    auto logger = get_logger();

    std::string_view sv(str, strlen(str));

    switch (level) {
    case KS_LOG_LVL_TRACE:
        logger->trace(sv);
        break;
    case KS_LOG_LVL_DEBUG:
        logger->debug(sv);
        break;
    case KS_LOG_LVL_INFO:
        logger->info(sv);
        break;
    case KS_LOG_LVL_WARN:
        logger->warn(sv);
        break;
    case KS_LOG_LVL_ERROR:
        logger->error(sv);
        break;
    case KS_LOG_LVL_CRITICAL:
        logger->critical(sv);
        break;
    }
}

Ks_Log_Level ks_log_get_level() {
    auto logger = get_logger();
    auto lvl = logger->level();
    switch (lvl) {
        case spdlog::level::trace: return KS_LOG_LVL_TRACE;
        case spdlog::level::debug: return KS_LOG_LVL_DEBUG;
        case spdlog::level::info:  return KS_LOG_LVL_INFO;
        case spdlog::level::warn:  return KS_LOG_LVL_WARN;
        case spdlog::level::err:   return KS_LOG_LVL_ERROR;
        case spdlog::level::critical: return KS_LOG_LVL_CRITICAL;
        default: return (Ks_Log_Level)-1;
    }
}