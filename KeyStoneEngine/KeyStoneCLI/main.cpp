#include <iostream>
#include <keystone.hpp>

int main(int argc, char** argv){
    ks_memory_init();

    ks_memory_shutdown();
    return 0;
}
