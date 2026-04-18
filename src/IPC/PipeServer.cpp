#include "IPC/PipeServer.hpp"
#include "Execution/ExecutionEngine.hpp"
#include "Core/Logger.hpp"
#include <thread>

#define EXEC_PIPE_NAME "\\\\.\\pipe\\BadPlaceExec"
#define LOG_PIPE_NAME  "\\\\.\\pipe\\BadPlaceLogs"

namespace BadPlace {
namespace IPC {

    PipeServer& PipeServer::Get() {
        static PipeServer instance;
        return instance;
    }

    void PipeServer::Start() {
        m_running = true;
        std::thread([this]() { ExecListenerThread(); }).detach();
        std::thread([this]() { LogBroadcastThread(); }).detach();
        Logger::Log("IPC pipe server started.");
    }

    void PipeServer::Stop() {
        m_running = false;
        // Wake any blocked ConnectNamedPipe by closing the handle
        if (m_hLogPipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hLogPipe);
            m_hLogPipe = INVALID_HANDLE_VALUE;
        }
    }

    // Called from EnvironmentManager (game thread) — queues the log safely
    void PipeServer::PushLog(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_logQueue.push(message);
    }

    // ── Exec listener ────────────────────────────────────────────────────────
    void PipeServer::ExecListenerThread() {
        while (m_running) {
            HANDLE hPipe = CreateNamedPipeA(
                EXEC_PIPE_NAME,
                PIPE_ACCESS_INBOUND,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,      // max instances
                0,      // outbound buffer
                65536,  // inbound buffer (64 KB)
                0,
                nullptr
            );

            if (hPipe == INVALID_HANDLE_VALUE) {
                Logger::Log("IPC: Failed to create exec pipe.");
                return;
            }

            Logger::Log("IPC: Exec pipe ready, waiting for UI...");
            if (!ConnectNamedPipe(hPipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
                CloseHandle(hPipe);
                continue;
            }

            Logger::Log("IPC: UI connected to exec pipe.");

            while (m_running) {
                // Read length prefix (4 bytes)
                uint32_t len = 0;
                DWORD bytesRead = 0;
                if (!ReadFile(hPipe, &len, sizeof(len), &bytesRead, nullptr) || bytesRead != sizeof(len))
                    break;

                // Sanity check
                if (len == 0 || len > 1024 * 1024) break;

                // Read script body
                std::string script(len, '\0');
                if (!ReadFile(hPipe, script.data(), len, &bytesRead, nullptr) || bytesRead != len)
                    break;

                Logger::LogF("IPC: Received script (%u bytes) — queuing.", len);
                Execution::ExecutionEngine::Get().QueueScript(script);
            }

            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            Logger::Log("IPC: UI disconnected from exec pipe. Waiting for reconnect...");
        }
    }

    // ── Log broadcaster ──────────────────────────────────────────────────────
    void PipeServer::LogBroadcastThread() {
        while (m_running) {
            m_hLogPipe = CreateNamedPipeA(
                LOG_PIPE_NAME,
                PIPE_ACCESS_OUTBOUND,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1,
                65536,  // outbound buffer (64 KB)
                0,
                0,
                nullptr
            );

            if (m_hLogPipe == INVALID_HANDLE_VALUE) {
                Logger::Log("IPC: Failed to create log pipe.");
                return;
            }

            Logger::Log("IPC: Log pipe ready, waiting for UI...");
            if (!ConnectNamedPipe(m_hLogPipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
                CloseHandle(m_hLogPipe);
                m_hLogPipe = INVALID_HANDLE_VALUE;
                continue;
            }

            m_logClientReady = true;
            Logger::Log("IPC: UI connected to log pipe.");

            // Drain queue and forward to UI until the write fails (UI disconnected)
            while (m_running && m_logClientReady) {
                std::string entry;
                {
                    std::lock_guard<std::mutex> lock(m_logMutex);
                    if (!m_logQueue.empty()) {
                        entry = m_logQueue.front();
                        m_logQueue.pop();
                    }
                }

                if (entry.empty()) {
                    Sleep(10); // Nothing to send — yield briefly
                    continue;
                }

                uint32_t len = static_cast<uint32_t>(entry.size());
                DWORD written = 0;

                if (!WriteFile(m_hLogPipe, &len, sizeof(len), &written, nullptr) ||
                    !WriteFile(m_hLogPipe, entry.c_str(), len, &written, nullptr)) {
                    // UI disconnected
                    m_logClientReady = false;
                    break;
                }
            }

            m_logClientReady = false;
            DisconnectNamedPipe(m_hLogPipe);
            CloseHandle(m_hLogPipe);
            m_hLogPipe = INVALID_HANDLE_VALUE;
            Logger::Log("IPC: UI disconnected from log pipe. Waiting for reconnect...");
        }
    }

} // namespace IPC
} // namespace BadPlace
