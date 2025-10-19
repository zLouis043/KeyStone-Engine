#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define KS_PLATFORM_WINDOWS
#elif defined(__linux__)
    #define KS_PLATFORM_LINUX
#elif defined(__APPLE__)
    #define KS_PLATFORM_MACOS
#endif

// Export/Import macros
#if defined(KS_PLATFORM_WINDOWS)
    #ifdef KS_EXPORT
        #define KS_API __declspec(dllexport)
    #else
        #define KS_API __declspec(dllimport)
    #endif
#elif defined(KS_PLATFORM_LINUX) || defined(KS_PLATFORM_MACOS)
    #ifdef KS_EXPORT
        #define KS_API __attribute__((visibility("default")))
    #else
        #define KS_API
    #endif
#else
    #define KS_API
#endif
