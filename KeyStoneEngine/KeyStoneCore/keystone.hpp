#pragma once

#include "keystone.h"

#include <string>

#include <memory>
#include <format>
#include <typeindex>
#include <unordered_map>

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
        T* type = static_cast<T*>(alloc(sizeof(T) * count, lifetime, tag, debug_name));
        new(type) T();
        return type;
    }

    template<typename T, size_t count = 0>
    T* alloc_t(ks::mem::Lifetime lifetime, ks::mem::Tag tag, const std::string& debug_name = "") {
        T* type = static_cast<T*>(alloc(sizeof(T) * count, lifetime, tag, debug_name));
        new(type) T();
        return type;
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
    /*
    class Error : public std::runtime_error {
    public:
        Error(const std::string& msg) : std::runtime_error(msg) {}
    };

    class Object;
    class Table;
    class Function;

    class Object {
    protected:
        Ks_Script_Ctx m_ctx = nullptr;
        Ks_Script_Object m_obj;

        void invalidate() { m_ctx = nullptr; }
    public:
        Object() { m_obj = ks_script_create_invalid_obj(nullptr); }

        Object(Ks_Script_Ctx ctx, Ks_Script_Object obj) : m_ctx(ctx), m_obj(obj) {}

        virtual ~Object() {
            if (m_ctx) {
                ks_script_free_obj(m_ctx, m_obj);
            }
        }

        Object(Object&& other) noexcept : m_ctx(other.m_ctx), m_obj(other.m_obj) {
            other.invalidate();
        }

        Object& operator=(Object&& other) noexcept {
            if (this != &other) {
                if (m_ctx) ks_script_free_obj(m_ctx, m_obj);

                m_ctx = other.m_ctx;
                m_obj = other.m_obj;
                other.invalidate();
            }
            return *this;
        }

        Object(const Object&) = delete;
        Object& operator=(const Object&) = delete;

        bool valid() const { return ks_script_obj_is_valid(m_ctx, m_obj); }
        Ks_Script_Ctx ctx() const { return m_ctx; }
        Ks_Script_Object raw() const { return m_obj; }
        Ks_Script_Object_Type type() const { return m_obj.type; }

        template<typename T>
        bool is() const {
            if (!m_ctx) return false;
            if constexpr (std::is_same_v<T, double> || std::is_same_v<T, int> || std::is_same_v<T, float>)
                return m_obj.type == KS_SCRIPT_OBJECT_TYPE_NUMBER;
            else if constexpr (std::is_same_v<T, std::string>)
                return m_obj.type == KS_SCRIPT_OBJECT_TYPE_STRING;
            else if constexpr (std::is_same_v<T, bool>)
                return m_obj.type == KS_SCRIPT_OBJECT_TYPE_BOOLEAN;
            return false;
        }

        template<typename T>
        T as() const {
            if (!m_ctx) throw Error("Object invalid");
            if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
                return static_cast<T>(ks_script_obj_as_number(m_ctx, m_obj));
            }
            else if constexpr (std::is_same_v<T, bool>) {
                return ks_script_obj_as_boolean(m_ctx, m_obj);
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                const char* s = ks_script_obj_as_str(m_ctx, m_obj);
                return s ? std::string(s) : std::string();
            }
            else {
                throw Error("Unsupported type cast");
            }
        }
    };

    class Context {
        Ks_Script_Ctx m_ctx;
        bool m_owns;
    public:
        explicit Context(bool create_new = true) : m_owns(create_new) {
            if (create_new) m_ctx = ks_script_create_ctx();
            else m_ctx = nullptr;
        }

        Context(Ks_Script_Ctx ctx, bool take_ownership = false)
            : m_ctx(ctx), m_owns(take_ownership) {}

        ~Context() {
            if (m_owns && m_ctx) {
                ks_script_destroy_ctx(m_ctx);
            }
        }

        Ks_Script_Ctx raw() const { return m_ctx; }

        Object do_string(const std::string& code) {
            auto res = ks_script_do_string(m_ctx, code.c_str());
            check_error(res);
            return Object(m_ctx, res);
        }

        template<typename T>
        void set(const std::string& name, T&& value) {
            // TODO: Implementare un converter generico T -> Ks_Script_Object
            // Per ora supportiamo solo alcuni tipi base come esempio
            if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
                auto obj = ks_script_create_number(m_ctx, static_cast<double>(value));
                ks_script_set_global(m_ctx, name.c_str(), obj);
                // Non serve free_obj qui perch� � un numero (copiato per valore)
            }
            // ... altri tipi ...
        }

        Object get(const std::string& name) {
            return Object(m_ctx, ks_script_get_global(m_ctx, name.c_str()));
        }
    private:
        void check_error(Ks_Script_Object possible_err_obj) {
            if (possible_err_obj.type == KS_SCRIPT_OBJECT_TYPE_NIL ||
                possible_err_obj.type == KS_SCRIPT_OBJECT_TYPE_UNKNOWN) { // O altro modo per rilevare fallimento
                // Verifica se c'� un errore nel contesto
                if (ks_script_get_last_error(m_ctx) != KS_SCRIPT_ERROR_NONE) {
                    throw Error(ks_script_get_last_error_str(m_ctx));
                }
            }
        }
    };
    */
};
namespace asset {
    using handle = uint32_t;

    constexpr uint32_t invalid_handle = KS_INVALID_HANDLE;
    constexpr Ks_AssetData invalid_data = KS_INVALID_ASSET_DATA;

    template <typename Derived>
    class Asset {
    public:
        static Ks_AssetData load_from_file_wrapper(const char* file_path) {
            Derived* asset = static_cast<Derived*>(Derived::create_impl());
            if (!asset) return nullptr;

            if (!asset->load_impl(file_path)) {
                Derived::destroy_impl(asset);
                return invalid_data;
            }

            return static_cast<Ks_AssetData>(asset);
        }

        static Ks_AssetData load_from_file_wrapper(const uint8_t* data) {
            Derived* asset = static_cast<Derived*>(Derived::create_impl());
            if (!asset) return nullptr;

            if (!asset->load_impl(data)) {
                Derived::destroy_impl(asset);
                return invalid_data;
            }

            return static_cast<Ks_AssetData>(asset);
        }

        static void destroy_wrapper(Ks_AssetData data) {
            Derived* asset = static_cast<Derived*>(data);
            Derived::destroy_impl(asset);
        }
    protected:
        static void* create_impl() {
            return ks::mem::alloc_t<Derived>(
                ks::mem::Lifetime::USER_MANAGED,
                ks::mem::Tag::RESOURCE,
                typeid(Derived).name()
            );
        }

        static void destroy_impl(void* asset) {
            Derived* derived_asset = static_cast<Derived*>(asset);
            derived_asset->~Derived();
            ks::mem::dealloc(derived_asset);
        }
    };

    class AssetsManager {
    public:
        AssetsManager() {
            am = ks_assets_manager_create();
        }

        ~AssetsManager() {
            ks_assets_manager_destroy(am);
        }

        template <typename T>
        void register_asset_type(const std::string& type_name) {

            static_assert(std::is_base_of_v<Asset<T>, T>,
                "Asset type must inherit from AssetBase");

            auto found = types.find(std::type_index(typeid(T)));
            if (found != types.end()) return;

            Ks_IAsset iasset = {
                .load_from_file_fn = &T::load_from_file_wrapper,
                .load_from_data_fn = &T::load_from_file_wrapper,
                .destroy_fn = &T::destroy_wrapper
            };

            types.emplace(std::type_index(typeid(T)), type_name);

            ks_assets_manager_register_asset_type(
                am, type_name.c_str(), iasset
            );
        }

        template <typename T>
        handle load_asset(const std::string& asset_name, const std::string& file_path) {
            auto found = types.find(std::type_index(typeid(T)));
            if (found == types.end()) {
                throw std::runtime_error("Trying to load an asset without registering first its type");
            }

            std::string type_name = found->second;

            Ks_Handle asset_handle = ks_assets_manager_load_asset_from_file(
                am, type_name.c_str(), asset_name.c_str(), file_path.c_str()
            );

            return asset_handle;
        }

        template <typename T>
        handle load_asset(const std::string& asset_name, const Ks_UserData data) {
            auto found = types.find(std::type_index(typeid(T)));
            if (found == types.end()) {
                throw std::runtime_error("Trying to load an asset without registering first its type");
            }

            std::string type_name = found->second;

            Ks_Handle asset_handle = ks_assets_manager_load_asset_from_data(
                am, type_name.c_str(), asset_name.c_str(), data
            );

            return asset_handle;
        }

        handle get_asset(const std::string& asset_name) {
            return ks_assets_manager_get_asset(am, asset_name.c_str());
        }

        uint32_t get_asset_ref_count(handle handle) {
            return ks_assets_manager_get_ref_count(am, handle);
        }

        template <typename T>
        T* get_asset_data(handle handle) {
            T* data = static_cast<T*>(ks_assets_manager_get_data(am, handle));
            return data;
        }

        void asset_release(const std::string& asset_name) {
            handle handle = get_asset(asset_name);
            ks_assets_manager_asset_release(
                am, handle
            );
        }

        void asset_release(handle handle) {
            ks_assets_manager_asset_release(
                am, handle
            );
        }

    private:
        std::unordered_map<std::type_index, std::string> types;
        Ks_AssetsManager am;
    };
}
};