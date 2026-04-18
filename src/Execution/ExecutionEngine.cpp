#include "Execution/ExecutionEngine.hpp"
#include "Luau/LuauAPI.hpp"
#include "Core/Logger.hpp"
#include "IPC/PipeServer.hpp"
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

        // Helper: logs an error from a thread and releases its registry ref
        static void HandleCoroutineError(lua_State* L, lua_State* thread, int ref, int status) {
            const char* errMsg = original_lua_tolstring ? original_lua_tolstring(thread, -1, nullptr) : nullptr;
            if (errMsg) {
                Logger::LogF("Execution error: %s", errMsg);
                IPC::PipeServer::Get().PushLog(std::string("BadPlace | Lua Error: ") + errMsg);
            } else {
                Logger::LogF("Execution failed (code %d, no message)", status);
            }
            if (original_luaL_unref) original_luaL_unref(L, LUA_REGISTRYINDEX, ref);
        }

        void ExecutionEngine::ExecuteQueue(lua_State* L) {
            if (!original_lua_newthread || !original_lua_xmove || !original_lua_resume) {
                // Fallback path — no coroutine support
                Logger::Log("Coroutine APIs not resolved, falling back to lua_pcall.");
                std::lock_guard<std::mutex> lock(m_queueMutex);
                while (!m_scriptQueue.empty()) {
                    std::string script = m_scriptQueue.front(); m_scriptQueue.pop();
                    if (CompileAndLoad(L, script) && original_lua_pcall) {
                        int status = original_lua_pcall(L, 0, 0, 0);
                        if (status != 0) {
                            const char* e = original_lua_tolstring ? original_lua_tolstring(L, -1, nullptr) : nullptr;
                            if (e) IPC::PipeServer::Get().PushLog(std::string("BadPlace | Lua Error: ") + e);
                            if (original_lua_settop) original_lua_settop(L, 0);
                        }
                    }
                }
                return;
            }

            // --- Launch new scripts as coroutines ---
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                while (!m_scriptQueue.empty()) {
                    std::string script = m_scriptQueue.front();
                    m_scriptQueue.pop();

                    if (!CompileAndLoad(L, script)) continue;
                    // L stack: [func]

                    lua_State* thread = original_lua_newthread(L);
                    // L stack: [func, thread_obj]

                    // Copy func (at absolute index 1) to thread via xmove
                    if (original_lua_pushvalue) original_lua_pushvalue(L, 1);
                    // L stack: [func, thread_obj, func_copy]
                    original_lua_xmove(L, thread, 1);
                    // L stack: [func, thread_obj], thread has func_copy ready

                    // Pin the thread_obj in the registry so GC won't collect it
                    // luaL_ref pops from top (thread_obj) → returns ref key
                    int ref = original_luaL_ref ? original_luaL_ref(L, LUA_REGISTRYINDEX) : -1;
                    // L stack: [func]
                    if (original_lua_settop) original_lua_settop(L, 0);
                    // L stack: []

                    int status = original_lua_resume(thread, L, 0);
                    if (status == 0) {
                        Logger::Log("Execution successful.");
                        if (original_luaL_unref && ref != -1) original_luaL_unref(L, LUA_REGISTRYINDEX, ref);
                    } else if (status == 1) {
                        // LUA_YIELD — coroutine suspended, schedule for next tick
                        Logger::Log("Coroutine yielded, scheduling for resumption.");
                        m_pendingCoroutines.push({ref, thread});
                    } else {
                        HandleCoroutineError(L, thread, ref, status);
                    }
                }
            }

            // --- Resume any previously yielded coroutines ---
            int pendingCount = (int)m_pendingCoroutines.size();
            for (int i = 0; i < pendingCount; i++) {
                PendingCoroutine co = m_pendingCoroutines.front();
                m_pendingCoroutines.pop();

                int status = original_lua_resume(co.thread, L, 0);
                if (status == 0) {
                    Logger::Log("Pending coroutine completed.");
                    if (original_luaL_unref && co.registryRef != -1)
                        original_luaL_unref(L, LUA_REGISTRYINDEX, co.registryRef);
                } else if (status == 1) {
                    // Still yielding — requeue for next tick
                    m_pendingCoroutines.push(co);
                } else {
                    HandleCoroutineError(L, co.thread, co.registryRef, status);
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
                // luau_load leaves error message on stack
                const char* errMsg = nullptr;
                if (original_lua_tolstring) errMsg = original_lua_tolstring(L, -1, nullptr);
                if (errMsg) {
                    Logger::LogF("Compile/Load error: %s", errMsg);
                    IPC::PipeServer::Get().PushLog(std::string("BadPlace | Compile Error: ") + errMsg);
                }
                if (original_lua_settop) original_lua_settop(L, 0);
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
