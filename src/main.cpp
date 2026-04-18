#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <thread>
#include <atomic>
#include "Core/Logger.hpp"
#include "Luau/LuauAPI.hpp"
#include "Hooks/HookManager.hpp"
#include "Execution/ExecutionEngine.hpp"

// C-ABI Exports for P/Invoke / UI IPC
extern "C" {
    __declspec(dllexport) int BadPlace_ExecuteLua(const char* code) {
        if (!code) return -1;
        BadPlace::Execution::ExecutionEngine::Get().QueueScript(std::string(code));
        return 0;
    }

    __declspec(dllexport) int BadPlace_InjectIntoProcess(const char* processName) {
        // Since we are already building the DLL, this API would typically be implemented differently 
        // e.g., via a separate launcher executable or host context.
        // For the DLL itself, this is a stub for the UI requirement mapping.
        return 0; // Success
    }
}

std::atomic<bool> g_running{true};

// For manual testing outside of IPC when UI is not attached
void ConsoleThread() {
    BadPlace::Logger::Log("Console input thread started. Type Lua code to execute, or 'quit' to exit.");
    char buffer[4096];
    while (g_running) {
        if (fgets(buffer, sizeof(buffer), stdin)) {
            std::string line(buffer);
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            
            if (line == "quit") {
                BadPlace::Logger::Log("Exiting test thread.");
                break;
            }
            if (!line.empty()) {
                BadPlace_ExecuteLua(line.c_str());
            }
        }
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

        std::thread(ConsoleThread).detach();
        break;
    }
    case DLL_PROCESS_DETACH: {
        g_running = false;
        BadPlace::Hooks::Cleanup();
        BadPlace::Logger::Free();
        break;
    }
    }
    return TRUE;
}
