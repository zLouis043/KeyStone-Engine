#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <format>

enum class LogLevel {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO = 2,
    LOG_WARN = 3,
    LOG_ERROR = 4,
    LOG_FATAL = 5
};

enum class OutputType {
    STDOUT,
    STDERR,
    FILE,
    BUFFER
};

class PlatformUtils;

class ColorManager {
private:
    static std::unique_ptr<PlatformUtils> platformUtils;
    static PlatformUtils& getPlatformUtils();

public:
    static bool supportsColor(std::ostream& stream);
    
    static std::string getColorCode(LogLevel level, bool useColors) {
        if (!useColors) return "";
        
        switch (level) {
            case LogLevel::LOG_TRACE:   return "\033[37m";     // White
            case LogLevel::LOG_DEBUG:   return "\033[36m";     // Cyan
            case LogLevel::LOG_INFO:    return "\033[32m";     // Green
            case LogLevel::LOG_WARN:    return "\033[33m";     // Yellow
            case LogLevel::LOG_ERROR:   return "\033[31m";     // Red
            case LogLevel::LOG_FATAL:   return "\033[35m";     // Magenta
            default:                return "";
        }
    }

    static std::string getResetCode(bool useColors) {
        return useColors ? "\033[0m" : "";
    }
};

class OutputHandler {
public:
    virtual ~OutputHandler() = default;
    virtual void write(const std::string& message) = 0;
    virtual bool supportsColors() const = 0;
};

class StdoutHandler : public OutputHandler {
public:
    void write(const std::string& message) override {
        std::cout << message << std::flush;
    }
    
    bool supportsColors() const override {
        return ColorManager::supportsColor(std::cout);
    }
};

class StderrHandler : public OutputHandler {
public:
    void write(const std::string& message) override {
        std::cerr << message << std::flush;
    }
    
    bool supportsColors() const override {
        return ColorManager::supportsColor(std::cerr);
    }
};

class FileHandler : public OutputHandler {
private:
    std::ofstream file;
    
public:
    explicit FileHandler(const std::string& filename) : file(filename, std::ios::app) {
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open log file: " + filename);
        }
    }
    
    void write(const std::string& message) override {
        file << message << std::flush;
    }
    
    bool supportsColors() const override {
        return false;
    }
};

class BufferHandler : public OutputHandler {
private:
    std::ostringstream buffer;
    
public:
    void write(const std::string& message) override {
        buffer << message;
    }
    
    bool supportsColors() const override {
        return false;
    }
    
    std::string getBuffer() const {
        return buffer.str();
    }
    
    void clearBuffer() {
        buffer.str("");
        buffer.clear();
    }
};

class Logger {
private:
    LogLevel minLevel;
    std::unique_ptr<OutputHandler> outputHandler;
    bool useColors;
    mutable std::mutex logMutex;

    std::string levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::LOG_TRACE: return "TRACE";
            case LogLevel::LOG_DEBUG: return "DEBUG";
            case LogLevel::LOG_INFO:  return "INFO ";
            case LogLevel::LOG_WARN:  return "WARN ";
            case LogLevel::LOG_ERROR: return "ERROR";
            case LogLevel::LOG_FATAL: return "FATAL";
            default:              return "UNKNOWN";
        }
    }

    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    template<typename... Args>
    void logMessage(LogLevel level, const std::string& format, Args&&... args) {
        if (level < minLevel) return;

        std::lock_guard<std::mutex> lock(logMutex);

        std::string message;
        if constexpr (sizeof...(args) > 0) {
            try {
                message = std::vformat(format, std::make_format_args(args...));
            } catch (const std::format_error& e) {
                message = format + " [FORMAT ERROR: " + e.what() + "]";
            }
        } else {
            message = format;
        }

        std::string timestamp = getCurrentTimestamp();
        std::string levelStr = levelToString(level);
        
        bool colorsEnabled = useColors && outputHandler->supportsColors();
        std::string colorCode = ColorManager::getColorCode(level, colorsEnabled);
        std::string resetCode = ColorManager::getResetCode(colorsEnabled);

        std::ostringstream logLine;
        logLine << colorCode << "[" << timestamp << "] [" << levelStr << "] " 
                << message << resetCode << "\n";

        outputHandler->write(logLine.str());
    }
    

public:
    Logger(LogLevel minLevel = LogLevel::LOG_INFO, OutputType outputType = OutputType::STDOUT,
           const std::string& filename = "", bool useColors = true)
        : minLevel(minLevel), useColors(useColors) {
        
        setOutput(outputType, filename);
    }

    void setMinLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(logMutex);
        minLevel = level;
    }

    LogLevel getMinLevel() const {
        std::lock_guard<std::mutex> lock(logMutex);
        return minLevel;
    }

    void setColors(bool enabled) {
        std::lock_guard<std::mutex> lock(logMutex);
        useColors = enabled;
    }

    void setOutput(OutputType outputType, const std::string& filename = "") {
        std::lock_guard<std::mutex> lock(logMutex);
        
        switch (outputType) {
            case OutputType::STDOUT:
                outputHandler = std::make_unique<StdoutHandler>();
                break;
            case OutputType::STDERR:
                outputHandler = std::make_unique<StderrHandler>();
                break;
            case OutputType::FILE:
                if (filename.empty()) {
                    throw std::invalid_argument("Filename required for file output");
                }
                outputHandler = std::make_unique<FileHandler>(filename);
                break;
            case OutputType::BUFFER:
                outputHandler = std::make_unique<BufferHandler>();
                break;
        }
    }

    BufferHandler* getBufferHandler() {
        return dynamic_cast<BufferHandler*>(outputHandler.get());
    }

    template<typename... Args>
    void trace(const std::string& format, Args&&... args) {
        logMessage(LogLevel::LOG_TRACE, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        logMessage(LogLevel::LOG_DEBUG, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        logMessage(LogLevel::LOG_INFO, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(const std::string& format, Args&&... args) {
        logMessage(LogLevel::LOG_WARN, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        logMessage(LogLevel::LOG_ERROR, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void fatal(const std::string& format, Args&&... args) {
        logMessage(LogLevel::LOG_FATAL, format, std::forward<Args>(args)...);
    }
};

extern Logger globalLogger;

#define LOG_TRACE(...) globalLogger.trace(__VA_ARGS__)
#define LOG_DEBUG(...) globalLogger.debug(__VA_ARGS__)
#define LOG_INFO(...)  globalLogger.info(__VA_ARGS__)
#define LOG_WARN(...)  globalLogger.warn(__VA_ARGS__)
#define LOG_ERROR(...) globalLogger.error(__VA_ARGS__)
#define LOG_FATAL(...) globalLogger.fatal(__VA_ARGS__)