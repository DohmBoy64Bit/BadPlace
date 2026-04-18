# Agent Memory Bank: BadPlace Executor

## Environment Status
- **Game Target**: Polytoria (Godot Engine 4.x .NET AOT).
- **Core DLLs**: `Luau.VM.dll` and `Luau.Compiler.dll`.
- **Compiler**: WinLibs POSIX UCRT (GCC 15.2.0), MinGW-based.
- **Build System**: `build.bat` → outputs `BadPlace.dll`.
- **UI Framework**: Avalonia C# (.NET 10), project at `BadPlaceUI/BadPlaceUI.csproj`.
- **Repository**: `https://github.com/DohmBoy64Bit/BadPlace`
- **Architectural Rules**:
  - Enforce strict Separation of Concerns (SoC).
  - Do NOT assume Roblox mechanics, memory structures, or API behaviors apply automatically.
  - The DLL payload is a pure backend — no UI logic, no injection logic inside the DLL.

## Architectural Design Decisions
1. **Thread Safety**: Execution queues are securely drained directly within `lua_getfield` hooks monitoring `_update` / `_fixed_update`, guaranteeing perfect synchronization with the Godot engine loop.
2. **Luau Compilation**: The executor correctly relies on `luau_compile` to transform raw text into Luau generic bytecode (`\x06`), followed dynamically by `luau_load`.
3. **Log Interception**: Standard logging (`print`, `warn`, `error`) is scraped *inside* the Luau environment via `lua_pushcclosurek` overrides. Intercepted logs are simultaneously pushed to the thread-safe `EnvironmentManager` queue (for C-ABI polling) AND to the `PipeServer` log queue (for live UI streaming).
4. **Injector Responsibility**: `BadPlace_InjectIntoProcess` has been **removed** from the DLL. Injection is fully delegated to the Avalonia UI via `Win32Injector.cs` (OpenProcess → VirtualAllocEx → WriteProcessMemory → CreateRemoteThread). The DLL is purely a backend payload.
5. **Memory Formatting**: JSON arrays (e.g. for `BadPlace_GetFunctions()`) are constructed manually to minimize unnecessary C++ external repository baggage unless complexity dictates pulling `nlohmann/json`.
6. **IPC Architecture**: The DLL hosts a **Named Pipe Server** (`\\.\pipe\BadPlaceExec` + `\\.\pipe\BadPlaceLogs`) in two background threads. The UI is a thin pipe client. Protocol is length-prefixed binary (4-byte uint32 length + UTF-8 body). This is the sole communication channel between the UI and the injected DLL.
7. **Console/Terminal**: The legacy `ConsoleThread` (fgets stdin loop) and `AllocConsole` have been fully removed. The DLL boots completely silently. All output goes through the Logger and/or the log pipe.
8. **UI Admin Rights**: `BadPlaceUI.exe` requires and requests administrator privileges via `app.manifest` (`requireAdministrator`) for `OpenProcess(PROCESS_ALL_ACCESS)` to succeed.
9. **Async Native Polling (Networking)**: C++ asynchronously fetches HTTP responses off-thread via `WinHttp` but **never** blindly fires closures directly into the Godot loop natively (`lua_pcall` inside `_update` crashes engine bindings). It adopts a pure string-ID ticket polling model leveraging a natively compiled Luau `request` polyfill script mapped tightly into `LUA_GLOBALSINDEX` that spins on engine-safe `task.spawn` loops.
10. **Native C++ Overflows & Tree-Traversal**: Complex, unbounded table iterations (like `saveinstance()` dumping 21,000+ objects) must manage the Lua stack precisely. A strict `lua_settop(L, loopTop)` cap policy ensures memory bounds are never exceeded, bypassing Luau VM limit crashes that occur with equivalent mapped Lua scripts.
11. **Luau VM API Expansion**: `LuauAPI.hpp` resolves ~70 un-obfuscated `Luau.VM.dll` exports directly via `pefile` mapped pointers. This bridges advanced, stack-safe operations directly to the host C++ container without breaking Godot's sandbox restrictions (e.g. `lua_rawequal` bypasses boolean mismatch falsing on type checking, `lua_pushthread` supports raw coroutine interactions).

## Key File Map
| File | Role |
|------|------|
| `src/main.cpp` | DLL entry point, C-ABI exports, starts PipeServer |
| `src/Hooks/HookManager.cpp` | `lua_getfield` hook, drives ExecutionEngine + EnvironmentManager |
| `src/Execution/ExecutionEngine.cpp` | Thread-safe script queue + luau_compile/luau_load execution |
| `src/Execution/EnvironmentManager.cpp` | Overrides print/warn/error, implements native `saveinstance`/`savemap` |
| `src/IPC/PipeServer.cpp` | Named pipe server — exec inbound thread + log outbound thread |
| `src/Luau/LuauAPI.cpp` | Resolves 70+ Luau function pointers from Luau.VM.dll at runtime |
| `BadPlaceUI/Injection/Win32Injector.cs` | C# DLL injector — targets "Polytoria Client" process by name |
| `BadPlaceUI/IPC/PipeClient.cs` | C# pipe client — writes scripts, reads logs on background thread |
| `BadPlaceUI/MainWindow.axaml.cs` | Wires Attach (inject + pipe connect) and Execute (send script) |

## Confirmed Working (End-to-End Verified)
- DLL injected silently into "Polytoria Client" process via Avalonia UI Attach button.
- Both named pipes (`BadPlaceExec`, `BadPlaceLogs`) connect successfully after injection.
- Scripts sent from AvaloniaEdit editor execute inside the Luau VM.
- `print()` output is intercepted by C++ hooks and streamed live back to the UI output panel.
- Asynchronous networking module (`request`) successfully fetches payloads yielding seamlessly through Godot's built-in `TaskScheduler` using polyfilled polling endpoints.
- Native `saveinstance()` fully dumps 21,000+ object environments without crashing engine threads by heavily policing `lua_settop()`.
- Polytoria `NullReferenceException` on `FreePTCallback` is a pre-existing Polytoria engine bug — unrelated to BadPlace.

## Known Gotchas
- The process name for Polytoria is `"Polytoria Client"` (with a space) — NOT `"Polytoria"`.
- `BadPlace.dll` is auto-copied to the UI build output via an MSBuild `AfterTargets="Build"` step in `BadPlaceUI.csproj`.
- The UI must be run as Administrator for `OpenProcess(PROCESS_ALL_ACCESS)` to succeed.
- A ~800ms delay is added in the UI after injection before connecting pipes, to allow the DLL's pipe server threads time to start.
- The C++ logger uses `AllocConsole` for raw debug output — this is intentional for development visibility and can be removed for production stealth.
