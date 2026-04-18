#include "Execution/ExecutionEngine.hpp"
#include "Luau/LuauAPI.hpp"
#include "Core/Logger.hpp"
#include <filesystem>
#include <fstream>
#include <string>

namespace BadPlace {
    namespace Execution {

        void ExecutionEngine::QueueScript(const std::string& script) {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_scriptQueue.push(script);
            Logger::LogF("Queued script of size %zu", script.length());
        }

        void ExecutionEngine::ExecuteQueue(lua_State* L) {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            while (!m_scriptQueue.empty()) {
                std::string script = m_scriptQueue.front();
                m_scriptQueue.pop();

                if (CompileAndLoad(L, script)) {
                    // Script is loaded onto the Lua stack, now call it (0 args, 0 results)
                    if (original_lua_pcall) {
                        int status = original_lua_pcall(L, 0, 0, 0);
                        if (status != 0) {
                            Logger::LogF("Execution failed with status: %d", status);
                            // Stack now contains the error message, we could pop it or log it
                            if (original_lua_settop) {
                                original_lua_settop(L, -2); // pop error
                            }
                        } else {
                            Logger::Log("Execution successful.");
                        }
                    } else {
                        Logger::Log("Error: lua_pcall pointer is null.");
                    }
                }
            }
        }

        bool ExecutionEngine::CompileAndLoad(lua_State* L, const std::string& script) {
            if (!original_luau_compile || !original_luau_load) {
                Logger::Log("Compiler or Loader pointers missing!");
                return false;
            }

            size_t outSize = 0;
            // compile options = nullptr
            char* bytecode = original_luau_compile(script.data(), script.size(), nullptr, &outSize);
            
            if (!bytecode || outSize == 0) {
                Logger::Log("Compilation failed! Returned empty bytecode.");
                return false;
            }

            int loadStatus = original_luau_load(L, "BadPlace", bytecode, outSize, 0);
            
            // Standard Luau doesn't rigorously enforce us freeing memory immediately if we don't have the fn,
            // but we really should. Actually, luau_compile uses standard malloc, so we can just use standard free.
            free(bytecode);

            if (loadStatus != 0) {
                Logger::LogF("luau_load failed with status: %d", loadStatus);
                if (original_lua_settop) {
                    original_lua_settop(L, -2); // pop error
                }
                return false;
            }

            return true;
        }

        void ExecutionEngine::RunAutoExec() {
            char* appData = nullptr;
            size_t len = 0;
            _dupenv_s(&appData, &len, "APPDATA");
            
            if (!appData) {
                Logger::Log("AutoExec: Failed to find APPDATA environment variable.");
                return;
            }

            std::filesystem::path autoExecDir = std::filesystem::path(appData) / "TheBadPlace" / "AutoExec";
            free(appData);

            if (!std::filesystem::exists(autoExecDir) || !std::filesystem::is_directory(autoExecDir)) {
                Logger::Log("AutoExec: Directory does not exist.");
                return;
            }

            Logger::LogF("AutoExec: Scanning %s", autoExecDir.string().c_str());

            for (const auto& entry : std::filesystem::directory_iterator(autoExecDir)) {
                if (entry.is_regular_file()) {
                    auto path = entry.path();
                    auto ext = path.extension().string();
                    
                    if (ext == ".lua" || ext == ".txt") {
                        std::ifstream file(path);
                        if (file.is_open()) {
                            std::string script((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            QueueScript(script);
                            Logger::LogF("AutoExec: Queued %s", path.filename().string().c_str());
                        }
                    }
                }
            }
        }

    }
}
