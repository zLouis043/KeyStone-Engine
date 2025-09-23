#include "window.h"

#include "../old/raywrap.h"

void set_window_width(float width){
    SetWindowSize((int)width, GetScreenHeight());
}

void set_window_height(float height){
    SetWindowSize(GetScreenWidth(), (int)height);
}

Result<void> WindowSystem::on_configs_load(EngineContext* ctx)
{
    sol::table configs = ctx->get_lua().view()["configs"];

    if(configs["window"].valid()){
        sol::table win_cfgs = configs["window"];
        width = win_cfgs.get_or("width", 800);
        height = win_cfgs.get_or("height", 600);
        title = win_cfgs.get_or("title", "KeyStone App");
    }

    return Result<void>::Ok();
}

Result<void> WindowSystem::on_lua_register(EngineContext *ctx)
{   
    auto view = ctx->get_lua().view();

    sol::table window_tb = view.create_named_table("window");
    window_tb.set_function("clear_background", &ClearBackground);
    window_tb.set_function("get_width", &GetScreenWidth);
    window_tb.set_function("get_height", &GetScreenHeight);
    window_tb.set_function("set_title", &SetWindowTitle);
    window_tb.set_function("set_width", &set_window_width);
    window_tb.set_function("set_height", &set_window_height);
    window_tb.set_function("set_size", &SetWindowSize);

    return Result<void>::Ok();
}

Result<void> WindowSystem::on_start(EngineContext *ctx)
{
    InitWindow(width, height, title.c_str());
    LOG_INFO("Window Successfully initialized!");
    return Result<void>::Ok();
}

Result<void> WindowSystem::on_end(EngineContext *ctx)
{   
    CloseWindow();
    LOG_INFO("Window Successfully closed!");
    return Result<void>::Ok();
}

Result<void> WindowSystem::on_begin_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> WindowSystem::on_end_loop(EngineContext *ctx)
{   
    if(WindowShouldClose()){
        auto& em = ctx->get_event_manager();
        em.publish_event<void*>("close", nullptr);
    }
    return Result<void>::Ok();
}
