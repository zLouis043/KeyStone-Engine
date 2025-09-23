#pragma once

#include <sol/sol.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <any>
#include <typeindex>
#include <variant>

using LuaEventData = sol::table;

template<typename T>
struct EventDataAdapter {
    static sol::table toLuaTable(sol::state_view lua, const T& data);
    static T fromLuaTable(sol::table luaData);
};

template<>
struct EventDataAdapter<LuaEventData> {
    static sol::table toLuaTable(sol::state_view lua, const LuaEventData& data) {
        return data;
    }
    
    static LuaEventData fromLuaTable(sol::table luaData) {
        return luaData;
    }
};

class EventManager {
public:
    template<typename EventData>
    using EventCallback = std::function<void(const EventData&)>;
    
    using LuaEventCallback = sol::protected_function;

    EventManager() : lua(nullptr) {};
    EventManager(lua_State* L);

    void init(lua_State* L);
    void cleanup();
    
    template<typename EventData>
    void register_event_type(const std::string& eventType);

    void register_event_type_lua(const std::string& eventType);
    
    template<typename EventData>
    void publish_event(const std::string& eventType, const EventData& eventData);
    void publish_event_lua(const std::string& eventType, sol::table luaData);
    
    template<typename EventData>
    void subscribe(const std::string& eventType, EventCallback<EventData> callback);
    void subscribe_lua(const std::string& eventType, sol::protected_function callback);
    
    void init_lua_bindings(lua_State* L);

private:
    enum class EventTypeKind {
        CPP_TYPED, 
        LUA_GENERIC
    };

    struct EventSubscription {
    public:
        EventSubscription(EventTypeKind k, std::type_index type, 
                         std::function<sol::table(sol::state_view, const std::any&)> to_lua_conv,
                         std::function<std::any(sol::table)> from_lua_conv)
            : kind(k), data_type(type), to_lua_conv(to_lua_conv), from_lua_conv(from_lua_conv) {}

        EventTypeKind kind;
        std::vector<std::function<void(const std::any&)>> cpp_cbs;
        std::vector<LuaEventCallback> lua_cbs;
        std::type_index data_type;
        std::function<sol::table(sol::state_view, const std::any&)> to_lua_conv;
        std::function<std::any(sol::table)> from_lua_conv;
    };

    lua_State* lua;
    std::unordered_map<std::string, EventSubscription> event_subs;
};

#include "event.inl" 