#pragma once

#include "result.h"
#include "event.h"
#include "asset.h"
#include "timer.h"
#include "luawrap.h"
#include "system_manager.h"

class engine;

struct EngineContext {
public:

    Result<void> init(){
        auto res = lua_state.init();
        if(!res.ok()){
            return Result<void>::Err("Error while loading engine context: {}", res.what());
        }

        event_manager.init(&lua_state);

        return Result<void>::Ok();
    }

    void cleanup(){
        event_manager.cleanup();
        assets_manager.clear_all_assets();
        lua_state.cleanup();
        MemoryManager::shutdown();
    }

    Result<ISystem*> get_system(const std::string& sys_name){
        return system_manager.get_system(sys_name);
    }

    template<typename T>
    Result<T*> get_system_as(const std::string& sys_name){
        return system_manager.get_system_as<T>(sys_name);
    }

    Timer* create_timer(){
        return time_manager.create_timer();
    }

    LuaState& get_lua(){ return lua_state; }
    EventManager& get_event_manager(){ return event_manager; }
    AssetsManager& get_assets_manager(){ return assets_manager; }
    MemoryManager& get_mem_manager() { return MemoryManager::get_instance(); }
    size_t get_frame_count() { return frame_count; }
private:
    friend engine;
    size_t frame_count = 0;

    AssetsManager assets_manager;
    TimeManager   time_manager;
    EventManager  event_manager;
    SystemManager system_manager;
    LuaState lua_state;
};
