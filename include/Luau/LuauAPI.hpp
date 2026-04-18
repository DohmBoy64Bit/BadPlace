#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstdint>

// Forward declare lua_State as an opaque struct pointer
typedef struct lua_State lua_State;

// Standard types
typedef int (*lua_CFunction)(lua_State *L);

// --- Luau-specific Constants ---
#define LUA_REGISTRYINDEX -10000
#define LUA_ENVIRONINDEX  -10001
#define LUA_GLOBALSINDEX  -10002

#define LUA_TNIL          0
#define LUA_TBOOLEAN      1
#define LUA_TLIGHTUSERDATA 2
#define LUA_TNUMBER       3
#define LUA_TVECTOR       4
#define LUA_TSTRING       5
#define LUA_TTABLE        6
#define LUA_TFUNCTION     7
#define LUA_TUSERDATA     8
#define LUA_TTHREAD       9

#define LUA_NOREF  (-2)
#define LUA_REFNIL (-1)

// =============================================================================
// Function pointer typedefs
// =============================================================================

// --- Core VM ---
typedef lua_State* (*luaL_newstate_t)();
typedef int        (*lua_pcall_t)(lua_State* L, int nargs, int nresults, int errfunc);
typedef void       (*lua_call_t)(lua_State* L, int nargs, int nresults);
typedef int        (*lua_getfield_t)(lua_State* L, int idx, const char* k);
typedef void       (*lua_setfield_t)(lua_State* L, int idx, const char* k);
typedef void       (*lua_settop_t)(lua_State* L, int idx);
typedef int        (*lua_gettop_t)(lua_State* L);
typedef int        (*lua_absindex_t)(lua_State* L, int idx);
typedef int        (*lua_checkstack_t)(lua_State* L, int sz);
typedef int        (*luau_load_t)(lua_State* L, const char* chunkname, const char* data, size_t size, int env);

// --- Compiler ---
typedef char* (*luau_compile_t)(const char* source, size_t size, void* options, size_t* outsize);

// --- Stack / Type Utilities ---
typedef int         (*lua_type_t)(lua_State* L, int idx);
typedef const char* (*lua_typename_t)(lua_State* L, int tp);
typedef int         (*lua_equal_t)(lua_State* L, int idx1, int idx2);
typedef int         (*lua_rawequal_t)(lua_State* L, int idx1, int idx2);
typedef int         (*lua_lessthan_t)(lua_State* L, int idx1, int idx2);
typedef int         (*lua_isnumber_t)(lua_State* L, int idx);
typedef int         (*lua_isstring_t)(lua_State* L, int idx);
typedef int         (*lua_isuserdata_t)(lua_State* L, int idx);
typedef int         (*lua_iscfunction_t)(lua_State* L, int idx);
typedef void        (*lua_insert_t)(lua_State* L, int idx);
typedef void        (*lua_replace_t)(lua_State* L, int idx);
typedef void        (*lua_remove_t)(lua_State* L, int idx);
typedef void        (*lua_concat_t)(lua_State* L, int n);
typedef int         (*lua_next_t)(lua_State* L, int idx);
typedef int         (*lua_objlen_t)(lua_State* L, int idx);

// --- Push ---
typedef void        (*lua_pushnil_t)(lua_State* L);
typedef void        (*lua_pushboolean_t)(lua_State* L, int b);
typedef void        (*lua_pushinteger_t)(lua_State* L, int n);
typedef void        (*lua_pushnumber_t)(lua_State* L, double n);
typedef void        (*lua_pushunsigned_t)(lua_State* L, unsigned n);
typedef void        (*lua_pushstring_t)(lua_State* L, const char* s);
typedef const char* (*lua_pushlstring_t)(lua_State* L, const char* s, size_t l);
typedef void        (*lua_pushvalue_t)(lua_State* L, int idx);
typedef void        (*lua_pushcclosurek_t)(lua_State* L, lua_CFunction fn, const char* debugname, int nup, void* cont);
typedef int         (*lua_pushthread_t)(lua_State* L);
typedef void        (*lua_error_t)(lua_State* L);

// --- Read (to*) ---
typedef const char*   (*lua_tolstring_t)(lua_State* L, int idx, size_t* len);
typedef int           (*lua_toboolean_t)(lua_State* L, int idx);
typedef double        (*lua_tonumberx_t)(lua_State* L, int idx, int* isnum);
typedef int           (*lua_tointegerx_t)(lua_State* L, int idx, int* isnum);
typedef unsigned      (*lua_tounsignedx_t)(lua_State* L, int idx, int* isnum);
typedef lua_CFunction (*lua_tocfunction_t)(lua_State* L, int idx);
typedef void*         (*lua_touserdata_t)(lua_State* L, int idx);
typedef lua_State*    (*lua_tothread_t)(lua_State* L, int idx);

// --- Table ---
typedef void (*lua_createtable_t)(lua_State* L, int narr, int nrec);
typedef void (*lua_gettable_t)(lua_State* L, int idx);
typedef void (*lua_settable_t)(lua_State* L, int idx);
typedef void (*lua_rawget_t)(lua_State* L, int idx);
typedef void (*lua_rawset_t)(lua_State* L, int idx);
typedef void (*lua_rawgeti_t)(lua_State* L, int idx, int n);
typedef void (*lua_rawseti_t)(lua_State* L, int idx, int n);
typedef int  (*lua_rawgetfield_t)(lua_State* L, int idx, const char* k);
typedef void (*lua_rawsetfield_t)(lua_State* L, int idx, const char* k);
typedef int  (*lua_getmetatable_t)(lua_State* L, int idx);
typedef int  (*lua_setmetatable_t)(lua_State* L, int idx);
typedef int  (*lua_getfenv_t)(lua_State* L, int idx);
typedef int  (*lua_setfenv_t)(lua_State* L, int idx);
typedef void (*lua_setsafeenv_t)(lua_State* L, int idx, int enabled);
typedef void (*lua_setreadonly_t)(lua_State* L, int idx, int enabled);
typedef int  (*lua_getreadonly_t)(lua_State* L, int idx);

// --- Ref ---
typedef int  (*luaL_ref_t)(lua_State* L, int idx);
typedef void (*luaL_unref_t)(lua_State* L, int idx, int ref);
typedef int  (*lua_ref_t)(lua_State* L, int idx);
typedef void (*lua_unref_t)(lua_State* L, int idx, int ref);

// --- Coroutine ---
typedef lua_State* (*lua_newthread_t)(lua_State* L);
typedef void       (*lua_xmove_t)(lua_State* from, lua_State* to, int n);
typedef int        (*lua_resume_t)(lua_State* L, lua_State* from, int narg);
typedef int        (*lua_yield_t)(lua_State* L, int nresults);
typedef int        (*lua_status_t)(lua_State* L);
typedef int        (*lua_costatus_t)(lua_State* L, lua_State* co);
typedef int        (*lua_isyieldable_t)(lua_State* L);

// --- luaL helpers ---
typedef void        (*luaL_checktype_t)(lua_State* L, int narg, int t);
typedef const char* (*luaL_tolstring_t)(lua_State* L, int idx, size_t* len);
typedef int         (*luaL_getmetafield_t)(lua_State* L, int obj, const char* e);
typedef const char* (*luaL_checklstring_t)(lua_State* L, int numArg, size_t* l);
typedef double      (*luaL_checknumber_t)(lua_State* L, int numArg);
typedef int         (*luaL_checkinteger_t)(lua_State* L, int numArg);
typedef void        (*luaL_argerrorL_t)(lua_State* L, int narg, const char* extramsg);
typedef void        (*luaL_errorL_t)(lua_State* L, const char* fmt, ...);
typedef const char* (*luaL_typename_t)(lua_State* L, int idx);
typedef void        (*luaL_traceback_t)(lua_State* L, lua_State* L1, const char* msg, int level);

// =============================================================================
// Extern globals
// =============================================================================

// Core
extern luaL_newstate_t  original_luaL_newstate;
extern lua_pcall_t      original_lua_pcall;
extern lua_call_t       original_lua_call;
extern lua_getfield_t   original_lua_getfield;
extern lua_setfield_t   original_lua_setfield;
extern lua_settop_t     original_lua_settop;
extern lua_gettop_t     original_lua_gettop;
extern lua_absindex_t   original_lua_absindex;
extern luau_load_t      original_luau_load;
extern lua_checkstack_t original_lua_checkstack;
extern luau_compile_t   original_luau_compile;

// Stack / Type utilities
extern lua_type_t        original_lua_type;
extern lua_typename_t    original_lua_typename;
extern lua_equal_t       original_lua_equal;
extern lua_rawequal_t    original_lua_rawequal;
extern lua_lessthan_t    original_lua_lessthan;
extern lua_isnumber_t    original_lua_isnumber;
extern lua_isstring_t    original_lua_isstring;
extern lua_isuserdata_t  original_lua_isuserdata;
extern lua_iscfunction_t original_lua_iscfunction;
extern lua_insert_t      original_lua_insert;
extern lua_replace_t     original_lua_replace;
extern lua_remove_t      original_lua_remove;
extern lua_concat_t      original_lua_concat;
extern lua_next_t        original_lua_next;
extern lua_objlen_t      original_lua_objlen;

// Push
extern lua_pushnil_t       original_lua_pushnil;
extern lua_pushboolean_t   original_lua_pushboolean;
extern lua_pushinteger_t   original_lua_pushinteger;
extern lua_pushnumber_t    original_lua_pushnumber;
extern lua_pushunsigned_t  original_lua_pushunsigned;
extern lua_pushstring_t    original_lua_pushstring;
extern lua_pushlstring_t   original_lua_pushlstring;
extern lua_pushvalue_t     original_lua_pushvalue;
extern lua_pushcclosurek_t original_lua_pushcclosurek;
extern lua_pushthread_t    original_lua_pushthread;
extern lua_error_t         original_lua_error;

// Read (to*)
extern lua_tolstring_t   original_lua_tolstring;
extern lua_toboolean_t   original_lua_toboolean;
extern lua_tonumberx_t   original_lua_tonumberx;
extern lua_tointegerx_t  original_lua_tointegerx;
extern lua_tounsignedx_t original_lua_tounsignedx;
extern lua_tocfunction_t original_lua_tocfunction;
extern lua_touserdata_t  original_lua_touserdata;
extern lua_tothread_t    original_lua_tothread;

// Table
extern lua_createtable_t  original_lua_createtable;
extern lua_gettable_t     original_lua_gettable;
extern lua_settable_t     original_lua_settable;
extern lua_rawget_t       original_lua_rawget;
extern lua_rawset_t       original_lua_rawset;
extern lua_rawgeti_t      original_lua_rawgeti;
extern lua_rawseti_t      original_lua_rawseti;
extern lua_rawgetfield_t  original_lua_rawgetfield;
extern lua_rawsetfield_t  original_lua_rawsetfield;
extern lua_getmetatable_t original_lua_getmetatable;
extern lua_setmetatable_t original_lua_setmetatable;
extern lua_getfenv_t      original_lua_getfenv;
extern lua_setfenv_t      original_lua_setfenv;
extern lua_setsafeenv_t   original_lua_setsafeenv;
extern lua_setreadonly_t  original_lua_setreadonly;
extern lua_getreadonly_t  original_lua_getreadonly;

// Ref
extern luaL_ref_t   original_luaL_ref;
extern luaL_unref_t original_luaL_unref;
extern lua_ref_t    original_lua_ref;
extern lua_unref_t  original_lua_unref;

// Coroutine
extern lua_newthread_t   original_lua_newthread;
extern lua_xmove_t       original_lua_xmove;
extern lua_resume_t      original_lua_resume;
extern lua_yield_t       original_lua_yield;
extern lua_status_t      original_lua_status;
extern lua_costatus_t    original_lua_costatus;
extern lua_isyieldable_t original_lua_isyieldable;

// luaL helpers
extern luaL_checktype_t    original_luaL_checktype;
extern luaL_tolstring_t    original_luaL_tolstring;
extern luaL_getmetafield_t original_luaL_getmetafield;
extern luaL_checklstring_t original_luaL_checklstring;
extern luaL_checknumber_t  original_luaL_checknumber;
extern luaL_checkinteger_t original_luaL_checkinteger;
extern luaL_argerrorL_t    original_luaL_argerrorL;
extern luaL_errorL_t       original_luaL_errorL;
extern luaL_typename_t     original_luaL_typename;
extern luaL_traceback_t    original_luaL_traceback;

namespace BadPlace {
    namespace LuauAPI {
        bool Initialize();
        uintptr_t GetFunctionAddress(const wchar_t* dll_name, const char* function_name);
    }
}
