#include "Hooks/HookManager.hpp"
#include "Luau/LuauAPI.hpp"
#include "Execution/ExecutionEngine.hpp"
#include "Execution/EnvironmentManager.hpp"
#include "Core/Logger.hpp"
#include "../../thirdparty/minhook/include/MinHook.h"
#include <cstring>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <shlobj.h>

namespace BadPlace {
    namespace Hooks {

        std::atomic<lua_State*> g_capturedState{ nullptr };
        bool g_hasCaptured = false;

        static std::string SanitizeChunkName(const std::string& name) {
            std::string s = name;
            for (char& c : s) { if (c == ':' || c == '*' || c == '?' || c == '<' || c == '>' || c == '|' || c == '\\' || c == '/') c = '_'; }
            return s.empty() ? "Unnamed" : s;
        }

        // GameID for bytecode cache hierarchy - set by EnvironmentManager
        static std::string g_currentGameId;

        void SetCurrentGameId(const std::string& gameId) {
            g_currentGameId = gameId;
        }

        // Hook for luau_load to intercept and cache incoming bytecode.
        int hooked_luau_load(lua_State* L, const char* chunkname, const char* data, size_t size, int env) {
            if (chunkname && data && size > 0) {
                char path[MAX_PATH];
                if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
                    // Use hierarchical path: BytecodeCache\{GameID}\{chunkname}.bin
                    std::filesystem::path cacheRoot = std::filesystem::path(path) / "TheBadPlace" / "BytecodeCache";
                    if (!g_currentGameId.empty()) {
                        cacheRoot = cacheRoot / g_currentGameId;
                    }
                    try {
                        if (!std::filesystem::exists(cacheRoot)) std::filesystem::create_directories(cacheRoot);
                        
                        // We use the chunkname (usually the script path/name) to key the cache
                        std::string safeName = SanitizeChunkName(chunkname);
                        std::filesystem::path binFile = cacheRoot / (safeName + ".bin");
                        
                        std::ofstream file(binFile, std::ios::binary | std::ios::trunc);
                        if (file.is_open()) {
                            file.write(data, size);
                            file.close();
                            // IPC::PipeServer::Get().PushLog("Bytecode Extractor | Cached -> " + safeName);
                        }
                    } catch (...) {}
                }
            }
            return original_luau_load(L, chunkname, data, size, env);
        }

        // Hook for lua_getfield to capture state and run tasks on the game tick
        int hooked_lua_getfield(lua_State* L, int idx, const char* k) {
            if (k && (strcmp(k, "_update") == 0 || strcmp(k, "_fixed_update") == 0)) {
                // We're inside the Godot logic loop ticking the Lua state!
                if (!g_hasCaptured) {
                    g_capturedState.store(L);
                    g_hasCaptured = true;
                    Logger::LogF("Captured active Lua state pointer: %p via %s", L, k);
                    Execution::ExecutionEngine::Get().RunAutoExec();
                }

                // If this is the main state that was captured
                if (L == g_capturedState.load()) {
                    // Initialize Log hooks dynamically into the global table
                    Execution::EnvironmentManager::Get().InitializeOverrides(L);
                    // Update global functions map securely
                    Execution::EnvironmentManager::Get().CacheGlobalFunctions(L);

                    // Pre-pcall hook execution queue (safely drain pending scripts)
                    Execution::ExecutionEngine::Get().ExecuteQueue(L);
                }
            }
            return original_lua_getfield(L, idx, k);
        }

        template<typename T>
        bool InstallHook(const char* name, LPVOID target, LPVOID detour, T** original) {
            if (MH_CreateHook(target, detour, reinterpret_cast<LPVOID*>(original)) != MH_OK) {
                Logger::LogF("Failed to create hook for %s", name);
                return false;
            }
            if (MH_EnableHook(target) != MH_OK) {
                Logger::LogF("Failed to enable hook for %s", name);
                return false;
            }
            Logger::LogF("Hook installed for %s", name);
            return true;
        }

        bool Initialize() {
            Logger::Log("Initializing MinHook...");
            if (MH_Initialize() != MH_OK) {
                Logger::Log("MinHook initialization failed!");
                return false;
            }

            if (original_lua_getfield) {
                InstallHook("lua_getfield", (LPVOID)original_lua_getfield, (LPVOID)hooked_lua_getfield, &original_lua_getfield);
            } else {
                Logger::Log("Cannot hook lua_getfield, pointer is null!");
                return false;
            }

            if (original_luau_load) {
                InstallHook("luau_load", (LPVOID)original_luau_load, (LPVOID)hooked_luau_load, &original_luau_load);
            } else {
                Logger::Log("Cannot hook luau_load, pointer is null!");
            }

            return true;
        }

        void Cleanup() {
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            Logger::Log("Hooks disabled and MinHook uninitialized.");
        }
    }
}
