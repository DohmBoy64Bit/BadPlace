#include "Core/Logger.hpp"
#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <vector>

namespace BadPlace {
    namespace Logger {
        FILE* fOut = nullptr;
        FILE* fErr = nullptr;

        void Initialize() {
            AllocConsole();
            freopen_s(&fOut, "CONOUT$", "w", stdout);
            freopen_s(&fErr, "CONOUT$", "w", stderr);
            SetConsoleTitleA("BadPlace Console");
            Log("Logger initialized.");
        }

        void Log(const std::string& message) {
            printf("[BadPlace] %s\n", message.c_str());
        }

        void LogF(const char* format, ...) {
            va_list args;
            va_start(args, format);
            printf("[BadPlace] ");
            vprintf(format, args);
            printf("\n");
            va_end(args);
        }

        void Free() {
            if (fOut) fclose(fOut);
            if (fErr) fclose(fErr);
            FreeConsole();
        }
    }
}
