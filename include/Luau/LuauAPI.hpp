#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>

// Forward declare lua_State as an opaque struct pointer
typedef struct lua_State lua_State;

// Standard types
typedef int (*lua_CFunction)(lua_State *L);

// --- VM DLL Pointers ---
typedef lua_State* (*luaL_newstate_t)();
typedef int (*lua_pcall_t)(lua_State* L, int nargs, int nresults, int errfunc);
typedef int (*lua_getfield_t)(lua_State* L, int idx, const char* k);
typedef void (*lua_settop_t)(lua_State* L, int idx);
typedef int (*lua_gettop_t)(lua_State* L);
typedef int (*luau_load_t)(lua_State* L, const char* chunkname, const char* data, size_t size, int env);

// --- COMPILER DLL Pointers ---
// luau_compile(const char* source, size_t size, void* options, size_t* outsize)
typedef char* (*luau_compile_t)(const char* source, size_t size, void* options, size_t* outsize);
// If luau_free is not exported, we use standard heap free, but typically it is. We won't strictly need it if we are just loading. 

// --- Extended Luau API Pointers ---
typedef int (*lua_next_t)(lua_State* L, int idx);
typedef int (*lua_type_t)(lua_State* L, int idx);
typedef const char* (*lua_tolstring_t)(lua_State* L, int idx, size_t* len);
typedef void (*lua_pushstring_t)(lua_State* L, const char* s);
typedef void (*lua_pushvalue_t)(lua_State* L, int idx);
typedef void (*lua_pushnil_t)(lua_State* L);
typedef void (*lua_pushcclosurek_t)(lua_State* L, lua_CFunction fn, const char* debugname, int nup, void* cont);
typedef void (*lua_setfield_t)(lua_State* L, int idx, const char* k);

// Extern globals for pointers
extern luaL_newstate_t original_luaL_newstate;
extern lua_pcall_t original_lua_pcall;
extern lua_getfield_t original_lua_getfield;
extern lua_setfield_t original_lua_setfield;
extern lua_settop_t original_lua_settop;
extern lua_gettop_t original_lua_gettop;
extern luau_load_t original_luau_load;

extern lua_next_t original_lua_next;
extern lua_type_t original_lua_type;
extern lua_tolstring_t original_lua_tolstring;
extern lua_pushstring_t original_lua_pushstring;
extern lua_pushvalue_t original_lua_pushvalue;
extern lua_pushnil_t original_lua_pushnil;
extern lua_pushcclosurek_t original_lua_pushcclosurek;

extern luau_compile_t original_luau_compile;

namespace BadPlace {
    namespace LuauAPI {
        bool Initialize();
        uintptr_t GetFunctionAddress(const wchar_t* dll_name, const char* function_name);
    }
}
