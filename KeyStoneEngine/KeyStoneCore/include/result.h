#pragma once

#include <string>
#include <format>
#include <sstream>
#include <utility>
#include <stdexcept>

template <typename T = void>
class Result;

// Versione migliorata
template <typename... Args>
std::string format_string(const std::string& format, Args&&... args) {
    std::ostringstream oss;
    size_t current_arg = 0;
    size_t format_size = format.size();
    
    for (size_t i = 0; i < format_size; ++i) {
        if (format[i] == '{' && i + 1 < format_size && format[i + 1] == '}') {
            // Gestione migliore degli argomenti
            if (current_arg >= sizeof...(args)) {
                oss << "{}"; // Placeholder vuoto se mancano argomenti
            } else {
                // Utilizza una lambda ricorsiva per estrarre l'argomento corretto
                size_t index = 0;
                auto print_arg = [&](auto&& arg) {
                    if (index++ == current_arg) {
                        oss << arg;
                        return true;
                    }
                    return false;
                };
                
                (print_arg(std::forward<Args>(args)) || ...);
            }
            current_arg++;
            i++; // Skip '}'
        } else {
            oss << format[i];
        }
    }
    
    return oss.str();
}

template <>
class Result<void> {
public:
    static Result<void> Ok() {
        return Result<void>(true);
    }

    static Result<void> Err(const std::string& msg) {
        return Result<void>(false, msg);
    }

    template <typename... FormatArgs>
    static Result<void> Err(const std::string& format, FormatArgs&&... args) {
        std::string formatted_msg = format_string(format, std::forward<FormatArgs>(args)...);
        return Result<void>(false, formatted_msg);
    }

    bool ok() const {
        return success;
    }

    const std::string& what() const {
        if (success) {
            throw std::logic_error("Cannot get error message from successful result");
        }
        return err_msg;
    }

    explicit operator bool() const { return success; }

private:
    Result(bool success, std::string err_msg = "") 
        : success(success), err_msg(std::move(err_msg)) {}
    
    bool success;
    std::string err_msg;
};

template <typename T>
class Result {
public:
    static Result<T> Ok(T val) {
        return Result<T>(std::move(val), true);
    }

    static Result<T> Err(const std::string& msg) {
        return Result<T>(T{}, false, msg);
    }

    template <typename... FormatArgs>
    static Result<T> Err(const std::string& format, FormatArgs&&... args) {
        std::string formatted_msg = format_string(format, std::forward<FormatArgs>(args)...);
        return Result<T>(T{}, false, formatted_msg);
    }

    bool ok() const {
        return success;
    }

    const std::string& what() const {
        if (success) {
            throw std::logic_error("Cannot get error message from successful result");
        }
        return err_msg;
    }

    T& value() {
        if (!success) {
            throw std::runtime_error("Cannot get value from failed result: " + err_msg);
        }
        return val;
    }

    const T& value() const {
        if (!success) {
            throw std::runtime_error("Cannot get value from failed result: " + err_msg);
        }
        return val;
    }

    T& operator*() { return value(); }
    const T& operator*() const { return value(); }

    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

    explicit operator bool() const { return success; }

private:
    Result(T val, bool success, std::string err_msg = "") 
        : val(std::move(val)), success(success), err_msg(std::move(err_msg)) {}
    
    T val;
    bool success;
    std::string err_msg;
};