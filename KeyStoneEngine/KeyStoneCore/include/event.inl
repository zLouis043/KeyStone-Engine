#include "logger.h"

template<typename EventData>
void EventManager::register_event_type(const std::string& eventType) {
    if (event_subs.find(eventType) == event_subs.end()) {

        auto toConv = [](sol::state_view lua, const std::any& data) -> sol::table {
            return EventDataAdapter<EventData>::toLuaTable(lua, 
                std::any_cast<const EventData&>(data));
        };
        
        auto fromConv = [](sol::table luaData) -> std::any {
            return EventDataAdapter<EventData>::fromLuaTable(luaData);
        };

        event_subs.emplace(
            eventType,
            EventSubscription(
                EventTypeKind::CPP_TYPED, 
                typeid(EventData), 
                toConv, 
                fromConv
            )
        );

    }
}

template<typename EventData>
void EventManager::publish_event(const std::string& eventType, const EventData& eventData) {
    auto it = event_subs.find(eventType);
    if (it == event_subs.end()) {
        register_event_type<EventData>(eventType);
        it = event_subs.find(eventType);
    }

    std::any dataAsAny = eventData;
    for (auto& callback : it->second.cpp_cbs) {
        callback(dataAsAny);
    }

    if (!it->second.lua_cbs.empty()) {
        sol::state_view view(lua);
        sol::table luaTable = it->second.to_lua_conv(view, dataAsAny);
        for (auto& callback : it->second.lua_cbs) {
            callback(luaTable);
        }
    }
}

template<typename EventData>
void EventManager::subscribe(const std::string& eventType, EventCallback<EventData> callback) {
    auto it = event_subs.find(eventType);
    if (it == event_subs.end()) {
        register_event_type<EventData>(eventType);
        it = event_subs.find(eventType);
    }

    auto wrapper = [callback, &eventType](const std::any& data) {
        try {
            callback(std::any_cast<const EventData&>(data));
        } catch (const std::bad_any_cast&) {
            LOG_ERROR("Error: Bad any cast in event callback for event type: {}", eventType.c_str());
        }
    };

    it->second.cpp_cbs.push_back(wrapper);
}