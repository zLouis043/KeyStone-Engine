#pragma once 

#include "keystone.h"

class WindowSystem : public ISystem {
public:
    std::string get_name() override {
        return "window";
    }

    std::vector<std::string> get_deps() override {
        return {};
    }

    Result<void> on_configs_load(EngineContext* ctx) override;
    Result<void> on_lua_register(EngineContext* ctx) override;
    Result<void> on_start(EngineContext* ctx) override;
    Result<void> on_end(EngineContext* ctx) override;
    Result<void> on_begin_loop(EngineContext* ctx) override;
    Result<void> on_end_loop(EngineContext* ctx) override;
private:
    int width;
    int height;
    std::string title;
};

EXPORT_SYSTEM(WindowSystem);