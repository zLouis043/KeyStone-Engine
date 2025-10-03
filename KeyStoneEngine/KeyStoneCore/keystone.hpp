#pragma once

#include "keystone.h"

#include <string>

#include <memory>
#include <sol/sol.hpp>
#include <format>

static void *lua_custom_Alloc(void *ud, void *ptr, size_t osize, size_t nsize);

namespace ks {

namespace log {

    enum class Level {
        L_TRACE,
        L_DEBUG,
        L_INFO,
        L_WARN,
        L_ERROR,
        L_CRITICAL
    };


    template <class... Args>
    void log(Level lvl, std::format_string<Args...> fmt_str, Args&&... args) {

        Ks_Log_Level level = KS_LOG_LVL_INFO;

        switch (lvl) {
            case Level::L_TRACE:    level = KS_LOG_LVL_TRACE; break;
            case Level::L_DEBUG:    level =  KS_LOG_LVL_DEBUG; break;
            case Level::L_INFO:     level =  KS_LOG_LVL_INFO; break;
            case Level::L_WARN:     level =  KS_LOG_LVL_WARN; break;
            case Level::L_ERROR:    level =  KS_LOG_LVL_ERROR; break;
            case Level::L_CRITICAL: level =  KS_LOG_LVL_CRITICAL; break;
        }

        if (level < ks_log_get_level()) return;

        std::string formatted_message = std::format(fmt_str, std::forward<Args>(args)...);

        ks_log(level, formatted_message.c_str());
    }

    void enable_file_sink(const std::string& filename) {
        ks_log_enable_file_sink(filename.c_str());
    }

    void set_pattern(const std::string& pattern) {
        ks_log_set_pattern(pattern.c_str());
    }

    void set_level(Level lvl) {

        Ks_Log_Level level = KS_LOG_LVL_INFO;

        switch (lvl) {
            case Level::L_TRACE:    level = KS_LOG_LVL_TRACE; break;
            case Level::L_DEBUG:    level = KS_LOG_LVL_DEBUG; break;
            case Level::L_INFO:     level = KS_LOG_LVL_INFO; break;
            case Level::L_WARN:     level = KS_LOG_LVL_WARN; break;
            case Level::L_ERROR:    level = KS_LOG_LVL_ERROR; break;
            case Level::L_CRITICAL: level = KS_LOG_LVL_CRITICAL; break;
        }


        ks_log_set_level(level);
    }

    template <class... Args>
    void trace(std::format_string<Args...> fmt_str, Args&&... args) {
        log(Level::L_TRACE, fmt_str, std::forward<Args>(args)...);
    }

    template <class... Args>
    void debug(std::format_string<Args...> fmt_str, Args&&... args) {
        log(Level::L_DEBUG, fmt_str, std::forward<Args>(args)...);
    }

    template <class... Args>
    void info(std::format_string<Args...> fmt_str, Args&&... args) {
        log(Level::L_INFO, fmt_str, std::forward<Args>(args)...);
    }

    template <class... Args>
    void warn(std::format_string<Args...> fmt_str, Args&&... args) {
        log(Level::L_WARN, fmt_str, std::forward<Args>(args)...);
    }

    template <class... Args>
    void error(std::format_string<Args...> fmt_str, Args&&... args) {
        log(Level::L_ERROR, fmt_str, std::forward<Args>(args)...);
    }

    template <class... Args>
    void critical(std::format_string<Args...> fmt_str, Args&&... args) {
        log(Level::L_CRITICAL, fmt_str, std::forward<Args>(args)...);
    }

};

namespace mem {

    enum class Lifetime {
        USER_MANAGED,
        PERMANENT,
        FRAME,
        SCOPED
    };

    enum class Tag {
        INTERNAL_DATA,
        RESOURCE,
        SCRIPT,
        PLUGIN_DATA,
        GARBAGE,
        COUNT
    };

    void* alloc(size_t size_in_bytes, Ks_Lifetime lifetime, Ks_Tag tag, const std::string& debug_name = ""){
        return ks_alloc_debug(size_in_bytes, lifetime, tag, debug_name.c_str());
    }

    void* alloc(size_t size_in_bytes, ks::mem::Lifetime lifetime, ks::mem::Tag tag, const std::string& debug_name = ""){
        Ks_Lifetime lt = KS_LT_USER_MANAGED;
        Ks_Tag      tg = KS_TAG_INTERNAL_DATA; 

        switch(lifetime){
            case ks::mem::Lifetime::USER_MANAGED: lt = KS_LT_USER_MANAGED; break;
            case ks::mem::Lifetime::PERMANENT: lt = KS_LT_PERMANENT; break;
            case ks::mem::Lifetime::FRAME: lt = KS_LT_FRAME; break;
            case ks::mem::Lifetime::SCOPED: lt = KS_LT_SCOPED; break;
            default:
                KS_LOG_ERROR("An invalid value was given as ks::mem::Lifetime := (%d)", (int)lt);
                return NULL;

        }

        switch(tag){
            case ks::mem::Tag::INTERNAL_DATA: tg = KS_TAG_INTERNAL_DATA; break;
            case ks::mem::Tag::RESOURCE: tg = KS_TAG_RESOURCE; break;
            case ks::mem::Tag::SCRIPT: tg = KS_TAG_SCRIPT; break;
            case ks::mem::Tag::PLUGIN_DATA: tg = KS_TAG_PLUGIN_DATA; break;
            case ks::mem::Tag::GARBAGE: tg = KS_TAG_GARBAGE; break;
            case ks::mem::Tag::COUNT:
            default:
                KS_LOG_ERROR("An invalid value was given as ks::mem::Tag := (%d)", (int)tag);
                return NULL;
        }
        
        return ks_alloc_debug(size_in_bytes, lt, tg, debug_name.c_str());
    }

    void* realloc(void* ptr, size_t new_size_in_bytes){
        return ks_realloc(ptr, new_size_in_bytes);
    }

    template<typename T, size_t count = 0>
    T* alloc_t(Ks_Lifetime lifetime, Ks_Tag tag, const std::string& debug_name = ""){
        return static_cast<T*>(alloc(sizeof(T) * count, lifetime, tag, debug_name));
    }

    template<typename T, size_t count = 0>
    T* alloc_t(ks::mem::Lifetime lifetime, ks::mem::Tag tag, const std::string& debug_name = "") {
        return static_cast<T*>(alloc(sizeof(T) * count, lifetime, tag, debug_name));
    }

    template <typename T>
    void dealloc(T* ptr){
        ks_dealloc(static_cast<void*>(ptr));
    }

    void set_frame_capacity(size_t frame_mem_capacity_in_bytes = 64 * 1024 /*64 kb*/){
        ks_set_frame_capacity(frame_mem_capacity_in_bytes);
    }
};

namespace script {

    class TypeManager {
        template<typename T, typename... Args>
        void register_type(const std::string& type_name, Args&&... args){

        }
    };

    class ScriptManager {
    public:
        bool init(){
            raw_state = lua_newstate(lua_custom_Alloc, nullptr);
            if(!raw_state) return false;
            sol::state_view view(raw_state);
            view.open_libraries(sol::lib::base, sol::lib::io, sol::lib::string, sol::lib::math);

            return true;
        }
        void shutdown(){
            if (raw_state) {
                lua_close(raw_state);
                raw_state = nullptr;
            }
        }

        sol::protected_function_result script_file(const std::string& file_path){
            return view().script_file(file_path);
        }

        sol::protected_function_result script(const std::string& script){
            return view().script(script);
        }

        template<typename T, typename... Args>
        void register_type(const std::string& type_name, Args&&... args){
            tm.register_type<T>(type_name, std::forward<Args>(args)...);
        }

        lua_State* get_raw_state() { return raw_state; }

        sol::state_view view(){
            return sol::state_view(raw_state);
        }

    private:
        TypeManager tm;
        lua_State* raw_state;
    };
}

};

static void *lua_custom_Alloc(void *ud, void *ptr, size_t osize, size_t nsize){
    if (nsize == 0) {
        if (ptr) {
            ks::mem::dealloc(ptr);
        }
        return nullptr;
    }else if(ptr == nullptr){
        return ks::mem::alloc(nsize, ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::SCRIPT, "LuaData");
    }

    return ks::mem::realloc(ptr, nsize);
}