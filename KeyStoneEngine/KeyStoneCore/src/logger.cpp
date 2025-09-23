#include "logger.h"

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
#endif

class PlatformUtils {
public:
    static bool isTerminalOutput(std::ostream& stream) {
        #ifdef _WIN32
            if (&stream == &std::cout) {
                return _isatty(_fileno(stdout));
            } else if (&stream == &std::cerr) {
                return _isatty(_fileno(stderr));
            }
            return false;
        #else
            if (&stream == &std::cout) {
                return isatty(STDOUT_FILENO);
            } else if (&stream == &std::cerr) {
                return isatty(STDERR_FILENO);
            }
            return false;
        #endif
    }
};

std::unique_ptr<PlatformUtils> ColorManager::platformUtils = nullptr;

PlatformUtils& ColorManager::getPlatformUtils() {
    if (!platformUtils) {
        platformUtils = std::make_unique<PlatformUtils>();
    }
    return *platformUtils;
}

bool ColorManager::supportsColor(std::ostream& stream) {
    return getPlatformUtils().isTerminalOutput(stream);
}

Logger globalLogger;