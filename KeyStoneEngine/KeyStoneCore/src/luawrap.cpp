#include "luawrap.h"
#include "memory.h"

static void *lua_custom_Alloc(void *ud, void *ptr, size_t osize, size_t nsize){
    MemoryManager& mem_manager = MemoryManager::get_instance();

    if (nsize == 0) {
        if (ptr) {
            mem_manager.dealloc(ptr);
        }
        return nullptr;
    }else if(ptr == nullptr){
        return mem_manager.alloc(nsize, MemoryManager::USER_MANAGED, MemoryManager::SCRIPT, "LuaData");
    }

    return mem_manager.realloc(ptr, nsize);
}

Result<void> LuaState::init(){
    lua_state = lua_newstate(lua_custom_Alloc, nullptr);
    if(!lua_state) return Result<void>::Err("Cannot create lua state");

    sol::state_view view(lua_state);

    view.open_libraries(sol::lib::base, sol::lib::io, sol::lib::string, sol::lib::math);

    type_manager.init(lua_state);

    return Result<void>::Ok();
}

void LuaState::cleanup(){
    if (lua_state) {
        lua_close(lua_state);
        lua_state = nullptr;
    }
}

sol::state_view LuaState::view()
{   
    if(!lua_state){
        throw std::runtime_error("Could not get lua state view because LuaState was not initialized or cleanup was done before using LuaState");
    }

    sol::state_view view(lua_state);
    return view;
}

Result<sol::table> LuaState::create_table(const std::string &name)
{       
    sol::table tb = view().create_named_table(name);
    return Result<sol::table>::Ok(tb);
}

Result<sol::object> LuaState::load_script_file(const std::string &file_path)
{   
    sol::protected_function_result func_res =  view().safe_script_file(file_path);
    if(func_res.status() != sol::call_status::ok){
        return Result<sol::object>::Err(
            "Could not load script file '{}'", file_path
        );
    }
    return Result<sol::object>::Ok(func_res);
}

Result<sol::object> LuaState::load_script(const std::string &script_code)
{
    sol::protected_function_result func_res =  view().safe_script(script_code);
    if(func_res.status() != sol::call_status::ok){
        return Result<sol::object>::Err(
            "Could not load script code '{}'", script_code
        );
    }
    return Result<sol::object>::Ok(func_res);
}