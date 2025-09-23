#include "event.h"
#include <iostream>

EventManager::EventManager(lua_State* L) : lua(L) {}

void EventManager::init(lua_State* L){
    lua = L;
}

void EventManager::cleanup(){
    event_subs.clear();
}

void EventManager::register_event_type_lua(const std::string& eventType) {
    if (event_subs.find(eventType) == event_subs.end()) {
        auto toConv = [](sol::state_view lua, const std::any& data) -> sol::table {
            try {
                return std::any_cast<LuaEventData>(data);
            } catch (const std::bad_any_cast&) {
                throw std::runtime_error("Invalid data type for Lua event");
            }
        };
        
        auto fromConv = [](sol::table luaData) -> std::any {
            return luaData;
        };
        
        event_subs.emplace(
            eventType, 
            EventSubscription(
                EventTypeKind::LUA_GENERIC, 
                typeid(LuaEventData), 
                toConv, 
                fromConv
            )
        );
    }
}

void EventManager::publish_event_lua(const std::string& eventType, sol::table luaData) {
    auto it = event_subs.find(eventType);
    if (it == event_subs.end()) {
        register_event_type_lua(eventType);
        it = event_subs.find(eventType);
    }

    EventSubscription& sub = it->second;
    
    if (sub.kind == EventTypeKind::LUA_GENERIC) {
        std::any cppData = sub.from_lua_conv(luaData);
        
        for (auto& callback : sub.lua_cbs) {
            callback(luaData);
        }
        
        for (auto& callbackAny : sub.cpp_cbs) {
            try {
                auto callback = std::any_cast<EventCallback<LuaEventData>>(callbackAny);
                callback(std::any_cast<LuaEventData>(cppData));
            } catch (...) {}
        }
    } else {
        try {

            sol::state_view view(lua);
            std::any cppData = sub.from_lua_conv(luaData);
        
            for (auto& callback : sub.cpp_cbs) {
                callback(cppData);
            }
        
            if (!sub.lua_cbs.empty()) {
                sol::state_view view(lua);
                sol::table convertedTable = sub.to_lua_conv(view, cppData);
                for (auto& callback : sub.lua_cbs) {
                    callback(convertedTable);
                }
            }
        } catch (const std::exception& e) {
           LOG_ERROR("Error converting Lua data: {}", e.what());
        }
    }
}

void EventManager::subscribe_lua(const std::string& eventType, sol::protected_function callback) {
    auto it = event_subs.find(eventType);
    if (it == event_subs.end()) {
        register_event_type_lua(eventType);
        it = event_subs.find(eventType);
    }
    it->second.lua_cbs.push_back(callback);
}

void EventManager::init_lua_bindings(lua_State* L) {
    sol::state_view lua(L);
    
    sol::table event_manager = lua.create_named_table("event_manager");
    
    event_manager["register_event"] = [this](const std::string& eventType) {
        this->register_event_type_lua(eventType);
    };
    
    event_manager["publish"] = [this](const std::string& eventType, sol::table eventData) {
        this->publish_event_lua(eventType, eventData);
    };
    
    event_manager["subscribe"] = [this](const std::string& eventType, sol::protected_function callback) {
        this->subscribe_lua(eventType, callback);
    };
}