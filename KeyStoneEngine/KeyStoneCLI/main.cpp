#include <iostream>
#include <keystone.hpp>

int main(int argc, char** argv){

    ks_memory_init();

    int* ptr = ks::mem::alloc_t<int, 5>(ks::mem::Lifetime::USER_MANAGED, ks::mem::Tag::GARBAGE);

    ptr[0] = 5;

    ks::log::info("ptr = {}", ptr[0]);

    ks::mem::dealloc(ptr);

    ks::log::info("ptr = {}", ptr[0]);

    ks::script::ScriptManager sm;
    sm.init();

    sm.script(
        R"(print("Hello World from script"))"
    );

    sm.shutdown();

    ks_memory_shutdown();

    return 0;

    /*
    if(argc < 2){
        LOG_FATAL("Project path was not given to the engine.\n\tUsage: ./keystone-cli <project_path>");
        return 1;
    }

    engine e;

    Result<void> res = e.init();

    if(!res.ok()){
        LOG_FATAL("Could not init the engine:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.set_engine_path(argv[0]);

    if(!res.ok()){
        LOG_FATAL("Could not set the engine path:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.set_project_path(argv[1]);

    if(!res.ok()){
        LOG_FATAL("Could not set the project path:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.load_configs();

    if(!res.ok()){
        LOG_FATAL("Could not load configs:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.load_systems();

    if(!res.ok()){
        LOG_FATAL("Could not load systems:\n\t{}", 
            res.what());
        return 1;
    }

    res = e.load_project();
    
    if(!res.ok()){
        LOG_FATAL("Could not load project:\n\t{}",
            res.what());
        return 1;
    } 

    res = e.run();

    if(!res.ok()){
        LOG_FATAL("Could not run the project:\n\t{}", res.what());
        return 1;
    }  
    */

    return 0;
}