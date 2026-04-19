#pragma once
#include <Windows.h>
#include <string>

namespace BadPlace {
    namespace Hooks {
        bool Initialize();
        void Cleanup();
        void SetCurrentGameId(const std::string& gameId);
    }
}
