#include "Luau/LuauAPI.hpp"
#include "Core/Logger.hpp"

luaL_newstate_t original_luaL_newstate = nullptr;
lua_pcall_t original_lua_pcall = nullptr;
lua_getfield_t original_lua_getfield = nullptr;
lua_settop_t original_lua_settop = nullptr;
lua_gettop_t original_lua_gettop = nullptr;
luau_load_t original_luau_load = nullptr;

luau_compile_t original_luau_compile = nullptr;

lua_next_t original_lua_next = nullptr;
lua_type_t original_lua_type = nullptr;
lua_tolstring_t original_lua_tolstring = nullptr;
lua_pushstring_t original_lua_pushstring = nullptr;
lua_pushvalue_t original_lua_pushvalue = nullptr;
lua_pushnil_t original_lua_pushnil = nullptr;
lua_pushboolean_t original_lua_pushboolean = nullptr;
lua_pushcclosurek_t original_lua_pushcclosurek = nullptr;
lua_setfield_t original_lua_setfield = nullptr;

lua_pushinteger_t original_lua_pushinteger = nullptr;
lua_createtable_t original_lua_createtable = nullptr;
lua_rawgeti_t original_lua_rawgeti = nullptr;
lua_rawseti_t original_lua_rawseti = nullptr;
luaL_ref_t original_luaL_ref = nullptr;
luaL_unref_t original_luaL_unref = nullptr;

namespace BadPlace {
    namespace LuauAPI {

        uintptr_t GetFunctionAddress(const wchar_t* dll_name, const char* function_name) {
            HMODULE module = GetModuleHandleW(dll_name);
            if (!module) {
                module = LoadLibraryW(dll_name);
                if (!module) return 0;
            }
            return (uintptr_t)GetProcAddress(module, function_name);
        }

        bool Initialize() {
            Logger::Log("Initializing Luau API pointers...");

            original_luaL_newstate = (luaL_newstate_t)GetFunctionAddress(L"Luau.VM.dll", "luaL_newstate");
            original_lua_pcall = (lua_pcall_t)GetFunctionAddress(L"Luau.VM.dll", "lua_pcall");
            original_lua_getfield = (lua_getfield_t)GetFunctionAddress(L"Luau.VM.dll", "lua_getfield");
            original_lua_settop = (lua_settop_t)GetFunctionAddress(L"Luau.VM.dll", "lua_settop");
            original_lua_gettop = (lua_gettop_t)GetFunctionAddress(L"Luau.VM.dll", "lua_gettop");
            original_luau_load = (luau_load_t)GetFunctionAddress(L"Luau.VM.dll", "luau_load");

            // Extension functions
            original_lua_next = (lua_next_t)GetFunctionAddress(L"Luau.VM.dll", "lua_next");
            original_lua_type = (lua_type_t)GetFunctionAddress(L"Luau.VM.dll", "lua_type");
            original_lua_tolstring = (lua_tolstring_t)GetFunctionAddress(L"Luau.VM.dll", "lua_tolstring");
            original_lua_pushstring = (lua_pushstring_t)GetFunctionAddress(L"Luau.VM.dll", "lua_pushstring");
            original_lua_pushvalue = (lua_pushvalue_t)GetFunctionAddress(L"Luau.VM.dll", "lua_pushvalue");
            original_lua_pushnil = (lua_pushnil_t)GetFunctionAddress(L"Luau.VM.dll", "lua_pushnil");
            original_lua_pushboolean = (lua_pushboolean_t)GetFunctionAddress(L"Luau.VM.dll", "lua_pushboolean");
            original_lua_pushcclosurek = (lua_pushcclosurek_t)GetFunctionAddress(L"Luau.VM.dll", "lua_pushcclosurek");
            original_lua_setfield = (lua_setfield_t)GetFunctionAddress(L"Luau.VM.dll", "lua_setfield");

            original_lua_pushinteger = (lua_pushinteger_t)GetFunctionAddress(L"Luau.VM.dll", "lua_pushinteger");
            original_lua_createtable = (lua_createtable_t)GetFunctionAddress(L"Luau.VM.dll", "lua_createtable");
            original_lua_rawgeti = (lua_rawgeti_t)GetFunctionAddress(L"Luau.VM.dll", "lua_rawgeti");
            original_lua_rawseti = (lua_rawseti_t)GetFunctionAddress(L"Luau.VM.dll", "lua_rawseti");
            original_luaL_ref = (luaL_ref_t)GetFunctionAddress(L"Luau.VM.dll", "luaL_ref");
            original_luaL_unref = (luaL_unref_t)GetFunctionAddress(L"Luau.VM.dll", "luaL_unref");

            original_luau_compile = (luau_compile_t)GetFunctionAddress(L"Luau.Compiler.dll", "luau_compile");

            if (!original_lua_pcall || !original_lua_getfield || !original_luau_load || !original_luau_compile) {
                Logger::Log("Failed to resolve critical Luau VM/Compiler components!");
                return false;
            }

            Logger::Log("Luau API initialized successfully.");
            return true;
        }

    }
}
