#pragma once 

#include <string>

/*

#include "enginectx.h"

#define DEBUG

class engine {
public:

    ~engine();

    Result<void> init();
    void stop();

    void cleanup();

    Result<void> set_engine_path(const std::string& path);
    Result<void> set_project_path(const std::string& path);

    Result<void> load_configs();
    Result<void> load_systems();
    Result<void> load_project();
    Result<void> run();

    EngineContext& get_ctx();
private:
    std::string                         p_engine_path;
    std::string                         p_project_path;
    const char *                        p_entry_point = "./data/scripts/main.lua";
    bool                                p_running = false;
    EngineContext                       p_ctx;
};

*/