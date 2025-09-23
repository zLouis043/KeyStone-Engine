#pragma once 

class engine;

#include "result.h"
#include "defines.h"

struct SystemAPI {
    // Name of the system
    const char * name;
    /*
    Priority for every system. It gives the engine the order in which every system has to be run.
    0-99: Input and event handling.
    100-899: Game Logic and Updates
    900-999: Physics, Animations and other similar systems. (pre-rendering)
    1000+: Rendering.
    */
    int priority;
    // List of dependencies for the System that will be resolved by the engine so that the execution of every System will always have the correct order
    const char** dependencies;
    // Count of the dependencies
    int dependencies_count;
    // Pointer to data used by the system
    void* data;
    // Function that gets configs from the config.lua e loads all the settings for the system from that file
    Result<void> (*load_configs)(engine*); 
    // Function called by the engine at very last to cleanup all the resources
    Result<void> (*destroy_fn)(engine*); 
    // Function called by the engine at the start of the app after all the core and plugin systems have been loaded
    Result<void> (*start_app)(engine*); 
    // Function called by the engine at the end of the app loop 
    Result<void> (*end_app)(engine*); 
    // Function called before start the app to load every bindings for every systems with lua
    void (*register_lua_bindings)(engine*); 
    // Function called at the start of the app loop
    void (*begin_loop)(engine*); 
    // Function called at the end of the app loop
    void (*end_loop)(engine*); 
};

struct EngineContext;

class ISystem {
public:
    virtual ~ISystem() = 0;
    virtual std::string get_name() = 0;
    virtual std::vector<std::string> get_deps() = 0;
    virtual size_t get_priority() { return 1; };
    virtual Result<void> on_configs_load    (EngineContext* ctx) = 0;
    virtual Result<void> on_lua_register    (EngineContext* ctx) = 0;
    virtual Result<void> on_start           (EngineContext* ctx) = 0;
    virtual Result<void> on_end             (EngineContext* ctx) = 0;
    virtual Result<void> on_begin_loop      (EngineContext* ctx) = 0;
    virtual Result<void> on_end_loop        (EngineContext* ctx) = 0;
};

#define EXPORT_SYSTEM(plugin_class) \
    extern "C" KSEXPORT ke::shared_ptr<ISystem> KSCALL load_system(){\
        return ke::make_shared<plugin_class>();\
    }
