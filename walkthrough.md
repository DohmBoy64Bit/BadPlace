# Project Status Summary: BadPlace Executor

The BadPlace project is a high-performance script executor and environment dumper designed for **Polytoria** (built on Godot 4 / .NET AOT). It consists of a C++ core DLL and an Avalonia C# UI.

## 🚀 Recent Major Achievements

### 1. High-Performance Native `saveinstance()`
We successfully migrated the environment dumper from a Luau script to a C++ native implementation within `BadPlace.dll`.
- **Performance:** Capable of iterating through 21,000+ objects without engine stalls or memory limit crashes.
- **Stack Safety:** Implemented a strict "pin-before-consume" stack management policy and refactored iteration to use absolute stack indexing (`gameAbsIdx`) to prevent crashes.
- **Hierarchy Reconstruction:** Rebuilds the folder structure of the game level locally in `%AppData%\TheBadPlace\Workspace`.

### 2. Luau API Expansion
We significantly expanded the Luau API surface in `LuauAPI.hpp/cpp`.
- We now resolve **70+ un-obfuscated exports** from `Luau.VM.dll` at runtime.
- This includes advanced table iteration (`lua_next`, `lua_objlen`), coroutine management, and raw equality checks (`lua_rawequal`) that bypass engine-side security checks.

### 3. Stability & Bug Fixes
- **Resolved Stalls:** Fixed a critical bug in `GetDescendants` where the Lua stack was corrupted by `luaL_ref`.
- **Crash Prevention:** Identified that Polytoria strips `luaL_ref` from its DLL; we refactored the backend to use `lua_ref` or stack indexing to eliminate segfaults.
- **Path Unification:** Unified all output and log paths to `%AppData%\TheBadPlace` to match the UI and Installer.

## 🔍 Current Focus: Bytecode Recovery

Polytoria strips the `.Source` property from scripts before sending them to the client to prevent code theft. We are currently implementing a multi-phase plan to bypass this.

### ✅ Phase 1: Bytecode Interception (COMPLETED)
- **Hooked `luau_load`:** Every time the game loads a script, we now intercept the raw bytecode chunk.
- **Cache System:** Intercepted chunks are saved to `%AppData%\TheBadPlace\BytecodeCache` as `.bin` files.
- **Dumper Integration:** `saveinstance()` now automatically searches this cache. If it finds a match for an empty script, it saves the `.bin` file alongside the object.

### ⏳ Phase 2: Standalone Decompilation (NEXT STEP)
The plan is to integrate a standalone CLI decompiler (like `unluau` or `luauDec`) into the workflow.
- **UI Logic:** The C# UI will detect `.bin` files in the dumped workspace, run the decompiler in the background, and replace the bytecode with readable `.lua` code.

## 📍 Where We Left Off
We just pushed the Phase 1 changes (the `luau_load` hook and cache recovery logic). The dumper is now successfully saving `.bin` bytecode payloads for protected scripts. 

**Next Action:** Integrate the decompression/decompilation executable into the `BadPlaceUI` build and logic.
