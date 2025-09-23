#include "filesystem.h"

bool file_exists(const char* f){
    return std::filesystem::exists(f);
}

const char* get_curr_dir_path(){
    return std::filesystem::current_path().generic_string().c_str();
}

Result<void> FileSystem::on_configs_load(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> FileSystem::on_lua_register(EngineContext *ctx)
{

    auto view = ctx->get_lua().view();

    sol::table file_sys_tb = view.create_named_table("filesystem");
    file_sys_tb.set_function("get_working_path", &get_curr_dir_path);
    file_sys_tb.set_function("file_exists", &file_exists);

    return Result<void>::Ok();
}

Result<void> FileSystem::on_start(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> FileSystem::on_end(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> FileSystem::on_begin_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> FileSystem::on_end_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
}
