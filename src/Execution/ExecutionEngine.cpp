#include "Execution/ExecutionEngine.hpp"
#include "Luau/LuauAPI.hpp"
#include "Core/Logger.hpp"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace BadPlace {
    namespace Execution {

        void ExecutionEngine::QueueScript(const std::string& script) {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_scriptQueue.push(script);
            Logger::LogF("Queued script of size %zu", script.length());
        }

        void ExecutionEngine::ExecuteQueue(lua_State* L) {
            // Process scripts
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                while (!m_scriptQueue.empty()) {
                    std::string script = m_scriptQueue.front();
                    m_scriptQueue.pop();

                    if (CompileAndLoad(L, script)) {
                        if (original_lua_pcall) {
                            int status = original_lua_pcall(L, 0, 0, 0);
                            if (status != 0) {
                                Logger::LogF("Execution failed: %d", status);
                                if (original_lua_settop) original_lua_settop(L, -2);
                            } else {
                                Logger::Log("Execution successful.");
                            }
                        }
                    }
                }
            }
        }


        bool ExecutionEngine::CompileAndLoad(lua_State* L, const std::string& script) {
            if (!original_luau_compile || !original_luau_load) return false;

            size_t outSize = 0;
            char* bytecode = original_luau_compile(script.data(), script.size(), nullptr, &outSize);
            
            if (!bytecode || outSize == 0) return false;

            int loadStatus = original_luau_load(L, "BadPlace", bytecode, outSize, 0);
            free(bytecode);

            if (loadStatus != 0) {
                if (original_lua_settop) original_lua_settop(L, -2);
                return false;
            }

            return true;
        }

        void ExecutionEngine::RunAutoExec() {
            char* appData = nullptr;
            size_t len = 0;
            _dupenv_s(&appData, &len, "APPDATA");
            if (!appData) return;

            std::filesystem::path autoExecDir = std::filesystem::path(appData) / "TheBadPlace" / "AutoExec";
            free(appData);

            if (!std::filesystem::exists(autoExecDir) || !std::filesystem::is_directory(autoExecDir)) return;

            for (const auto& entry : std::filesystem::directory_iterator(autoExecDir)) {
                if (entry.is_regular_file()) {
                    auto path = entry.path();
                    if (path.extension() == ".lua" || path.extension() == ".txt") {
                        std::ifstream file(path);
                        if (file.is_open()) {
                            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                            QueueScript(content);
                            Logger::LogF("AutoExec: %s", path.filename().string().c_str());
                        }
                    }
                }
            }
        }



    }
}
