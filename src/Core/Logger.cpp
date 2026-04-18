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
            char* appData;
            size_t len;
            _dupenv_s(&appData, &len, "APPDATA");
            
            if (appData) {
                std::string logPath = std::string(appData) + "\\TheBadPlace\\BadPlace.log";
                freopen_s(&fOut, logPath.c_str(), "a", stdout);
                freopen_s(&fErr, logPath.c_str(), "a", stderr);
                free(appData);
                
                // Set unbuffered for real-time logging to file
                if (fOut) setvbuf(fOut, NULL, _IONBF, 0);
                if (fErr) setvbuf(fErr, NULL, _IONBF, 0);
            }

            Log("Logger initialized (File).");
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
        }
    }
}
