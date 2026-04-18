#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace BadPlace {
    namespace Execution {

        struct HttpRequest {
            std::string Url;
            std::string Method = "GET";
            std::map<std::string, std::string> Headers;
            std::string Body;
            int RequestId = -1; // Ticket ID for polling
        };

        struct HttpResponse {
            int StatusCode = 0;
            std::string Body;
            std::map<std::string, std::string> Headers;
            int RequestId = -1;
        };

        class HttpManager {
        public:
            static int StartRequest(const HttpRequest& req);
            static bool PollRequest(int id, HttpResponse& outRes);
            
        private:
            static HttpResponse PerformSyncRequest(const HttpRequest& req);
            static int GetNextRequestId();
        };

    }
}
