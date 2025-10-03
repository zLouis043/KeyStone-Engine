#pragma 

#ifdef KS_EXPORT
    #define KS_API __declspec(dllexport)
#else
    #define KS_API __declspec(dllimport)
#endif