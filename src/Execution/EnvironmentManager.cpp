#include "Execution/EnvironmentManager.hpp"
#include "Luau/LuauAPI.hpp"
#include "Core/Logger.hpp"
#include "IPC/PipeServer.hpp"
#include "Execution/HttpManager.hpp"
#include "Execution/ExecutionEngine.hpp"
#include <filesystem>
#include <fstream>
#include <shlobj.h>

#include "Execution/ExecutionEngine.hpp"

namespace BadPlace {
    namespace Execution {

        static int s_callbackCounter = 1000;

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
            std::string logLine = ExtractLogArgs(L);
            EnvironmentManager::Get().PushLog("LUA", logLine);
            IPC::PipeServer::Get().PushLog(logLine);
            Logger::Log(("[LUA PRINT] " + logLine).c_str());
            return 0;
        }

        int HookedWarn(lua_State* L) {
            std::string logLine = ExtractLogArgs(L);
            std::string entry = "BadPlace | Warning: " + logLine;
            EnvironmentManager::Get().PushLog("WARN", logLine);
            IPC::PipeServer::Get().PushLog(entry);
            Logger::Log(("[LUA WARN]  " + logLine).c_str());
            return 0;
        }

        int HookedError(lua_State* L) {
            std::string logLine = ExtractLogArgs(L);
            std::string entry = "BadPlace | Error: " + logLine;
            EnvironmentManager::Get().PushLog("ERROR", logLine);
            IPC::PipeServer::Get().PushLog(entry);
            Logger::Log(("[LUA ERROR] " + logLine).c_str());
            return 0;
        }

        // --- File System Sandbox Helper ---
        static std::string ResolveWorkspacePath(const std::string& inputPath) {
            char path[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
                std::filesystem::path workspaceRoot = std::filesystem::path(path) / "BadPlaceExecutor" / "Workspace";
                if (!std::filesystem::exists(workspaceRoot)) std::filesystem::create_directories(workspaceRoot);
                std::filesystem::path targetPath = workspaceRoot / inputPath;
                std::filesystem::path normRoot = std::filesystem::weakly_canonical(workspaceRoot);
                std::filesystem::path normTarget = std::filesystem::weakly_canonical(targetPath);
                std::string rootStr = normRoot.string();
                std::string targetStr = normTarget.string();
                if (targetStr.find(rootStr) == 0) return targetStr;
            }
            return "";
        }

        int CppHttpStart(lua_State* L) {
            HttpRequest req;
            
            if (original_lua_type(L, 1) != LUA_TTABLE) {
                Logger::Log("CppHttpStart Error: Argument 1 is not a table");
                return 0;
            }

            original_lua_getfield(L, 1, "Url");
            if (original_lua_type(L, -1) == LUA_TSTRING) req.Url = original_lua_tolstring(L, -1, nullptr);
            if (original_lua_settop) original_lua_settop(L, -2);

            original_lua_getfield(L, 1, "Method");
            if (original_lua_type(L, -1) == LUA_TSTRING) req.Method = original_lua_tolstring(L, -1, nullptr);
            if (original_lua_settop) original_lua_settop(L, -2);

            original_lua_getfield(L, 1, "Body");
            if (original_lua_type(L, -1) == LUA_TSTRING) req.Body = original_lua_tolstring(L, -1, nullptr);
            if (original_lua_settop) original_lua_settop(L, -2);

            original_lua_getfield(L, 1, "Headers");
            if (original_lua_type(L, -1) == LUA_TTABLE && original_lua_pushnil && original_lua_next) {
                original_lua_pushnil(L);
                while (original_lua_next(L, -2) != 0) {
                    if (original_lua_type(L, -2) == LUA_TSTRING && original_lua_type(L, -1) == LUA_TSTRING) {
                        std::string key = original_lua_tolstring(L, -2, nullptr);
                        std::string val = original_lua_tolstring(L, -1, nullptr);
                        req.Headers[key] = val;
                    }
                    if (original_lua_settop) original_lua_settop(L, -2);
                }
            }
            if (original_lua_settop) original_lua_settop(L, -2);

            int ticketId = HttpManager::StartRequest(req);
            
            if (original_lua_pushinteger) original_lua_pushinteger(L, ticketId);
            return 1;
        }

        int CppHttpPoll(lua_State* L) {
            if (original_lua_type(L, 1) != LUA_TNUMBER) {
                if (original_lua_pushboolean) original_lua_pushboolean(L, 0);
                return 1;
            }

            const char* idStr = original_lua_tolstring(L, 1, nullptr);
            int ticketId = idStr ? std::atoi(idStr) : -1;

            HttpResponse res;
            if (HttpManager::PollRequest(ticketId, res)) {
                if (original_lua_pushboolean) original_lua_pushboolean(L, 1);
                original_lua_pushinteger(L, res.StatusCode);
                if (original_lua_pushstring) original_lua_pushstring(L, res.Body.c_str());
                return 3;
            }

            if (original_lua_pushboolean) original_lua_pushboolean(L, 0);
            return 1;
        }


        // --- File System C-ABI Bindings ---
        int CppWriteFile(lua_State* L) {
            if (original_lua_type(L, 1) != LUA_TSTRING || original_lua_type(L, 2) != LUA_TSTRING) return 0;
            std::string path = original_lua_tolstring(L, 1, nullptr);
            size_t dataLen;
            const char* data = original_lua_tolstring(L, 2, &dataLen);
            std::string resolved = ResolveWorkspacePath(path);
            if (!resolved.empty()) {
                std::ofstream file(resolved, std::ios::binary);
                if (file.is_open()) {
                    file.write(data, dataLen);
                    file.close();
                }
            }
            return 0;
        }

        int CppReadFile(lua_State* L) {
            if (original_lua_type(L, 1) != LUA_TSTRING) return 0;
            std::string path = original_lua_tolstring(L, 1, nullptr);
            std::string resolved = ResolveWorkspacePath(path);
            if (!resolved.empty() && std::filesystem::exists(resolved) && std::filesystem::is_regular_file(resolved)) {
                std::ifstream file(resolved, std::ios::binary | std::ios::ate);
                if (file.is_open()) {
                    std::streamsize size = file.tellg();
                    file.seekg(0, std::ios::beg);
                    std::string buffer(size, 0);
                    if (file.read(&buffer[0], size)) {
                        if (original_lua_pushstring) original_lua_pushstring(L, buffer.c_str()); // Wait, pushstring or pushlstring? Luau API doesn't have lua_pushlstring bound. That's fine if no null-bytes, but binaries might break. For script source it's fine! Let's rely on lua_pushstring for now.
                        return 1;
                    }
                }
            }
            if (original_lua_pushnil) original_lua_pushnil(L);
            return 1;
        }

        int CppMakeFolder(lua_State* L) {
            if (original_lua_type(L, 1) != LUA_TSTRING) return 0;
            std::string path = original_lua_tolstring(L, 1, nullptr);
            std::string resolved = ResolveWorkspacePath(path);
            if (!resolved.empty()) {
                std::filesystem::create_directories(resolved);
            }
            return 0;
        }

        int CppListFiles(lua_State* L) {
            if (original_lua_type(L, 1) != LUA_TSTRING) return 0;
            if (!original_lua_createtable || !original_lua_pushstring) return 0;
            std::string path = original_lua_tolstring(L, 1, nullptr);
            std::string resolved = ResolveWorkspacePath(path);
            original_lua_createtable(L, 0, 0); // lua_newtable is a macro for this
            if (!resolved.empty() && std::filesystem::exists(resolved) && std::filesystem::is_directory(resolved)) {
                if (original_lua_rawseti) {
                    int counter = 1;
                    for (const auto& entry : std::filesystem::directory_iterator(resolved)) {
                        std::string entryStr = entry.path().filename().string();
                        original_lua_pushstring(L, entryStr.c_str());
                        original_lua_rawseti(L, -2, counter++);
                    }
                } else {
                    Logger::Log("CppListFiles: lua_rawseti not resolved -- entries will be missing.");
                }
            }
            return 1;
        }

        int CppIsFile(lua_State* L) {
            if (original_lua_type(L, 1) != LUA_TSTRING) return 0;
            std::string path = original_lua_tolstring(L, 1, nullptr);
            std::string resolved = ResolveWorkspacePath(path);
            bool isF = (!resolved.empty() && std::filesystem::exists(resolved) && std::filesystem::is_regular_file(resolved));
            if (original_lua_pushboolean) original_lua_pushboolean(L, isF ? 1 : 0);
            return 1;
        }

        int CppIsFolder(lua_State* L) {
            if (original_lua_type(L, 1) != LUA_TSTRING) return 0;
            std::string path = original_lua_tolstring(L, 1, nullptr);
            std::string resolved = ResolveWorkspacePath(path);
            bool isF = (!resolved.empty() && std::filesystem::exists(resolved) && std::filesystem::is_directory(resolved));
            if (original_lua_pushboolean) original_lua_pushboolean(L, isF ? 1 : 0);
            return 1;
        }

        static std::string SanitizeName(const std::string& name) {
            std::string s = name;
            for (char& c : s) { if (c == ':' || c == '*' || c == '?' || c == '<' || c == '>' || c == '|' || c == '\\' || c == '/') c = '_'; }
            return s.empty() ? "Unnamed" : s;
        }

        int CppSaveInstance(lua_State* L) {
            Logger::Log("C++ SaveInstance: Starting native dump...");
            IPC::PipeServer::Get().PushLog("SaveInstance | Starting native dump...");

            original_lua_getfield(L, LUA_GLOBALSINDEX, "game");
            if (original_lua_type(L, -1) == LUA_TNIL) {
                Logger::Log("[ERROR] C++ SaveInstance: 'game' global not found.");
                return 0;
            }

            // Get Game Name
            std::string gameName = "World";
            original_lua_getfield(L, -1, "Name");
            if (original_lua_type(L, -1) == LUA_TSTRING) {
                const char* n = original_lua_tolstring(L, -1, nullptr);
                if (n) gameName = SanitizeName(n);
            }
            original_lua_settop(L, -2);

            char appdataPath[MAX_PATH];
            SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdataPath);
            std::filesystem::path workspaceRoot = std::filesystem::path(appdataPath) / "BadPlaceExecutor" / "Workspace";
            std::filesystem::path gameRootPath = workspaceRoot / gameName;
            std::filesystem::path scriptsRootPath = gameRootPath / "Scripts";
            
            try {
                std::filesystem::create_directories(scriptsRootPath);
            } catch (...) {
                Logger::Log("[ERROR] C++ SaveInstance: Failed to create base directory.");
                return 0;
            }

            // Pin a registry ref to `game` for parent-walking later.
            // luaL_ref POPS, so duplicate first — original stays on top.
            original_lua_pushvalue(L, -1);
            int rootRef = original_luaL_ref(L, LUA_REGISTRYINDEX);
            Logger::Log(("C++ SaveInstance: game pinned, rootRef=" + std::to_string(rootRef)).c_str());

            // Call game:GetDescendants()
            // Stack before: [game]
            original_lua_getfield(L, -1, "GetDescendants"); // Stack: [game, GetDescendants]
            original_lua_pushvalue(L, -2);                  // Stack: [game, GetDescendants, game]
            original_lua_remove(L, -3);                     // Stack: [GetDescendants, game]
            Logger::Log("C++ SaveInstance: Calling GetDescendants...");
            if (original_lua_pcall(L, 1, 1, 0) != 0) {     // Stack: [table] or [errmsg]
                const char* err = original_lua_tolstring(L, -1, nullptr);
                Logger::Log(("[ERROR] GetDescendants failed: " + std::string(err ? err : "?")).c_str());
                IPC::PipeServer::Get().PushLog("SaveInstance | Error: GetDescendants failed.");
                original_luaL_unref(L, LUA_REGISTRYINDEX, rootRef);
                return 0;
            }
            Logger::Log("C++ SaveInstance: GetDescendants returned.");

            int tableIdx = original_lua_gettop(L);
            int len = 0;
            if (original_lua_objlen) {
                len = original_lua_objlen(L, tableIdx);
            } else {
                // Manual fallback count
                original_lua_pushnil(L);
                while (original_lua_next(L, tableIdx) != 0) {
                    len++;
                    original_lua_settop(L, -2); // Pop value
                }
            }

            Logger::Log(("C++ SaveInstance: Found " + std::to_string(len) + " objects. Processing...").c_str());
            IPC::PipeServer::Get().PushLog("SaveInstance | Found " + std::to_string(len) + " objects. Processing...");

            int scriptCount = 0;
            for (int i = 1; i <= len; i++) {
                int loopTop = original_lua_gettop(L);
                
                original_lua_rawgeti(L, tableIdx, i);
                int objIdx = original_lua_gettop(L);
                if (original_lua_type(L, objIdx) == LUA_TNIL) {
                    original_lua_settop(L, loopTop);
                    continue;
                }

                // Identify Script via IsA
                bool isScript = false;
                original_lua_getfield(L, objIdx, "IsA");
                if (original_lua_type(L, -1) == LUA_TFUNCTION) {
                    const char* types[] = { "Script", "LocalScript", "ModuleScript" };
                    for (auto t : types) {
                        original_lua_pushvalue(L, -1); // IsA Func
                        original_lua_pushvalue(L, objIdx); // self
                        original_lua_pushstring(L, t);
                        if (original_lua_pcall(L, 2, 1, 0) == 0) {
                            if (original_lua_type(L, -1) == LUA_TBOOLEAN &&
                                original_lua_toboolean && original_lua_toboolean(L, -1)) {
                                // IsA returned true — this is a script!
                                isScript = true;
                                original_lua_settop(L, -2);
                                break;
                            }
                        }
                        original_lua_settop(L, -2);
                    }
                }
                original_lua_settop(L, -2); // Pop IsA

                if (isScript) {
                    // Source
                    original_lua_getfield(L, objIdx, "Source");
                    size_t srcLen = 0;
                    const char* src = original_lua_tolstring(L, -1, &srcLen);
                    
                    if (src && srcLen > 0) {
                        // Name
                        original_lua_getfield(L, objIdx, "Name");
                        std::string name = "Unnamed";
                        if (original_lua_type(L, -1) == LUA_TSTRING) {
                            const char* n = original_lua_tolstring(L, -1, nullptr);
                            if (n) name = SanitizeName(n);
                        }
                        original_lua_settop(L, -2);

                        // Path reconstruction via lua_rawequal parent comparison
                        std::vector<std::string> pathParts;
                        original_lua_pushvalue(L, objIdx);
                        while (true) {
                            original_lua_getfield(L, -1, "Parent");
                            if (original_lua_type(L, -1) == LUA_TNIL) {
                                original_lua_settop(L, -2);
                                break;
                            }

                            // Push game ref and use rawequal to compare
                            original_lua_rawgeti(L, LUA_REGISTRYINDEX, rootRef);
                            bool atRoot = original_lua_rawequal ? (original_lua_rawequal(L, -1, -2) != 0) : false;
                            original_lua_settop(L, -2); // Pop root ref, parent stays on top

                            if (atRoot) {
                                original_lua_settop(L, -2); // Pop parent, restore to current obj
                                break;
                            }

                            // Not root — get parent name and push to pathParts
                            original_lua_getfield(L, -1, "Name");
                            if (original_lua_type(L, -1) == LUA_TSTRING) {
                                const char* pRawName = original_lua_tolstring(L, -1, nullptr);
                                if (pRawName) pathParts.insert(pathParts.begin(), SanitizeName(pRawName));
                            }
                            original_lua_settop(L, -2); // Pop name

                            // Move up: replace current with parent
                            original_lua_pushvalue(L, -1); // duplicate parent
                            original_lua_remove(L, -2);     // remove previous current

                            if (pathParts.size() > 15) break; // Depth limit
                        }
                        original_lua_settop(L, -2); // Pop current for path loop

                        std::filesystem::path fullDir = scriptsRootPath;
                        for (const auto& p : pathParts) fullDir /= p;
                        
                        try {
                            if (!std::filesystem::exists(fullDir)) std::filesystem::create_directories(fullDir);
                            std::ofstream file(fullDir / (name + ".lua"), std::ios::binary);
                            if (file.is_open()) {
                                file.write(src, srcLen);
                                file.close();
                                scriptCount++;
                            }
                        } catch (...) {}
                    }
                }
                
                original_lua_settop(L, loopTop); // STACK SAFETY: Restore to before object processing
                
                if (i % 5000 == 0) {
                    Logger::Log(("SaveInstance Progress: " + std::to_string(i) + "/" + std::to_string(len)).c_str());
                    IPC::PipeServer::Get().PushLog("SaveInstance | Progress: " + std::to_string(i) + "/" + std::to_string(len));
                }
            }

            original_luaL_unref(L, LUA_REGISTRYINDEX, rootRef);
            original_lua_settop(L, -2); // Pop result table and game

            Logger::Log(("SaveInstance: Complete. Saved " + std::to_string(scriptCount) + " scripts.").c_str());
            IPC::PipeServer::Get().PushLog("SaveInstance | Complete! Saved " + std::to_string(scriptCount) + " scripts.");
            return 0;
        }

        int CppSaveMap(lua_State* L) {
            // Placeholder for now, but exposed
            Logger::Log("C++ SaveMap: Initializing engine serialization...");
            IPC::PipeServer::Get().PushLog("SaveMap | Attempting engine-side save (.poly)...");
            
            // We'll search for ResourceSaver in a future step
            IPC::PipeServer::Get().PushLog("SaveMap | Warning: Engine serialization under development for AOT.");
            return 0;
        }

        void EnvironmentManager::PushLog(const std::string& type, const std::string& message) {
            std::lock_guard<std::mutex> lock(m_logMutex);
            m_logQueue.push("[" + type + "] " + message);
        }

        const char* EnvironmentManager::PollLog() {
            std::lock_guard<std::mutex> lock(m_logMutex);
            if (m_logQueue.empty()) return nullptr;
            static std::string currentLog;
            currentLog = m_logQueue.front();
            m_logQueue.pop();
            return currentLog.c_str();
        }

        void EnvironmentManager::InitializeOverrides(lua_State* L) {
            if (m_isInitialized) return;
            if (!original_lua_pushcclosurek || !original_lua_setfield) return;

            original_lua_pushcclosurek(L, HookedPrint, "print", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "print");

            original_lua_pushcclosurek(L, HookedWarn, "warn", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "warn");

            original_lua_pushcclosurek(L, HookedError, "error", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "error");

            original_lua_pushcclosurek(L, CppHttpStart, "cpp_http_start", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "cpp_http_start");

            original_lua_pushcclosurek(L, CppHttpPoll, "cpp_http_poll", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "cpp_http_poll");

            // File System Bindings
            original_lua_pushcclosurek(L, CppWriteFile, "writefile", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "writefile");

            original_lua_pushcclosurek(L, CppReadFile, "readfile", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "readfile");

            original_lua_pushcclosurek(L, CppMakeFolder, "makefolder", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "makefolder");

            original_lua_pushcclosurek(L, CppListFiles, "listfiles", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "listfiles");

            original_lua_pushcclosurek(L, CppIsFile, "isfile", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "isfile");

            original_lua_pushcclosurek(L, CppIsFolder, "isfolder", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "isfolder");

            original_lua_pushcclosurek(L, CppSaveInstance, "saveinstance", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "saveinstance");

            original_lua_pushcclosurek(L, CppSaveMap, "savemap", 0, nullptr);
            original_lua_setfield(L, LUA_GLOBALSINDEX, "savemap");

            std::string wrapper = R"(
                request = function(reqTable)
                    if type(reqTable) ~= "table" then error("BadPlace | request() requires a table") end
                    
                    local ticketId = cpp_http_start(reqTable)
                    
                    if task and task.spawn then
                        task.spawn(function()
                            while true do
                                local ready, status, resBody = cpp_http_poll(ticketId)
                                if ready then
                                    if reqTable.Callback then
                                        reqTable.Callback({ StatusCode = status, Body = resBody })
                                    end
                                    break
                                end
                                task.wait(0.1)
                            end
                        end)
                    else
                        local ready, status, resBody
                        repeat
                            ready, status, resBody = cpp_http_poll(ticketId)
                        until ready
                        if reqTable.Callback then
                            reqTable.Callback({ StatusCode = status, Body = resBody })
                        end
                    end
                end
                
                http_request = request -- Alias
            )";
            
            if (ExecutionEngine::Get().CompileAndLoad(L, wrapper)) {
                if (original_lua_pcall) original_lua_pcall(L, 0, 0, 0);
            }

            Logger::Log("Luau environment log overrides and async wrappers injected.");
            m_isInitialized = true;
        }

        void EnvironmentManager::CacheGlobalFunctions(lua_State* L) {
            if (!original_lua_pushvalue || !original_lua_next || !original_lua_type || !original_lua_tolstring || !original_lua_settop || !original_lua_pushnil) return;
            std::string json = "[";
            bool first = true;
            original_lua_pushvalue(L, LUA_GLOBALSINDEX);
            original_lua_pushnil(L);
            while (original_lua_next(L, -2) != 0) {
                if (original_lua_type(L, -1) == LUA_TFUNCTION) {
                    if (original_lua_type(L, -2) == LUA_TSTRING) {
                        size_t len;
                        const char* key = original_lua_tolstring(L, -2, &len);
                        if (key) {
                            if (!first) json += ",";
                            json += "\"" + std::string(key, len) + "\"";
                            first = false;
                        }
                    }
                }
                original_lua_settop(L, -2);
            }
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
