#pragma once 

#include "keystone.h"

class DrawSystem : public ISystem {
public:
    std::string get_name() override {
        return "draw";
    }

    std::vector<std::string> get_deps() override {
        return {"window"};
    }

    Result<void> on_configs_load(EngineContext* ctx) override;
    Result<void> on_lua_register(EngineContext* ctx) override;
    Result<void> on_start(EngineContext* ctx) override;
    Result<void> on_end(EngineContext* ctx) override;
    Result<void> on_begin_loop(EngineContext* ctx) override;
    Result<void> on_end_loop(EngineContext* ctx) override;
};

EXPORT_SYSTEM(DrawSystem);