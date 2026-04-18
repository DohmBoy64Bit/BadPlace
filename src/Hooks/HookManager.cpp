#include "Hooks/HookManager.hpp"
#include "Luau/LuauAPI.hpp"
#include "Execution/ExecutionEngine.hpp"
#include "Core/Logger.hpp"
#include "../../thirdparty/minhook/include/MinHook.h"
#include <cstring>
#include <atomic>

namespace PolyHook {
    namespace Hooks {

        std::atomic<lua_State*> g_capturedState{ nullptr };
        bool g_hasCaptured = false;

        // Hook for lua_getfield to capture state and run tasks on the game tick
        int hooked_lua_getfield(lua_State* L, int idx, const char* k) {
            if (k && (strcmp(k, "_update") == 0 || strcmp(k, "_fixed_update") == 0)) {
                // We're inside the Godot logic loop ticking the Lua state!
                if (!g_hasCaptured) {
                    g_capturedState.store(L);
                    g_hasCaptured = true;
                    Logger::LogF("Captured active Lua state pointer: %p via %s", L, k);
                }

                // If this is the main state that was captured
                if (L == g_capturedState.load()) {
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

            return true;
        }

        void Cleanup() {
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            Logger::Log("Hooks disabled and MinHook uninitialized.");
        }
    }
}
