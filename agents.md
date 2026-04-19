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

### Why luau_load Hook?

Direct memory extraction during `saveinstance()` would require:
- Reverse-engineering Polytoria's specific script memory offsets
- Offsets could break on game updates
- ZSTD decompression (RSB1) handling in-game

Hooking `luau_load` intercepts bytecode AFTER engine deserializes it: reliable, works cross-version.

### Decompilation (Phase 2)

- Polytoria uses Luau bytecode **v82 (0x52)** — older decompilers fail with `expected version 3...6, got 82`
- Bytecode may be compressed with **RSB1** (zstd) header
- **Medal** (better quality): `thirdparty/medal/target/release/luau-lifter.exe <input.bin>`
- **Koralys** (fallback): `thirdparty/Koralys/luau_decomp.py <input.bin> [output.lua]`
- Medal output uses proper function names, upvalue tracking, cleaner API calls
- Koralys requires patch for v82: version 82 → 5 mapping in koralys.py:323

### UI Integration Workflow (Backup Plan Only — Don't Implement Yet)

**Folder Structure (match saveinstance hierarchy):**
```
%APPDATA%\TheBadPlace\BytecodeCache\
  <GameName>\
    <Path\To\Script.bin>
```
Example: `World\PlayerGUI\CurrencyHUD\UIView\LocalScript.bin`

Same structure for decompiled output after Medal.

**Implementation Options:**

1. **Auto-decompile after saveinstance** (Recommended - most seamless)
   - After dump completes, detect all `.bin` files in workspace
   - Run medal CLI on each in background thread
   - Replace `.bin` with readable `.lua` automatically
   - User sees clean decompiled code immediately

2. **Manual decompile button**
   - "Decompile" button in script viewer
   - Converts clicked/selected script to readable Lua
   - Shows progress indicator for batch operations

3. **View toggle**
   - Toggle between: Raw Bytecode / Disassembled / Decompiled
   - Switch tabs in script preview panel

4. **Cache decompiled results**
   - Store decompiled `.lua` in separate cache
   - Skip decompiler if `.lua` already exists
   - "Recompile" option to refresh

5. **Progress UI**
   - Show decompilation progress bar
   - Cancel button for batch operations
   - Estimated time remaining

### Future UI (Long-term)

- **Darkdex-like interface** - Modern dark-themed UI with:
  - Script browser with folder tree view
  - Search/filter scripts by name
  - "Tools" menu with decompile, disassemble, copy options
  - Script preview pane with syntax highlighting
  - Settings panel for cache management

### Alternative: Memory Offset Approach (Requires Research)

**Theory:** Instead of hooking `luau_load`, directly read bytecode from script objects during `saveinstance()`.

**How it would work:**
```cpp
// Pseudocode:
uintptr_t script_ptr = /* get from Lua stack during iteration */;
uintptr_t bytecode_ptr = *(uintptr_t*)(script + bytecodeOffset); // Offset TBD
int len = *(int*)(bytecodePtr + lengthOffset);
char* data = (char*)(bytecodePtr + dataOffset);
```

**Required for implementation:**
1. Reverse-engineer Polytoria's script object memory layout
2. Find bytecode offset within script objects (like Roblox's ~0x120)
3. Handle possible RSB1 compression

**Challenges:**
- Offsets vary between game versions
- Requires debugging live game process
- Godot stores scripts differently than Roblox

**Research resources:**
- WeAreDevs forum: "How do you implement getscriptbytecode and decompile?"
- Godot engine source: `script_language.cpp` / bytecode handling
- Polytoria .pck file contains compiled scripts

### Research Notes (2026-04-19)

**Attempted approach:** Attach debugger → scan for script name string ("NumberUtils") → scan nearby for bytecode header `52 01`.

**Findings:**
- Script names found at multiple addresses (e.g., `0x1E41AEEF140`)
- Bytecode header `52 01` found at `0x7FF676A26E12` in executable code section
- No direct offset found between string and bytecode in tested addresses
- Godot may use different structure than Roblox (different memory layout)

**Conclusion:** Current `luau_load` hook approach remains more reliable:
- No reverse engineering required
- Works across game versions
- Already implemented and working