#pragma once 

#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "defines.h"
#include "logger.h"

class KSEXPORT TypeManager {
public:
    void init(lua_State* state){
        lua_state = state;
    }

public:

    template<typename T, typename... Args>
    void register_type(const std::string& type_name, Args&&... args){
        sol::state_view lua_view(lua_state);
        lua_view.new_usertype<T>(type_name, std::forward<Args>(args)...);

        sol::table type_tb = lua_view[type_name];
        type_tb["get_type_name"] = [type_name]() -> std::string {
            return type_name;
        };
    };

    sol::table get_registered_type(const std::string& type_name) {
        sol::state_view lua_view(lua_state);
        
        if(lua_view[type_name].valid()){
            sol::table type_tb = lua_view[type_name];
            return type_tb;
        }

        return sol::nil;
    }

    std::string get_type_name(const sol::object& obj) const {
        if (obj.get_type() != sol::type::userdata) {
            return "";
        }
        
        sol::table obj_table = obj.as<sol::table>();

        if(obj_table.valid() && obj_table["get_type_name"].valid()){
            sol::object obj = obj_table["get_type_name"]();
            return obj.as<std::string>();
        }
        
        return "";
    }

private:
    lua_State* lua_state;
};