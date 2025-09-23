#pragma once 

#include <sol/sol.hpp>
#include <string>
#include "defines.h"
#include "typemanager.h"
#include "result.h"

class KSEXPORT LuaState {
public:
    Result<void> init();
    void cleanup();

    sol::state_view view();
    lua_State*      state() { return lua_state; }

    sol::table globals() { 
        sol::state_view view(lua_state); 
        return view.globals(); 
    }

    Result<sol::table> create_table(const std::string& name = "");
    Result<sol::object> load_script(const std::string& script_code);
    Result<sol::object> load_script_file(const std::string& file_path);

    template<typename T, typename... Args>
    void register_type(const std::string& type_name, Args&&... args){
        type_manager.register_type<T>(type_name, std::forward(args)...);
    };

    std::string get_type_name(const sol::object& obj){
        return type_manager.get_type_name(obj);
    }

    sol::table get_registered_type(const std::string& type_name){
        return type_manager.get_registered_type(type_name);
    }

    lua_State* operator&() {
        return lua_state;
    }

    template<class... Args>
    Result<sol::object> safe_call(sol::function func, Args&&... args){
        sol::protected_function_result res = func.call(std::forward<Args>(args)...);
        if(res.status() != sol::call_status::ok){
            return Result<sol::object>::Err(
                "Error when func called"
            );
        }
        return Result<sol::object>::Ok(res);
    }

    template<class... Args>
    Result<sol::object> safe_call(const std::string& func_name, Args&&... args){
        sol::protected_function func = view()[func_name];

        if(!func.valid()){
            return Result<sol::object>::Err(
                "Error when func '{}' was called: Func '{}' does not exist", func_name, func_name
            );
        }

        sol::protected_function_result func_res = func.call(std::forward<Args>(args)...);
        if(func_res.status() != sol::call_status::ok){
            return Result<sol::object>::Err(
                "Error when func '{}' was called", func_name
            );
        }
        return Result<sol::object>::Ok(func_res);
    }

private:
    lua_State * lua_state;
    TypeManager type_manager;
};