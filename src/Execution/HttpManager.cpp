#include "Execution/HttpManager.hpp"
#include "Core/Logger.hpp"
#include "IPC/PipeServer.hpp"
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <iostream>

#pragma comment(lib, "winhttp.lib")

namespace BadPlace {
    namespace Execution {

        // Helper to parse URL into Host, Path, Port
        bool ParseUrl(const std::string& url, bool& isHttps, std::wstring& host, std::wstring& path, INTERNET_PORT& port) {
            URL_COMPONENTSW urlComp = { 0 };
            urlComp.dwStructSize = sizeof(urlComp);
            urlComp.dwHostNameLength = (DWORD)-1;
            urlComp.dwUrlPathLength = (DWORD)-1;
            urlComp.dwExtraInfoLength = (DWORD)-1;

            std::wstring widestr = std::wstring(url.begin(), url.end());
            if (!WinHttpCrackUrl(widestr.c_str(), 0, 0, &urlComp)) {
                return false;
            }

            isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
            host = std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength);
            path = std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
            if (urlComp.dwExtraInfoLength > 0) {
                path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
            }
            port = urlComp.nPort;
            return true;
        }

        static std::map<int, HttpResponse> s_readyResponses;
        static std::mutex s_responseMutex;
        static int s_requestCounter = 0;

        int HttpManager::GetNextRequestId() {
            std::lock_guard<std::mutex> lock(s_responseMutex);
            return ++s_requestCounter;
        }

        int HttpManager::StartRequest(const HttpRequest& req) {
            int ticketId = GetNextRequestId();
            HttpRequest threadReq = req;
            threadReq.RequestId = ticketId;

            std::string logMsg = "BadPlace | StartRequest dispatcher called for: " + req.Url + " (Ticket: " + std::to_string(ticketId) + ")";
            IPC::PipeServer::Get().PushLog(logMsg);
            
            std::thread([threadReq]() {
                IPC::PipeServer::Get().PushLog("BadPlace | HttpManager background thread started!");
                HttpResponse res = PerformSyncRequest(threadReq);
                
                std::lock_guard<std::mutex> lock(s_responseMutex);
                s_readyResponses[threadReq.RequestId] = res;
            }).detach();

            return ticketId;
        }

        bool HttpManager::PollRequest(int id, HttpResponse& outRes) {
            std::lock_guard<std::mutex> lock(s_responseMutex);
            auto it = s_readyResponses.find(id);
            if (it != s_readyResponses.end()) {
                outRes = it->second;
                s_readyResponses.erase(it);
                return true;
            }
            return false;
        }

        HttpResponse HttpManager::PerformSyncRequest(const HttpRequest& req) {
            HttpResponse res;
            res.RequestId = req.RequestId;

            bool isHttps = false;
            std::wstring host, path;
            INTERNET_PORT port;

            if (!ParseUrl(req.Url, isHttps, host, path, port)) {
                Logger::LogF("HTTP Error: Failed to parse URL: %s", req.Url.c_str());
                res.StatusCode = 0;
                res.Body = "Invalid URL";
                return res;
            }

            Logger::LogF("HTTP: Starting %s request to %s", req.Method.c_str(), req.Url.c_str());

            HINTERNET hSession = WinHttpOpen(L"BadPlace/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) {
                Logger::Log("HTTP Error: WinHttpOpen failed.");
                return res;
            }

            HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                return res;
            }

            std::wstring wMethod = std::wstring(req.Method.begin(), req.Method.end());
            DWORD dwFlags = isHttps ? WINHTTP_FLAG_SECURE : 0;

            HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);
            if (!hRequest) {
                Logger::Log("HTTP Error: WinHttpOpenRequest failed.");
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return res;
            }

            // Headers
            std::wstring headers;
            for (auto const& [key, val] : req.Headers) {
                headers += std::wstring(key.begin(), key.end()) + L": " + std::wstring(val.begin(), val.end()) + L"\r\n";
            }

            // Send Request
            BOOL bResults = WinHttpSendRequest(hRequest, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), (DWORD)-1L, (LPVOID)req.Body.c_str(), (DWORD)req.Body.length(), (DWORD)req.Body.length(), 0);

            if (bResults) {
                bResults = WinHttpReceiveResponse(hRequest, NULL);
                if (!bResults) Logger::LogF("HTTP Error: WinHttpReceiveResponse failed (%lu)", GetLastError());
            } else {
                Logger::LogF("HTTP Error: WinHttpSendRequest failed (%lu)", GetLastError());
            }

            if (bResults) {
                // Status Code
                DWORD dwStatusCode = 0;
                DWORD dwSize = sizeof(dwStatusCode);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
                res.StatusCode = (int)dwStatusCode;

                // Body
                DWORD dwDownloaded = 0;
                do {
                    dwSize = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                    if (dwSize == 0) break;

                    char* pszOutBuffer = new char[dwSize + 1];
                    ZeroMemory(pszOutBuffer, dwSize + 1);

                    if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                        res.Body += std::string(pszOutBuffer, dwDownloaded);
                    }
                    delete[] pszOutBuffer;
                } while (dwSize > 0);

                Logger::LogF("HTTP: Response processed. Status: %d, Body size: %zu", res.StatusCode, res.Body.length());
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            return res;
        }

    }
}
