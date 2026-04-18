#pragma once
#include <string>
#include <queue>
#include <mutex>

typedef struct lua_State lua_State;

namespace PolyHook {
    namespace Execution {
        class ExecutionEngine {
        private:
            std::queue<std::string> m_scriptQueue;
            std::mutex m_queueMutex;
            
            ExecutionEngine() = default;

        public:
            static ExecutionEngine& Get() {
                static ExecutionEngine instance;
                return instance;
            }

            // Expose C-ABI compatible method for external UI / manual push
            void QueueScript(const std::string& script);

            // Triggered seamlessly from hooked tick functions
            void ExecuteQueue(lua_State* L);

            // Utility to wrap luau compiler and load
            bool CompileAndLoad(lua_State* L, const std::string& script);
        };
    }
}
