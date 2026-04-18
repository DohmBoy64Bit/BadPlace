@echo off
setlocal

set CXX=g++
set CXXFLAGS=-shared -std=c++17 -Wall -Wno-unused-variable -Wno-unused-function
set DEFINES=-DMH_STATIC -DWIN32_LEAN_AND_MEAN
set INCLUDES=-I include -I thirdparty/minhook/include -I thirdparty/minhook/src/hde
set LINKLIBS=-lpsapi -lwinhttp -static
set TARGET=BadPlace.dll

set SOURCES=src/main.cpp src/Core/Logger.cpp src/Hooks/HookManager.cpp src/Luau/LuauAPI.cpp src/Execution/ExecutionEngine.cpp src/Execution/EnvironmentManager.cpp src/IPC/PipeServer.cpp src/Execution/HttpManager.cpp
set MINHOOK_SOURCES=thirdparty/minhook/src/hook.c thirdparty/minhook/src/buffer.c thirdparty/minhook/src/trampoline.c thirdparty/minhook/src/hde/hde64.c

echo Building %TARGET%...
%CXX% %CXXFLAGS% -o %TARGET% %SOURCES% %MINHOOK_SOURCES% %INCLUDES% %DEFINES% %LINKLIBS%

if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
) else (
    echo [SUCCESS] Build succeeded: %TARGET%
)
