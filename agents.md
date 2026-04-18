# Agent Memory Bank: BadPlace Executor

## Environment Status
- **Game Target**: Polytoria (Godot Engine 4.x .NET AOT).
- **Core DLLs**: `Luau.VM.dll` and `Luau.Compiler.dll`.
- **Architectural Rules**:
  - Enforce strict Separation of Concerns (SoC).
  - Do NOT assume Roblox mechanics, memory structures, or API behaviors apply automatically. 

## Architectural Design decisions
1. **Thread Safety**: Execution queues are securely drained directly within `lua_getfield` hooks monitoring `_update` / `_fixed_update`, guaranteeing perfect synchronization with the Godot engine loop.
2. **Luau Compilation**: The executor correctly relies on `luau_compile` to transform raw text into Luau generic bytecode (`\x06`), followed dynamically by `luau_load`.
3. **Log Interception**: Standard logging (`print`, `warn`, `error`) is scraped *inside* the Luau environment via `lua_pushcclosure` overrides instead of low-level Windows pipe detours (WriteFile) to maintain VM safety and avoid Godot background threading clashes.
4. **Injector API Constraint**: While inherently an anti-pattern for a payload DLL to host its own injecting APIs, `BadPlace_InjectIntoProcess` remains mapped purely for developmental debugging bridges into Avalonia UI. 
5. **Memory Formatting**: JSON arrays (e.g. for `BadPlace_GetFunctions()`) are to be constructed manually to minimize unnecessary C++ external repository baggage unless complexity dictates pulling `nlohmann/json`.
