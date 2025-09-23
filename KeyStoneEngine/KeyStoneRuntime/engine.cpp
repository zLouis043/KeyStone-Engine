#include "engine.h"
#include <fstream>

engine::~engine(){
    cleanup();
}

Result<void> engine::init(){

    auto res = p_ctx.init();
    if(!res.ok()){
        return Result<void>::Err(
            "Error during Engine Initialization:\n\t{}",
            res.what()
        );
    }

    return Result<void>::Ok(); 
}

void engine::stop(){
    p_running = false;
}

void engine::cleanup(){
    p_ctx.cleanup();

}

Result<void> engine::set_engine_path(const std::string &path)
{
    p_engine_path = std::filesystem::path(path).parent_path().string();
    return Result<void>::Ok();
}

Result<void> engine::set_project_path(const std::string &path)
{
    if(!std::filesystem::exists(path)){
        return Result<void>::Err(
            "Given invalid file path for project. File path given:\n\t{}",
            path
        );
    }
    p_project_path = std::filesystem::absolute(path).string();
    return Result<void>::Ok();
}

Result<void> engine::load_configs()
{
    std::string config_path = p_project_path + "/./data/config.lua";
    auto res = p_ctx.get_lua().view().script_file(config_path);
    if(!res.valid()){
        return Result<void>::Err(
            "Failed to load config file '{}'.",
            config_path
        );
    }

    return Result<void>::Ok();
}

constexpr const char* core_lib_path = "/./libs/core/";
constexpr const char* plugins_lib_path = "/./libs/plugins/";
const std::vector<std::string> core_libs = {
    "window", "audio", "input", "draw", "filesystem", "serialization"
};

Result<void> engine::load_systems()
{       
    sol::state_view view = p_ctx.get_lua().view();
    if(!view["configs"].valid()){
        return Result<void>::Err("Configs table was not found in config.lua file. Remember to add the 'configs' table in the config.lua file");
    }
    sol::table configs = view["configs"];

    if(!configs["systems"].valid()){
        LOG_WARN("Systems load table not found in configs, loading all default core systems");
        return Result<void>::Ok();
    }
    sol::table systems = configs["systems"];
    
    if(!systems["core"].valid()){
        LOG_WARN("Core systems load table not found in configs, loading all default core systems");
        return Result<void>::Ok();
    }
    sol::object core = systems["core"];

    if(core.is<bool>()){
        bool enable_core = core.as<bool>();
        if(!enable_core){
            return Result<void>::Ok();
        }
        std::string path = p_engine_path + core_lib_path;
        p_ctx.system_manager.load_systems_from_directory(path, core_libs);
    }else if(core.is<sol::table>()){

        std::vector<std::string> libs_to_load;

        for(auto& val : core.as<sol::table>()){
            if(!val.second.is<std::string>()){
                return Result<void>::Err(
                    "Invalid value type for system name. It must be a string"
                );
            }
            std::string system = val.second.as<std::string>();
            libs_to_load.push_back(system);
        }

        std::string path = p_engine_path + core_lib_path;
        auto res = p_ctx.system_manager.load_systems_from_directory(path, libs_to_load);

        if(!res.ok()){
            return Result<void>::Err(
                "Could not load core systems correctly:\n\t{}", res.what()
            );
        }

    }else{
        return Result<void>::Err(
            "Invalid value type for core systems load table. Must be a boolean to switch"
            " core systems on or off or an of all the core systems the app requires."
        );
    }

    if(!systems["plugins"].valid()){
        return Result<void>::Ok();
    }
    sol::object plugins = systems["plugins"];

    if(!plugins.is<sol::table>()){
        return Result<void>::Err(
            "Invalid value type for plugin systems load table. It must be an array of the names of the plugins "
            "dynamic libraries"
        );
    }

    std::vector<std::string> plugins_lib_to_load;

    for(auto& val : plugins.as<sol::table>()){
        if(!val.second.is<std::string>()){
            return Result<void>::Err(
                "Invalid value type for system name. It must be a string"
            );
        }
        std::string system = val.second.as<std::string>();
        plugins_lib_to_load.push_back(system);
    }

    std::string path = p_engine_path + plugins_lib_path;
    auto res = p_ctx.system_manager.load_systems_from_directory(path, plugins_lib_to_load);

    if(!res.ok()){
        return Result<void>::Err(
            "Could not load plugin systems correctly:\n\t{}", res.what()
        );
    }


    return Result<void>::Ok();
}

Result<void> engine::load_project(){
    sol::table config_tb = p_ctx.get_lua().view()["configs"];
    if(config_tb.valid()){
        if(config_tb["entry_point"].valid()){
            sol::object entry_point_obj = config_tb["entry_point"];
            p_entry_point = entry_point_obj.as<const char*>();
        }

        if(config_tb["target_fps"].valid()){
            int target_fps = config_tb["target_fps"].get<int>();
            p_ctx.time_manager.set_target_fps(target_fps);
        }
    }


    auto res_ord = p_ctx.system_manager.resolve_system_order();
    if(!res_ord.ok()){
        return Result<void>::Err(
            "Could not order systems:\n\t{}", res_ord.what()
        );
    }

    for(auto* s : res_ord.value()){
        LOG_INFO("System '{}' loaded successfully with priority:\n\t{}", s->get_name(), s->get_priority());
    }

    return Result<void>::Ok();
}

Result<void> engine::run(){
    auto view = p_ctx.get_lua().view();

    auto res_init = p_ctx.system_manager.initialize_systems_in_order(&p_ctx);

    if(!res_init.ok()){
        return Result<void>::Err(
            "Failed to initialize systems:\n\t{}", res_init.what()
        );
    }

    auto res_load = p_ctx.get_lua().load_script_file(p_project_path + "/" + p_entry_point);
    if(!res_load.ok()){
        return Result<void>::Err(
            "Failed to load entry point file at '{}':\n\t{}", p_entry_point, res_load.what()
        );
    }

    p_running = true;

    p_ctx.get_lua().safe_call("on_start");

    while(p_running){

        p_ctx.time_manager.begin_frame();

        auto res_begin_loop = p_ctx.system_manager.run_systems_begin_loop(&p_ctx);

        if(!res_begin_loop.ok()){
            return Result<void>::Err(
                "Failed to at begin loop for systems:\n\t{}", res_begin_loop.what()
            );
        }

        p_ctx.get_lua().safe_call("on_update", p_ctx.time_manager.get_delta_time());
        p_ctx.get_lua().safe_call("on_draw");

        MemoryManager::get_instance().reset_frame();

        auto res_end_loop = p_ctx.system_manager.run_systems_end_loop(&p_ctx);

        if(!res_end_loop.ok()){
            return Result<void>::Err(
                "Failed to at end loop for systems:\n\t{}", res_end_loop.what()
            );
        }
        
        p_ctx.time_manager.end_frame();

        p_ctx.frame_count++;
    }

    p_ctx.get_lua().safe_call("on_close");

    auto res_shut = p_ctx.system_manager.shutdown_systems_in_reverse_order(&p_ctx);

    if(!res_shut.ok()){
        return Result<void>::Err(
            "Failed to at shutdown for systems:\n\t{}", res_shut.what()
        );
    }

    return Result<void>::Ok();
}

EngineContext& engine::get_ctx()
{
    return p_ctx;
}
