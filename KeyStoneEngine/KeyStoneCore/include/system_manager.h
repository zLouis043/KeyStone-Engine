#pragma once

#include "system.h"
#include "result.h"
#include "memory.h"

#include <unordered_map>
#include <string>
#include <algorithm>
#include <format>
#include <iostream>
#include <filesystem>
#include <set>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

class SystemManager {
    using SystemFactory = ke::shared_ptr<ISystem>(KSCALL*)();

    struct LoadedLibrary {
    #if defined(_WIN32)
        HMODULE handle;
    #else
        void* handle;
    #endif
        std::string path;
    };

public:
    Result<void> load_system(const std::string& library_path, const std::string& system_name) {
        if (systems_.find(system_name) != systems_.end()) {
            return Result<void>::Ok();
        }

    #if defined(_WIN32)
        HMODULE module = LoadLibraryA(library_path.c_str());
        if (!module) {
            return Result<void>::Err(
                "Unable load library: '{}'", std::filesystem::absolute(library_path).string()
            );
        }
    #else
        void* module = dlopen(library_path.c_str(), RTLD_LAZY);
        if (!module) {
            return Result<void>::Err(
                "Unable load library: '{}'", std::filesystem::absolute(library_path).string()
            );
        }
    #endif

    #if defined(_WIN32)
        SystemFactory factory_func = (SystemFactory)GetProcAddress(module, "load_system");
    #else
        SystemFactory factory_func = (SystemFactory)dlsym(module, "load_system");
    #endif

        if (!factory_func) {        
        #if defined(_WIN32)
            FreeLibrary(module);
        #else
            dlclose(module);
        #endif
            return Result<void>::Err(
                "Function 'load_system' not found in '{}' lib", std::filesystem::absolute(library_path).string()
            );
        }

        ke::shared_ptr<ISystem> system = factory_func();
        if (!system) {
        #if defined(_WIN32)
            FreeLibrary(module);
        #else
            dlclose(module);
        #endif
            return Result<void>::Err(
                "Unable to create system for library: '{}'", std::filesystem::absolute(library_path).string()
            );
        }

        systems_[system_name] = std::move(system);
        libraries_[system_name] = {module, library_path};
    
        return Result<void>::Ok();
    }

     Result<void> load_systems_from_directory(const std::string& directory_path, const std::vector<std::string>& systems) {
        for (const auto& system : systems) {
            std::string full_path = std::filesystem::absolute(directory_path + "/" + system + get_library_extension()).lexically_normal().string();
            auto result = load_system(full_path, system);
            if (!result) {
                return Result<void>::Err("Failed to load {}: {}", system, result.what());
            }
        }
        
        return Result<void>::Ok();
    }


    Result<ISystem*> get_system(const std::string& name) {
        auto it = systems_.find(name);
        if (it != systems_.end()) {
            return Result<ISystem*>::Ok(it->second.get());
        }
        return Result<ISystem*>::Err("System '{}' not found", name);
    }

    template<typename T>
    Result<T*> get_system_as(const std::string& name) {
        auto result = get_system(name);
        if (!result.ok()) {
            return Result<T*>::Err(result.what());
        }
        
        T* casted = dynamic_cast<T*>(result.value());
        if (!casted) {
            return Result<T*>::Err("System '{}' is not the same type as requested", name);
        }
        
        return Result<T*>::Ok(casted);
    }

    Result<void> unload_system(const std::string& name) {
        auto sys_it = systems_.find(name);
        auto lib_it = libraries_.find(name);
        
        if (sys_it == systems_.end() || lib_it == libraries_.end()) {
            return Result<void>::Err("System '{}' was not loaded", name);
        }

        systems_.erase(sys_it);

    #if defined(_WIN32)
        if (!FreeLibrary(lib_it->second.handle)) {
            return Result<void>::Err("Cannot free library '{}'", name);
        }
    #else
        if (dlclose(lib_it->second.handle) != 0) {
            return Result<void>::Err("Cannot free library '{}': {}", 
                                   name, dlerror());
        }
    #endif
        
        libraries_.erase(lib_it);
        
        return Result<void>::Ok();
    }

    Result<void> unload_all_systems() {
        std::vector<std::string> errors;
        
        for (auto it = systems_.begin(); it != systems_.end(); ) {
            std::string name = it->first;
            auto result = unload_system(name);
            if (!result) {
                errors.push_back(result.what());
            } else {
                it = systems_.erase(it);
            }
        }

        if (!errors.empty()) {
            std::string error_msg = "Errors during unload unload:\n";
            for (const auto& error : errors) {
                error_msg += "  - " + error + "\n";
            }
            return Result<void>::Err(error_msg);
        }
        
        return Result<void>::Ok();
    }

    std::vector<std::string> get_loaded_systems() const {
        std::vector<std::string> names;
        for (const auto& pair : systems_) {
            names.push_back(pair.first);
        }
        return names;
    }

    Result<const std::vector<ISystem*>> get_ordered_systems() {
        if (p_ordered_systems.empty()) {
            return Result<const std::vector<ISystem*>>::Err("No ordered system available");
        }
        return Result<const std::vector<ISystem*>>::Ok(p_ordered_systems);
    }

    std::vector<std::string> get_loaded_system_names() {
        std::vector<std::string> names;
        for (const auto& pair : systems_) {
            names.push_back(pair.first);
        }
        return names;
    }

    Result<std::vector<ISystem*>> resolve_system_order() {
        std::vector<ISystem*> systems_to_process;
        std::vector<std::string> loaded_system_names;
        
        for (auto& [name, system] : systems_) {
            systems_to_process.push_back(system.get());
        }

        std::vector<ISystem*> ordered_systems;
        ordered_systems.reserve(systems_to_process.size());

        int loaded_in_pass;
        do {
            loaded_in_pass = 0;
            ISystem* next_system = nullptr;
            int highest_priority = -1;
            size_t next_system_index = -1;

            for (size_t i = 0; i < systems_to_process.size(); ++i) {
                ISystem* current_system = systems_to_process[i];
                bool dependencies_met = true;

                for (const auto& dependency : current_system->get_deps()) {
                    bool found = std::any_of(loaded_system_names.begin(), 
                                           loaded_system_names.end(),
                                           [&](const std::string& loaded_name) {
                                               return loaded_name == dependency;
                                           });
                    if (!found) {
                        dependencies_met = false;
                        break;
                    }
                }
                

                if (dependencies_met && current_system->get_priority() > highest_priority) {
                    highest_priority = current_system->get_priority();
                    next_system = current_system;
                    next_system_index = i;
                }
            }
            
            if (next_system != nullptr) {
                loaded_system_names.push_back(next_system->get_name());
                ordered_systems.push_back(next_system);
                systems_to_process.erase(systems_to_process.begin() + next_system_index);
                loaded_in_pass = 1;
            }

        } while (!systems_to_process.empty() && loaded_in_pass > 0);
        if (!systems_to_process.empty()) {
            bool is_cyclic = true;
            
            for (ISystem* blocked_system : systems_to_process) {
                bool has_external_dependency = false;
                
                for (const auto& dep : blocked_system->get_deps()) {
                    bool dep_is_in_blocked_set = std::any_of(
                        systems_to_process.begin(), systems_to_process.end(),
                        [&](ISystem* s) { return s->get_name() == dep; });
                    
                    if (!dep_is_in_blocked_set) {
                        has_external_dependency = true;
                        break;
                    }
                }
                
                if (has_external_dependency) {
                    is_cyclic = false;
                    break;
                }
            }
            
            std::string error_msg;
            if (is_cyclic) {
                error_msg = "Found cyclic dependency for systems:\n";
            } else {
                error_msg = "Found deadlock or missing dependency for systems:\n";
            }

            for (ISystem* blocked_system : systems_to_process) {
                error_msg += format_string("  System: {} (priority: {})\n", 
                                       blocked_system->get_name(), 
                                       blocked_system->get_priority());
                
                for (const auto& dep : blocked_system->get_deps()) {
                    bool dep_is_loaded = std::any_of(
                        loaded_system_names.begin(), loaded_system_names.end(),
                        [&](const std::string& name) { return name == dep; });
                    
                    bool dep_is_in_blocked_set = std::any_of(
                        systems_to_process.begin(), systems_to_process.end(),
                        [&](ISystem* s) { return s->get_name() == dep; });
                    
                    if (dep_is_loaded) {
                        error_msg += format_string("    - depends on {} (✓ loaded)\n", dep);
                    } else if (dep_is_in_blocked_set) {
                        error_msg += format_string("    - depends on {} (✗ cyclic)\n", dep);
                    } else {
                        error_msg += format_string("    - depends on {} (✗ missing)\n", dep);
                    }
                }
            }
            
            return Result<std::vector<ISystem*>>::Err(error_msg);
        }

        p_ordered_systems = ordered_systems;
        return Result<std::vector<ISystem*>>::Ok(ordered_systems);
    }

    Result<void> initialize_systems_in_order(EngineContext* ctx) {
        auto order_result = resolve_system_order();
        if (!order_result) {
            return Result<void>::Err(order_result.what());
        }

        for (ISystem* system : p_ordered_systems) {
            auto result = system->on_configs_load(ctx);
            if (!result) {
                return Result<void>::Err("Error in on_configs_load for {}: {}", 
                                       system->get_name(), result.what());
            }
        }

        for (ISystem* system : p_ordered_systems) {
            auto result = system->on_lua_register(ctx);
            if (!result) {
                return Result<void>::Err("Error in on_configs_load for {}: {}", 
                                       system->get_name(), result.what());
            }
        }

        for (ISystem* system : p_ordered_systems) {
            auto result = system->on_start(ctx);
            if (!result) {
                return Result<void>::Err("Error in on_start for {}: {}", 
                                       system->get_name(), result.what());
            }
        }

        return Result<void>::Ok();
    }

    Result<void> run_systems_begin_loop(EngineContext* ctx) {
        for (ISystem* system : p_ordered_systems) {
            auto result = system->on_begin_loop(ctx);
            if (!result) {
                return Result<void>::Err("Error in on_begin_loop for {}: {}", 
                                       system->get_name(), result.what());
            }
        }
        return Result<void>::Ok();
    }

    Result<void> run_systems_end_loop(EngineContext* ctx) {
        for (auto it = p_ordered_systems.rbegin(); it != p_ordered_systems.rend(); ++it) {
            auto result = (*it)->on_end_loop(ctx);
            if (!result) {
                return Result<void>::Err("Error in on_end_loop for {}: {}", 
                                       (*it)->get_name(), result.what());
            }
        }
        return Result<void>::Ok();
    }

    Result<void> shutdown_systems_in_reverse_order(EngineContext* ctx) {
        for (auto it = p_ordered_systems.rbegin(); it != p_ordered_systems.rend(); ++it) {
            auto result = (*it)->on_end(ctx);
            if (!result) {
                return Result<void>::Err("Error in on_end for {}: {}", 
                                       (*it)->get_name(), result.what());
            }
        }
        return Result<void>::Ok();
    }

private:
    std::string get_library_extension() const {
    #if defined(_WIN32)
        return ".dll";
    #elif defined(__APPLE__)
        return ".dylib";
    #else
        return ".so";
    #endif
    }

private:
    std::unordered_map<std::string, ke::shared_ptr<ISystem>> systems_;
    std::unordered_map<std::string, LoadedLibrary> libraries_;
    std::vector<ISystem*> p_ordered_systems;
};