/*
 * Copyright (C) 2026 mcreatorLoginDanila also known as NotAndrey
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef LUME_PLUGIN_H
#define LUME_PLUGIN_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "lib/lua.h"
#include "lib/lauxlib.h"
#include "lib/lualib.h"
    typedef void* HTP_NodeHandle;
    typedef struct {
        const char* tag_name;
        int (*estimate_width)(HTP_NodeHandle node, int max_width);
        int (*estimate_height)(HTP_NodeHandle node);
        void (*render)(HDC hdc, HTP_NodeHandle node, int x, int y, int width, int height, int scroll_y);
        void (*on_click)(HTP_NodeHandle node, int local_x, int local_y, int button);
    } CustomTagHandler;
    typedef struct {
        const char* extension;
        void* (*load_page)(const char* url, const char* raw_data, size_t data_length);
        void (*free_page)(void* page_context);
        void (*render_page)(void* page_context, HDC hdc, int window_w, int window_h, int scroll_y);
        int (*get_document_height)(void* page_context);
        void (*on_mouse_down)(void* page_context, int x, int y, int button);
        void (*on_key_down)(void* page_context, int vk);
        void (*on_mouse_move)(void* page_context, int x, int y);
        void (*on_mouse_wheel)(void* page_context, int delta);
        void (*on_char)(void* page_context, unsigned int ch);
    } CustomPageHandler;
    struct CustomProtocolHandler {
        const char* scheme;
        void* (*fetch_and_parse)(const char* url);
        void (*render_page)(void* ctx, HDC dc, int w, int h, int scroll_y);
        void (*free_page)(void* ctx);
        int (*get_document_height)(void* ctx);
        void (*on_mouse_down)(void* ctx, int x, int y, int button);
        void (*on_mouse_move)(void* ctx, int x, int y);
        void (*on_mouse_wheel)(void* ctx, int delta);
    };
    typedef struct LumeHostAPI {
        HWND(*get_main_hwnd)(void);
        void (*invalidate_content)(void);
        void (*set_status)(const char* text);
        void (*navigate_to)(const char* url);
        void (*alert)(const char* msg);
        int api_version;
        lua_Number(*p_luaL_checknumber)(lua_State* L, int arg);
        lua_Integer(*p_luaL_checkinteger)(lua_State* L, int arg);
        lua_Integer(*p_luaL_optinteger)(lua_State* L, int arg, lua_Integer def);
        lua_Number(*p_luaL_optnumber)(lua_State* L, int arg, lua_Number def);
        const char* (*p_luaL_optlstring)(lua_State* L, int arg, const char* def, size_t* l);
        const char* (*p_luaL_checklstring)(lua_State* L, int arg, size_t* l);
        void (*p_lua_pushnumber)(lua_State* L, lua_Number n);
        void (*p_lua_pushboolean)(lua_State* L, int b);
        const char* (*p_lua_pushstring)(lua_State* L, const char* s);
        void (*p_lua_pushcclosure)(lua_State* L, lua_CFunction fn, int n);
        void (*p_lua_setglobal)(lua_State* L, const char* name);
        int (*p_lua_type)(lua_State* L, int index);
        int (*p_lua_rawgeti)(lua_State* L, int index, lua_Integer n);
        lua_Number(*p_lua_tonumberx)(lua_State* L, int index, int* isnum);
        void (*p_lua_settop)(lua_State* L, int index);
        void (*p_lua_createtable)(lua_State* L, int narr, int nrec);
        void (*p_lua_setfield)(lua_State* L, int index, const char* k);
        const char* (*get_node_prop)(HTP_NodeHandle node, const char* key, const char* default_val);
        int (*get_node_prop_int)(HTP_NodeHandle node, const char* key, int default_val);
        void (*register_tag)(CustomTagHandler handler);
        void (*register_page_engine)(CustomPageHandler handler);
        int (*gl_begin_render)(const char* id, int w, int h);
        void (*gl_end_render)(const char* id);
        void (*gl_place)(const char* id, int x, int y, int w, int h, int scroll_y);
        int (*wasm_load)(const char* bytes, size_t len);
        void (*register_on_reset)(void (*callback)());
        void (*register_protocol_engine)(CustomProtocolHandler handler);
        int (*gl_get_context)(const char* id, HDC* out_hdc, HGLRC* out_hglrc);
        HTP_NodeHandle(*get_dom_root)(void);
        int (*get_node_children_count)(HTP_NodeHandle node);
        HTP_NodeHandle(*get_node_child)(HTP_NodeHandle node, int index);
        void (*set_node_prop)(HTP_NodeHandle node, const char* key, const char* val);
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
#undef lua_createtable
#undef lua_newtable
#undef lua_setfield
#define luaL_checknumber(L, arg) g_api->p_luaL_checknumber(L, arg)
#define luaL_checkinteger(L, arg) g_api->p_luaL_checkinteger(L, arg)
#define luaL_optinteger(L, arg, def) g_api->p_luaL_optinteger(L, arg, def)
#define luaL_optstring(L, arg, def) g_api->p_luaL_optlstring(L, arg, def, NULL)
#define luaL_checkstring(L, arg) g_api->p_luaL_checklstring(L, arg, NULL)
#define lua_pushnumber(L, n) g_api->p_lua_pushnumber(L, n)
#define lua_pushboolean(L, b) g_api->p_lua_pushboolean(L, b)
#define lua_pushstring(L, s) g_api->p_lua_pushstring(L, s)
#define lua_pushcfunction(L, f) g_api->p_lua_pushcclosure(L, f, 0)
#define lua_setglobal(L, name) g_api->p_lua_setglobal(L, name)
#define lua_register(L, n, f) (g_api->p_lua_pushcclosure(L, f, 0), g_api->p_lua_setglobal(L, n))
#define lua_createtable(L, narr, nrec) g_api->p_lua_createtable(L, narr, nrec)
#define lua_newtable(L) g_api->p_lua_createtable(L, 0, 0)
#define lua_setfield(L, idx, k) g_api->p_lua_setfield(L, idx, k)
#else
#define LUME_PLUGIN_EXPORT __declspec(dllimport)
#endif
    typedef int (*lume_plugin_init_fn)(lua_State* L, LumeHostAPI* api);
#ifdef __cplusplus
}
#endif
#endif
