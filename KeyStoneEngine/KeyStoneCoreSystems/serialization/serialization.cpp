#include "serialization.h"

#include "../old/jsonwrap.h"

json serialize_to_json(EngineContext* ctx, const sol::object& obj);
sol::object deserialize_from_json(EngineContext* ctx, const json& data);

Result<void> SerializationSystem::on_configs_load(EngineContext* ctx){
    return Result<void>::Ok();
}

Result<void> SerializationSystem::on_lua_register(EngineContext *ctx)
{
    auto view = ctx->get_lua().view();

    ctx->get_lua().register_type<json>("json", 
        sol::constructors<json>(),
        "save_to_file", [](const json& self, const std::string& file_path){
            std::ofstream file(file_path);
            file << self.dump();
            file.close();
        },
        "load_from_file", [](json& self, const std::string& file_path){
            std::ifstream file(file_path);
            std::string data;
            while (std::getline (file, data));
            self = json::parse(data);;
            file.close();
        }
    );

    sol::table serializer_tb = view.create_named_table("serializer");

    serializer_tb.set_function("to_json", [&ctx](const sol::object& obj) -> json {
        return serialize_to_json(ctx, obj);
    });

    serializer_tb.set_function("from_json", [&ctx](const json& data) -> sol::object {
        return deserialize_from_json(ctx, data);
    });

    serializer_tb.set_function("load_json_from_file", [](const std::string&file_path) -> json {
        std::ifstream file(file_path);
        std::string data;
        while (std::getline (file, data));
        json j = json::parse(data);;
        file.close();
        return j;
    });

    serializer_tb.set_function("save_json_to_file", [](const json& data, const std::string&file_path) {
        std::ofstream file(file_path);
        file << data.dump();
        file.close();
    });

    return Result<void>::Ok();
}

Result<void> SerializationSystem::on_start(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> SerializationSystem::on_end(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> SerializationSystem::on_begin_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
}

Result<void> SerializationSystem::on_end_loop(EngineContext *ctx)
{
    return Result<void>::Ok();
}

json serialize_to_json(EngineContext * ctx, const sol::object& obj){
    if (!obj.valid()) {
        return nullptr;
    }

    switch (obj.get_type()) {
        case sol::type::string:
            return obj.as<std::string>();
        case sol::type::number:
            if (obj.is<int>()) {
                return obj.as<int>();
            } else {
                return obj.as<double>();
            }
        case sol::type::boolean:
            return obj.as<bool>();
        case sol::type::table: {
            sol::table t = obj.as<sol::table>();
            json j = json::object();
            for (const auto& pair : t) {
                if (pair.first.is<std::string>()) {
                    j[pair.first.as<std::string>()] = serialize_to_json(ctx, pair.second);
                }
            }
            return j;
        }
        case sol::type::userdata: {
            std::string type_name = ctx->get_lua().get_type_name(obj);

            if (type_name.empty()) {
                return nullptr;
            }

            sol::table type_table = ctx->get_lua().get_registered_type(type_name);
            if(type_table.valid() && type_table["to_json"].valid()){
                sol::function to_json = type_table["to_json"];
                sol::object res = to_json(obj);
                if (res.is<json>()) {
                    json j = res.as<json>();
                    j["__type"] = type_name;
                    return j;
                } else {
                    LOG_WARN("to_json function for type '{}' did not return a valid json object", type_name);
                    return nullptr;
                }
            }else {
                LOG_WARN("No serialization support for type: {}", type_name);
                return nullptr;
            }
        }
        default:
            return nullptr;
    }
}

sol::object deserialize_from_json(EngineContext* ctx, const json& data){

    auto res = ctx->get_lua().view();
    auto& view = res.value();
    
    if (data.is_null()) {
        return sol::make_object(view, sol::nil);
    }
    else if (data.is_boolean()) {
        return sol::make_object(view, data.get<bool>());
    }
    else if (data.is_number_integer()) {
        return sol::make_object(view, data.get<int>());
    }
    else if (data.is_number_float()) {
        return sol::make_object(view, data.get<double>());
    }
    else if (data.is_string()) {
        return sol::make_object(view, data.get<std::string>());
    }
    else if (data.is_array()) {
        sol::table array = view.create_table();
        size_t index = 1;
        for (const auto& item : data) {
            array[index++] = deserialize_from_json(ctx, item);
        }
        return array;
    }
    else if (data.is_object()) {
        if (data.contains("__type")) {
            std::string type_name = data["__type"];
            sol::table type_table = ctx->get_lua().get_registered_type(type_name);

            if(type_table.valid() && type_table["from_json"].valid()){
                sol::function from_json = type_table["from_json"];
                return from_json(data);
            } else {
                LOG_WARN("No deserialization support for type: {}. Treating as table.", type_name);
            }
        }
        sol::table obj = view.create_table();
        for (auto& [key, value] : data.items()) {
            obj[key] = deserialize_from_json(ctx, value);
        }
        return obj;
    }

    return sol::make_object(view, sol::nil);
}