#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Core/Logger.hpp"
#include "Luau/LuauAPI.hpp"
#include "Hooks/HookManager.hpp"
#include "Execution/ExecutionEngine.hpp"
#include "Execution/EnvironmentManager.hpp"

// C-ABI Exports for P/Invoke / UI IPC
extern "C" {
    __declspec(dllexport) int BadPlace_ExecuteLua(const char* code) {
        if (!code) return -1;
        BadPlace::Execution::ExecutionEngine::Get().QueueScript(std::string(code));
        return 0;
    }

    __declspec(dllexport) const char* BadPlace_GetFunctions() {
        return BadPlace::Execution::EnvironmentManager::Get().GetFunctionsJSON();
    }

    __declspec(dllexport) const char* BadPlace_PollLogs() {
        return BadPlace::Execution::EnvironmentManager::Get().PollLog();
    }

}




BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(hModule);
        
        BadPlace::Logger::Initialize();
        BadPlace::Logger::Log("BadPlace DLL Injected successfully.");

        if (BadPlace::LuauAPI::Initialize()) {
            if (BadPlace::Hooks::Initialize()) {
                BadPlace::Logger::Log("All systems online.");
            }
        }

        break;
    }
    case DLL_PROCESS_DETACH: {
        BadPlace::Hooks::Cleanup();
        BadPlace::Logger::Free();
        break;
    }
    }
    return TRUE;
}
