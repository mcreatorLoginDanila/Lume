#ifndef LUME_PLUGIN_H
#define LUME_PLUGIN_H
#ifdef __cplusplus
extern "C" {
#endif
#include "lib/lua.h"
#include "lib/lauxlib.h"
#include "lib/lualib.h"
#ifdef __cplusplus
}
#endif
#include <windows.h>
typedef struct LumeHostAPI {
    HWND(*get_main_hwnd)(void);
    void (*invalidate_content)(void);
    void (*set_status)(const char* text);
    void (*navigate_to)(const char* url);
    int api_version;
    lua_Number(*p_luaL_checknumber)(lua_State* L, int arg);
    lua_Integer(*p_luaL_checkinteger)(lua_State* L, int arg);
    lua_Integer(*p_luaL_optinteger)(lua_State* L, int arg, lua_Integer def);
    const char* (*p_luaL_optlstring)(lua_State* L, int arg, const char* def, size_t* l);
    const char* (*p_luaL_checklstring)(lua_State* L, int arg, size_t* l);
    void        (*p_lua_pushnumber)(lua_State* L, lua_Number n);
    void        (*p_lua_pushboolean)(lua_State* L, int b);
    const char* (*p_lua_pushstring)(lua_State* L, const char* s);
    void        (*p_lua_pushcclosure)(lua_State* L, lua_CFunction fn, int n);
    void        (*p_lua_setglobal)(lua_State* L, const char* name);
} LumeHostAPI;
#ifdef BUILDING_PLUGIN
#define LUME_PLUGIN_EXPORT __declspec(dllexport)
extern LumeHostAPI* g_api;
#undef luaL_checknumber
#undef luaL_checkinteger
#undef luaL_optinteger
#undef luaL_optstring
#undef luaL_checkstring
#undef lua_pushnumber
#undef lua_pushboolean
#undef lua_pushstring
#undef lua_pushcfunction
#undef lua_setglobal
#undef lua_register
#define luaL_checknumber(L, arg)        g_api->p_luaL_checknumber(L, arg)
#define luaL_checkinteger(L, arg)       g_api->p_luaL_checkinteger(L, arg)
#define luaL_optinteger(L, arg, def)    g_api->p_luaL_optinteger(L, arg, def)
#define luaL_optstring(L, arg, def)     g_api->p_luaL_optlstring(L, arg, def, NULL)
#define luaL_checkstring(L, arg)        g_api->p_luaL_checklstring(L, arg, NULL)
#define lua_pushnumber(L, n)            g_api->p_lua_pushnumber(L, n)
#define lua_pushboolean(L, b)           g_api->p_lua_pushboolean(L, b)
#define lua_pushstring(L, s)            g_api->p_lua_pushstring(L, s)
#define lua_pushcfunction(L, f)         g_api->p_lua_pushcclosure(L, f, 0)
#define lua_setglobal(L, name)          g_api->p_lua_setglobal(L, name)
#define lua_register(L, n, f)           (g_api->p_lua_pushcclosure(L, f, 0), g_api->p_lua_setglobal(L, n))
#else
#define LUME_PLUGIN_EXPORT __declspec(dllimport)
#endif
typedef int (*lume_plugin_init_fn)(lua_State* L, LumeHostAPI* api);
#endif
