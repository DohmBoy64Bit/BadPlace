#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <queue>
#include <mutex>
#include <atomic>

namespace BadPlace {
namespace IPC {

    class PipeServer {
    public:
        static PipeServer& Get();

        // Boot both pipe threads
        void Start();
        void Stop();

        // Called by EnvironmentManager hooks to forward logs to the UI
        void PushLog(const std::string& message);

    private:
        PipeServer() = default;

        // Inbound: receives script strings from the UI
        void ExecListenerThread();
        // Outbound: drains m_logQueue and writes to the UI
        void LogBroadcastThread();

        std::atomic<bool>  m_running          { false };
        std::atomic<bool>  m_logClientReady   { false };

        // Log queue — written by game thread, drained by LogBroadcastThread
        std::queue<std::string> m_logQueue;
        std::mutex              m_logMutex;

        HANDLE m_hLogPipe { INVALID_HANDLE_VALUE };
    };

} // namespace IPC
} // namespace BadPlace
