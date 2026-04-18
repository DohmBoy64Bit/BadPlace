#pragma once
#include <string>

namespace BadPlace {
    namespace Logger {
        void Initialize();
        void Log(const std::string& message);
        void LogF(const char* format, ...);
        void Free();
    }
}
