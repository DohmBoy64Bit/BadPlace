#include "Luau/LuauAPI.hpp"
#include "Core/Logger.hpp"

// =============================================================================
// Global definitions
// =============================================================================

// Core
luaL_newstate_t  original_luaL_newstate  = nullptr;
lua_pcall_t      original_lua_pcall      = nullptr;
lua_call_t       original_lua_call       = nullptr;
lua_getfield_t   original_lua_getfield   = nullptr;
lua_setfield_t   original_lua_setfield   = nullptr;
lua_settop_t     original_lua_settop     = nullptr;
lua_gettop_t     original_lua_gettop     = nullptr;
lua_absindex_t   original_lua_absindex   = nullptr;
luau_load_t      original_luau_load      = nullptr;
lua_checkstack_t original_lua_checkstack = nullptr;
luau_compile_t   original_luau_compile   = nullptr;

// Stack / Type utilities
lua_type_t        original_lua_type        = nullptr;
lua_typename_t    original_lua_typename    = nullptr;
lua_equal_t       original_lua_equal       = nullptr;
lua_rawequal_t    original_lua_rawequal    = nullptr;
lua_lessthan_t    original_lua_lessthan    = nullptr;
lua_isnumber_t    original_lua_isnumber    = nullptr;
lua_isstring_t    original_lua_isstring    = nullptr;
lua_isuserdata_t  original_lua_isuserdata  = nullptr;
lua_iscfunction_t original_lua_iscfunction = nullptr;
lua_insert_t      original_lua_insert      = nullptr;
lua_replace_t     original_lua_replace     = nullptr;
lua_remove_t      original_lua_remove      = nullptr;
lua_concat_t      original_lua_concat      = nullptr;
lua_next_t        original_lua_next        = nullptr;
lua_objlen_t      original_lua_objlen      = nullptr;

// Push
lua_pushnil_t       original_lua_pushnil       = nullptr;
lua_pushboolean_t   original_lua_pushboolean   = nullptr;
lua_pushinteger_t   original_lua_pushinteger   = nullptr;
lua_pushnumber_t    original_lua_pushnumber    = nullptr;
lua_pushunsigned_t  original_lua_pushunsigned  = nullptr;
lua_pushstring_t    original_lua_pushstring    = nullptr;
lua_pushlstring_t   original_lua_pushlstring   = nullptr;
lua_pushvalue_t     original_lua_pushvalue     = nullptr;
lua_pushcclosurek_t original_lua_pushcclosurek = nullptr;
lua_pushthread_t    original_lua_pushthread    = nullptr;
lua_error_t         original_lua_error         = nullptr;

// Read (to*)
lua_tolstring_t   original_lua_tolstring   = nullptr;
lua_toboolean_t   original_lua_toboolean   = nullptr;
lua_tonumberx_t   original_lua_tonumberx   = nullptr;
lua_tointegerx_t  original_lua_tointegerx  = nullptr;
lua_tounsignedx_t original_lua_tounsignedx = nullptr;
lua_tocfunction_t original_lua_tocfunction = nullptr;
lua_touserdata_t  original_lua_touserdata  = nullptr;
lua_tothread_t    original_lua_tothread    = nullptr;

// Table
lua_createtable_t  original_lua_createtable  = nullptr;
lua_gettable_t     original_lua_gettable     = nullptr;
lua_settable_t     original_lua_settable     = nullptr;
lua_rawget_t       original_lua_rawget       = nullptr;
lua_rawset_t       original_lua_rawset       = nullptr;
lua_rawgeti_t      original_lua_rawgeti      = nullptr;
lua_rawseti_t      original_lua_rawseti      = nullptr;
lua_rawgetfield_t  original_lua_rawgetfield  = nullptr;
lua_rawsetfield_t  original_lua_rawsetfield  = nullptr;
lua_getmetatable_t original_lua_getmetatable = nullptr;
lua_setmetatable_t original_lua_setmetatable = nullptr;
lua_getfenv_t      original_lua_getfenv      = nullptr;
lua_setfenv_t      original_lua_setfenv      = nullptr;
lua_setsafeenv_t   original_lua_setsafeenv   = nullptr;
lua_setreadonly_t  original_lua_setreadonly  = nullptr;
lua_getreadonly_t  original_lua_getreadonly  = nullptr;

// Ref
luaL_ref_t   original_luaL_ref   = nullptr;
luaL_unref_t original_luaL_unref = nullptr;
lua_ref_t    original_lua_ref    = nullptr;
lua_unref_t  original_lua_unref  = nullptr;

// Coroutine
lua_newthread_t   original_lua_newthread   = nullptr;
lua_xmove_t       original_lua_xmove       = nullptr;
lua_resume_t      original_lua_resume      = nullptr;
lua_yield_t       original_lua_yield       = nullptr;
lua_status_t      original_lua_status      = nullptr;
lua_costatus_t    original_lua_costatus    = nullptr;
lua_isyieldable_t original_lua_isyieldable = nullptr;

// luaL helpers
luaL_checktype_t    original_luaL_checktype    = nullptr;
luaL_tolstring_t    original_luaL_tolstring    = nullptr;
luaL_getmetafield_t original_luaL_getmetafield = nullptr;
luaL_checklstring_t original_luaL_checklstring = nullptr;
luaL_checknumber_t  original_luaL_checknumber  = nullptr;
luaL_checkinteger_t original_luaL_checkinteger = nullptr;
luaL_argerrorL_t    original_luaL_argerrorL    = nullptr;
luaL_errorL_t       original_luaL_errorL       = nullptr;
luaL_typename_t     original_luaL_typename     = nullptr;
luaL_traceback_t    original_luaL_traceback    = nullptr;

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

            #define RESOLVE(var, dll, name) var = (decltype(var))GetFunctionAddress(dll, name)

            // Core VM
            RESOLVE(original_luaL_newstate,  L"Luau.VM.dll", "luaL_newstate");
            RESOLVE(original_lua_pcall,      L"Luau.VM.dll", "lua_pcall");
            RESOLVE(original_lua_call,       L"Luau.VM.dll", "lua_call");
            RESOLVE(original_lua_getfield,   L"Luau.VM.dll", "lua_getfield");
            RESOLVE(original_lua_setfield,   L"Luau.VM.dll", "lua_setfield");
            RESOLVE(original_lua_settop,     L"Luau.VM.dll", "lua_settop");
            RESOLVE(original_lua_gettop,     L"Luau.VM.dll", "lua_gettop");
            RESOLVE(original_lua_absindex,   L"Luau.VM.dll", "lua_absindex");
            RESOLVE(original_luau_load,      L"Luau.VM.dll", "luau_load");
            RESOLVE(original_lua_checkstack, L"Luau.VM.dll", "lua_checkstack");

            // Stack / Type
            RESOLVE(original_lua_type,        L"Luau.VM.dll", "lua_type");
            RESOLVE(original_lua_typename,    L"Luau.VM.dll", "lua_typename");
            RESOLVE(original_lua_equal,       L"Luau.VM.dll", "lua_equal");
            RESOLVE(original_lua_rawequal,    L"Luau.VM.dll", "lua_rawequal");
            RESOLVE(original_lua_lessthan,    L"Luau.VM.dll", "lua_lessthan");
            RESOLVE(original_lua_isnumber,    L"Luau.VM.dll", "lua_isnumber");
            RESOLVE(original_lua_isstring,    L"Luau.VM.dll", "lua_isstring");
            RESOLVE(original_lua_isuserdata,  L"Luau.VM.dll", "lua_isuserdata");
            RESOLVE(original_lua_iscfunction, L"Luau.VM.dll", "lua_iscfunction");
            RESOLVE(original_lua_insert,      L"Luau.VM.dll", "lua_insert");
            RESOLVE(original_lua_replace,     L"Luau.VM.dll", "lua_replace");
            RESOLVE(original_lua_remove,      L"Luau.VM.dll", "lua_remove");
            RESOLVE(original_lua_concat,      L"Luau.VM.dll", "lua_concat");
            RESOLVE(original_lua_next,        L"Luau.VM.dll", "lua_next");
            RESOLVE(original_lua_objlen,      L"Luau.VM.dll", "lua_objlen");

            // Push
            RESOLVE(original_lua_pushnil,       L"Luau.VM.dll", "lua_pushnil");
            RESOLVE(original_lua_pushboolean,   L"Luau.VM.dll", "lua_pushboolean");
            RESOLVE(original_lua_pushinteger,   L"Luau.VM.dll", "lua_pushinteger");
            RESOLVE(original_lua_pushnumber,    L"Luau.VM.dll", "lua_pushnumber");
            RESOLVE(original_lua_pushunsigned,  L"Luau.VM.dll", "lua_pushunsigned");
            RESOLVE(original_lua_pushstring,    L"Luau.VM.dll", "lua_pushstring");
            RESOLVE(original_lua_pushlstring,   L"Luau.VM.dll", "lua_pushlstring");
            RESOLVE(original_lua_pushvalue,     L"Luau.VM.dll", "lua_pushvalue");
            RESOLVE(original_lua_pushcclosurek, L"Luau.VM.dll", "lua_pushcclosurek");
            RESOLVE(original_lua_pushthread,    L"Luau.VM.dll", "lua_pushthread");
            RESOLVE(original_lua_error,         L"Luau.VM.dll", "lua_error");

            // Read (to*)
            RESOLVE(original_lua_tolstring,   L"Luau.VM.dll", "lua_tolstring");
            RESOLVE(original_lua_toboolean,   L"Luau.VM.dll", "lua_toboolean");
            RESOLVE(original_lua_tonumberx,   L"Luau.VM.dll", "lua_tonumberx");
            RESOLVE(original_lua_tointegerx,  L"Luau.VM.dll", "lua_tointegerx");
            RESOLVE(original_lua_tounsignedx, L"Luau.VM.dll", "lua_tounsignedx");
            RESOLVE(original_lua_tocfunction, L"Luau.VM.dll", "lua_tocfunction");
            RESOLVE(original_lua_touserdata,  L"Luau.VM.dll", "lua_touserdata");
            RESOLVE(original_lua_tothread,    L"Luau.VM.dll", "lua_tothread");

            // Table
            RESOLVE(original_lua_createtable,  L"Luau.VM.dll", "lua_createtable");
            RESOLVE(original_lua_gettable,     L"Luau.VM.dll", "lua_gettable");
            RESOLVE(original_lua_settable,     L"Luau.VM.dll", "lua_settable");
            RESOLVE(original_lua_rawget,       L"Luau.VM.dll", "lua_rawget");
            RESOLVE(original_lua_rawset,       L"Luau.VM.dll", "lua_rawset");
            RESOLVE(original_lua_rawgeti,      L"Luau.VM.dll", "lua_rawgeti");
            RESOLVE(original_lua_rawseti,      L"Luau.VM.dll", "lua_rawseti");
            RESOLVE(original_lua_rawgetfield,  L"Luau.VM.dll", "lua_rawgetfield");
            RESOLVE(original_lua_rawsetfield,  L"Luau.VM.dll", "lua_rawsetfield");
            RESOLVE(original_lua_getmetatable, L"Luau.VM.dll", "lua_getmetatable");
            RESOLVE(original_lua_setmetatable, L"Luau.VM.dll", "lua_setmetatable");
            RESOLVE(original_lua_getfenv,      L"Luau.VM.dll", "lua_getfenv");
            RESOLVE(original_lua_setfenv,      L"Luau.VM.dll", "lua_setfenv");
            RESOLVE(original_lua_setsafeenv,   L"Luau.VM.dll", "lua_setsafeenv");
            RESOLVE(original_lua_setreadonly,  L"Luau.VM.dll", "lua_setreadonly");
            RESOLVE(original_lua_getreadonly,  L"Luau.VM.dll", "lua_getreadonly");

            // Ref
            RESOLVE(original_luaL_ref,   L"Luau.VM.dll", "luaL_ref");
            RESOLVE(original_luaL_unref, L"Luau.VM.dll", "luaL_unref");
            RESOLVE(original_lua_ref,    L"Luau.VM.dll", "lua_ref");
            RESOLVE(original_lua_unref,  L"Luau.VM.dll", "lua_unref");

            // Coroutine
            RESOLVE(original_lua_newthread,   L"Luau.VM.dll", "lua_newthread");
            RESOLVE(original_lua_xmove,       L"Luau.VM.dll", "lua_xmove");
            RESOLVE(original_lua_resume,      L"Luau.VM.dll", "lua_resume");
            RESOLVE(original_lua_yield,       L"Luau.VM.dll", "lua_yield");
            RESOLVE(original_lua_status,      L"Luau.VM.dll", "lua_status");
            RESOLVE(original_lua_costatus,    L"Luau.VM.dll", "lua_costatus");
            RESOLVE(original_lua_isyieldable, L"Luau.VM.dll", "lua_isyieldable");

            // luaL helpers
            RESOLVE(original_luaL_checktype,    L"Luau.VM.dll", "luaL_checktype");
            RESOLVE(original_luaL_tolstring,    L"Luau.VM.dll", "luaL_tolstring");
            RESOLVE(original_luaL_getmetafield, L"Luau.VM.dll", "luaL_getmetafield");
            RESOLVE(original_luaL_checklstring, L"Luau.VM.dll", "luaL_checklstring");
            RESOLVE(original_luaL_checknumber,  L"Luau.VM.dll", "luaL_checknumber");
            RESOLVE(original_luaL_checkinteger, L"Luau.VM.dll", "luaL_checkinteger");
            RESOLVE(original_luaL_argerrorL,    L"Luau.VM.dll", "luaL_argerrorL");
            RESOLVE(original_luaL_errorL,       L"Luau.VM.dll", "luaL_errorL");
            RESOLVE(original_luaL_typename,     L"Luau.VM.dll", "luaL_typename");
            RESOLVE(original_luaL_traceback,    L"Luau.VM.dll", "luaL_traceback");

            // Compiler
            RESOLVE(original_luau_compile, L"Luau.Compiler.dll", "luau_compile");

            #undef RESOLVE

            if (!original_lua_pcall || !original_lua_getfield || !original_luau_load || !original_luau_compile) {
                Logger::Log("[ERROR] Failed to resolve critical Luau VM/Compiler components!");
                return false;
            }

            if (!original_lua_objlen) {
                Logger::Log("[WARNING] lua_objlen not found — table iteration may be limited.");
            }

            Logger::Log("Luau API initialized successfully.");
            return true;
        }

    }
}
