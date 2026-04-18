#pragma once
#include <string>
#include <queue>
#include <mutex>

typedef struct lua_State lua_State;

namespace BadPlace {
    namespace Execution {
        class EnvironmentManager {
        private:
            std::queue<std::string> m_logQueue;
            std::mutex m_logMutex;
            
            std::string m_cachedFunctions;
            bool m_isInitialized = false;

            EnvironmentManager() = default;

        public:
            static EnvironmentManager& Get() {
                static EnvironmentManager instance;
                return instance;
            }

            void InitializeOverrides(lua_State* L);
            
            void CacheGlobalFunctions(lua_State* L);

            const char* GetFunctionsJSON();

            void PushLog(const std::string& type, const std::string& message);
            const char* PollLog();
        };

        // C-function hooked overrides
        int HookedPrint(lua_State* L);
        int HookedWarn(lua_State* L);
        int HookedError(lua_State* L);
    }
}
