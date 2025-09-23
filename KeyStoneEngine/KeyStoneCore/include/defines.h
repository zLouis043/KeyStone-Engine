#pragma once

#ifdef _WIN32
    #define KSEXPORT __declspec(dllexport)
    #define KSCALL __cdecl
#else
    #define KSEXPORT __attribute__((visibility("default")))
    #define KSCALL
#endif