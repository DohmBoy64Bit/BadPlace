# BadPlace Executor

## Build Commands

- **Build DLL**: Run `build.bat` in root (requires MinGW g++ in PATH)
- **Build UI**: `dotnet build BadPlaceUI/BadPlaceUI.csproj` (requires .NET 10 SDK)
- **Full build**: Build DLL first, then UI (MSBuild auto-copies DLL to UI output)

## Key Constraints

- Target process name is `"Polytoria Client"` (with space) — not `"Polytoria"`
- UI requires Administrator privileges for `OpenProcess(PROCESS_ALL_ACCESS)` to succeed
- DLL must be built before UI build (auto-copied via `CopyBadPlaceDll` target)
- Wait ~800ms after injection before connecting pipes (DLL needs time to spawn pipe server threads)

## Architecture

- **C++ DLL** (`BadPlace.dll`): Pure backend payload, no injection logic. Exposes C-ABI exports.
- **C# UI** (Avalonia .NET 10): Handles injection via `Win32Injector.cs` and IPC via `PipeClient.cs`
- **IPC**: Named pipes `\\.\pipe\BadPlaceExec` and `\\.\pipe\BadPlaceLogs` — length-prefixed binary (4-byte uint32 + UTF-8 body)
- **Hook**: `lua_getfield` hook drives script execution queue on `_update`/`_fixed_update`

## Important Files

- `src/main.cpp` — DLL entry point
- `BadPlaceUI/Injection/Win32Injector.cs` — Process injection
- `BadPlaceUI/IPC/PipeClient.cs` — Pipe client for UI
- `BadPlaceUI/app.manifest` — Contains `requireAdministrator`

## Runtime Dependencies

- `Luau.VM.dll` and `Luau.Compiler.dll` must be present in Polytoria Client process
- WinLibs POSIX UCRT (MinGW) for DLL compilation

## Bytecode Recovery (Phase 1)

- `HookManager.cpp` hooks `luau_load` to intercept bytecode chunks
- Caches `.bin` files to `%APPDATA%\TheBadPlace\BytecodeCache`
- `saveinstance()` falls back to bytecode cache if script source is empty
- See `walkthrough.md` for full plan

## Decompilation (Phase 2)

- Polytoria uses Luau bytecode **v82 (0x52)** — older decompilers fail with `expected version 3...6, got 82`
- Bytecode may be compressed with **RSB1** (zstd) header — decompress first
- **Koralys** integrated at `thirdparty/Koralys/` (Python 3.13 + zstandard)
- Wrapper: `thirdparty/Koralys/luau_decomp.py <input.bin> [output.lua]`
- Requires patch: version 82 → 5 mapping in koralys.py:323
- Custom Luau compiler is backup if Koralys fails (build from `luau-lang/luau` source)