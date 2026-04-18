#include "Execution/EnvironmentManager.hpp"
#include "Luau/LuauAPI.hpp"
#include "Core/Logger.hpp"

#define LUA_GLOBALSINDEX -10002
#define LUA_TFUNCTION 6

namespace BadPlace {
    namespace Execution {

        // --- Core Overrides logic ---
        static std::string ExtractLogArgs(lua_State* L) {
            std::string logLine = "";
            int top = original_lua_gettop(L);
            for (int i = 1; i <= top; i++) {
                size_t len = 0;
                const char* str = original_lua_tolstring(L, i, &len);
                if (str) {
                    if (i > 1) logLine += " ";
                    logLine += std::string(str, len);
                }
            }
            return logLine;
        }

        int HookedPrint(lua_State* L) {
            EnvironmentManager::Get().PushLog("PRINT", ExtractLogArgs(L));
            return 0; // We swallow it, or we could redirect it
        }

        int HookedWarn(lua_State* L) {
            EnvironmentManager::Get().PushLog("WARN", ExtractLogArgs(L));
            return 0;
        }

        int HookedError(lua_State* L) {
            EnvironmentManager::Get().PushLog("ERROR", ExtractLogArgs(L));
            return 0;
        }


        // --- Manager logic ---

        void EnvironmentManager::PushLog(const std::string& type, const std::string& message) {
            std::lock_guard<std::mutex> lock(m_logMutex);
            // Format: [TYPE] Message
            m_logQueue.push("[" + type + "] " + message);
        }

        const char* EnvironmentManager::PollLog() {
            std::lock_guard<std::mutex> lock(m_logMutex);
            if (m_logQueue.empty()) {
                return nullptr;
            }
            // Memory must be handled by caller or statically buffered if simple
            // For P/Invoke, we can return a dynamically allocated string, but caller must free it (hard with C#).
            // A safer approach: internal static buffer.
            static std::string currentLog;
            currentLog = m_logQueue.front();
            m_logQueue.pop();
            return currentLog.c_str();
        }

        void EnvironmentManager::InitializeOverrides(lua_State* L) {
            if (m_isInitialized) return;
            if (!original_lua_pushcclosurek || !original_lua_setfield) return;

            // push the hooked functions and set them in globals
            // lua_pushcfunction is typically mapped as lua_pushcclosurek(L, fn, name, 0, NULL)
            
            original_lua_pushcclosurek(L, HookedPrint, "print", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "print");

            original_lua_pushcclosurek(L, HookedWarn, "warn", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "warn");

            original_lua_pushcclosurek(L, HookedError, "error", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "error");

            Logger::Log("Luau environment log overrides injected.");
            m_isInitialized = true;
        }

        void EnvironmentManager::CacheGlobalFunctions(lua_State* L) {
            if (!original_lua_pushvalue || !original_lua_next || !original_lua_type || !original_lua_tolstring || !original_lua_settop || !original_lua_pushnil) {
                return;
            }

            std::string json = "[";
            bool first = true;

            // Push globals table
            original_lua_pushvalue(L, LUA_GLOBALSINDEX);
            
            // Push nil for initial lua_next key
            original_lua_pushnil(L);

            while (original_lua_next(L, -2) != 0) {
                // key is at -2, value is at -1
                if (original_lua_type(L, -1) == LUA_TFUNCTION) {
                    // It's a function. Let's get the key name.
                    // IMPORTANT: lua_tolstring on a key can break lua_next if it converts a number,
                    // but in _G most keys are strings anyway. For safety, we check type.
                    if (original_lua_type(L, -2) == 4) { // LUA_TSTRING
                        size_t len;
                        const char* key = original_lua_tolstring(L, -2, &len);
                        if (key) {
                            if (!first) {
                                json += ",";
                            }
                            json += "\"" + std::string(key, len) + "\"";
                            first = false;
                        }
                    }
                }
                // pop value, keep key for next iteration
                original_lua_settop(L, -2);
            }
            
            // pop globals table
            original_lua_settop(L, -2);

            json += "]";
            m_cachedFunctions = json;
        }

        const char* EnvironmentManager::GetFunctionsJSON() {
            if (m_cachedFunctions.empty()) return "[]";
            return m_cachedFunctions.c_str();
        }

    }
}
