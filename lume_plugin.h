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
    typedef struct CustomProtocolHandler {
        const char* scheme;
        void* (*fetch_and_parse)(const char* url);
        void (*render_page)(void* ctx, HDC dc, int w, int h, int scroll_y);
        void (*free_page)(void* ctx);
        int (*get_document_height)(void* ctx);
        void (*on_mouse_down)(void* ctx, int x, int y, int button);
        void (*on_mouse_move)(void* ctx, int x, int y);
        void (*on_mouse_wheel)(void* ctx, int delta);
    } CustomProtocolHandler;
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
        void (*p_lua_pushnil)(lua_State* L);
        void (*p_lua_pushvalue)(lua_State* L, int idx);
        void (*p_lua_pushlightuserdata)(lua_State* L, void* p);
        void (*p_lua_setglobal)(lua_State* L, const char* name);
        int (*p_lua_type)(lua_State* L, int index);
        int (*p_lua_rawgeti)(lua_State* L, int index, lua_Integer n);
        lua_Number(*p_lua_tonumberx)(lua_State* L, int index, int* isnum);
        int (*p_lua_toboolean)(lua_State* L, int idx);
        void* (*p_lua_touserdata)(lua_State* L, int idx);
        void (*p_lua_settop)(lua_State* L, int index);
        int (*p_lua_gettop)(lua_State* L);
        void (*p_lua_createtable)(lua_State* L, int narr, int nrec);
        void (*p_lua_setfield)(lua_State* L, int index, const char* k);
        const char* (*get_node_prop)(HTP_NodeHandle node, const char* key, const char* default_val);
        int (*get_node_prop_int)(HTP_NodeHandle node, const char* key, int default_val);
        void (*set_node_prop)(HTP_NodeHandle node, const char* key, const char* val);
        HTP_NodeHandle(*get_dom_root)(void);
        int (*get_node_children_count)(HTP_NodeHandle node);
        HTP_NodeHandle(*get_node_child)(HTP_NodeHandle node, int index);
        void (*register_tag)(CustomTagHandler handler);
        void (*register_page_engine)(CustomPageHandler handler);
        void (*register_protocol_engine)(CustomProtocolHandler handler);
        void (*register_on_reset)(void (*callback)());
        void (*register_script_engine)(const char* lang, void (*exec_fn)(const char* code));
        int (*gl_begin_render)(const char* id, int w, int h);
        void (*gl_end_render)(const char* id);
        void (*gl_place)(const char* id, int x, int y, int w, int h, int scroll_y);
        int (*gl_get_context)(const char* id, HDC* out_hdc, HGLRC* out_hglrc);
        int (*wasm_load)(const char* bytes, size_t len);
        void (*b_set_prop)(const char* id, const char* prop, const char* val);
        void (*b_set_inner_htp)(const char* id, const char* htp);
        void (*b_set_text)(const char* id, const char* t);
        const char* (*b_get_text)(const char* id);
        void (*b_set_offset_y)(const char* id, int o);
        int (*b_get_offset_y)(const char* id);
        void (*b_set_shimmer)(const char* id, float s);
        void (*b_fire_click)(const char* id);
        void (*b_fire_right_click)(const char* id);
        void (*b_register_click_handler)(const char* id, void(*cb)(void*), void* ctx);
        void (*b_register_right_click_handler)(const char* id, void(*cb)(void*), void* ctx);
        int (*b_is_key_pressed)(int vk);
        int (*b_is_key_released)(int vk);
        int (*b_key_down)(int vk);
        void (*b_get_mouse)(int* x, int* y);
        void (*b_get_mouse_delta)(int* dx, int* dy);
        int (*b_get_mouse_wheel)();
        int (*b_capture_mouse)(const char* id, int mode);
        void (*b_get_canvas_mouse)(const char* id, int* x, int* y);
        void (*b_release_mouse)();
        int (*b_is_mouse_captured)();
        void (*b_fullscreen_canvas)(const char* id, int enter);
        int (*b_is_fullscreen)();
        int (*b_window_active)();
        const char* (*b_get_input)(const char* id);
        void (*b_set_input)(const char* id, const char* t);
        void (*b_http_request)(const char* url, char** out_body, int* out_code);
        void (*b_free_string)(char* s);
        void (*b_cv_clear)(const char* id, const char* color_hex);
        void (*b_cv_rect)(const char* id, int x, int y, int w, int h, const char* color_hex);
        void (*b_cv_circle)(const char* id, int cx, int cy, int r, const char* color_hex);
        void (*b_cv_line)(const char* id, int x1, int y1, int x2, int y2, const char* color_hex, int t);
        void (*b_cv_text)(const char* id, int x, int y, const char* txt, int sz, const char* color_hex);
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
#undef lua_pushnil
#undef lua_pushvalue
#undef lua_pushlightuserdata
#undef lua_setglobal
#undef lua_register
#undef lua_createtable
#undef lua_newtable
#undef lua_setfield
#undef lua_type
#undef lua_tonumber
#undef lua_toboolean
#undef lua_touserdata
#undef lua_settop
#undef lua_gettop
#undef lua_pop
#define luaL_checknumber(L, arg) g_api->p_luaL_checknumber(L, arg)
#define luaL_checkinteger(L, arg) g_api->p_luaL_checkinteger(L, arg)
#define luaL_optinteger(L, arg, def) g_api->p_luaL_optinteger(L, arg, def)
#define luaL_optstring(L, arg, def) g_api->p_luaL_optlstring(L, arg, def, NULL)
#define luaL_checkstring(L, arg) g_api->p_luaL_checklstring(L, arg, NULL)
#define lua_pushnumber(L, n) g_api->p_lua_pushnumber(L, n)
#define lua_pushboolean(L, b) g_api->p_lua_pushboolean(L, b)
#define lua_pushstring(L, s) g_api->p_lua_pushstring(L, s)
#define lua_pushcfunction(L, f) g_api->p_lua_pushcclosure(L, f, 0)
#define lua_pushnil(L) g_api->p_lua_pushnil(L)
#define lua_pushvalue(L, idx) g_api->p_lua_pushvalue(L, idx)
#define lua_pushlightuserdata(L, p) g_api->p_lua_pushlightuserdata(L, p)
#define lua_setglobal(L, name) g_api->p_lua_setglobal(L, name)
#define lua_register(L, n, f) (g_api->p_lua_pushcclosure(L, f, 0), g_api->p_lua_setglobal(L, n))
#define lua_createtable(L, narr, nrec) g_api->p_lua_createtable(L, narr, nrec)
#define lua_newtable(L) g_api->p_lua_createtable(L, 0, 0)
#define lua_setfield(L, idx, k) g_api->p_lua_setfield(L, idx, k)
#define lua_type(L, idx) g_api->p_lua_type(L, idx)
#define lua_tonumber(L, idx) g_api->p_lua_tonumberx(L, idx, NULL)
#define lua_toboolean(L, idx) g_api->p_lua_toboolean(L, idx)
#define lua_touserdata(L, idx) g_api->p_lua_touserdata(L, idx)
#define lua_settop(L, idx) g_api->p_lua_settop(L, idx)
#define lua_gettop(L) g_api->p_lua_gettop(L)
#define lua_pop(L, n) g_api->p_lua_settop(L, -(n)-1)
#define lume_set_prop(id, prop, val) g_api->b_set_prop(id, prop, val)
#define lume_set_inner_htp(id, htp) g_api->b_set_inner_htp(id, htp)
#define lume_set_text(id, t) g_api->b_set_text(id, t)
#define lume_get_text(id) g_api->b_get_text(id)
#define lume_set_offset_y(id, o) g_api->b_set_offset_y(id, o)
#define lume_get_offset_y(id) g_api->b_get_offset_y(id)
#define lume_set_shimmer(id, s) g_api->b_set_shimmer(id, s)
#define lume_fire_click(id) g_api->b_fire_click(id)
#define lume_fire_right_click(id) g_api->b_fire_right_click(id)
#define lume_register_click(id, cb, ctx) g_api->b_register_click_handler(id, cb, ctx)
#define lume_register_right_click(id, cb, ctx) g_api->b_register_right_click_handler(id, cb, ctx)
#define lume_is_key_pressed(vk) g_api->b_is_key_pressed(vk)
#define lume_is_key_released(vk) g_api->b_is_key_released(vk)
#define lume_key_down(vk) g_api->b_key_down(vk)
#define lume_get_mouse(x, y) g_api->b_get_mouse(x, y)
#define lume_get_mouse_delta(dx, dy) g_api->b_get_mouse_delta(dx, dy)
#define lume_get_mouse_wheel() g_api->b_get_mouse_wheel()
#define lume_capture_mouse(id, mode) g_api->b_capture_mouse(id, mode)
#define lume_get_canvas_mouse(id, x, y) g_api->b_get_canvas_mouse(id, x, y)
#define lume_release_mouse() g_api->b_release_mouse()
#define lume_is_mouse_captured() g_api->b_is_mouse_captured()
#define lume_fullscreen_canvas(id, enter) g_api->b_fullscreen_canvas(id, enter)
#define lume_is_fullscreen() g_api->b_is_fullscreen()
#define lume_window_active() g_api->b_window_active()
#define lume_navigate(url) g_api->navigate_to(url)
#define lume_alert(msg) g_api->alert(msg)
#define lume_refresh() g_api->invalidate_content()
#define lume_get_input(id) g_api->b_get_input(id)
#define lume_set_input(id, t) g_api->b_set_input(id, t)
#define lume_http_request(url, out_body, out_code) g_api->b_http_request(url, out_body, out_code)
#define lume_free_string(s) g_api->b_free_string(s)
#define lume_cv_clear(id, color) g_api->b_cv_clear(id, color)
#define lume_cv_rect(id, x, y, w, h, color) g_api->b_cv_rect(id, x, y, w, h, color)
#define lume_cv_circle(id, cx, cy, r, color) g_api->b_cv_circle(id, cx, cy, r, color)
#define lume_cv_line(id, x1, y1, x2, y2, color, t) g_api->b_cv_line(id, x1, y1, x2, y2, color, t)
#define lume_cv_text(id, x, y, txt, sz, color) g_api->b_cv_text(id, x, y, txt, sz, color)
#else
#define LUME_PLUGIN_EXPORT __declspec(dllimport)
#endif
    typedef int (*lume_plugin_init_fn)(lua_State* L, LumeHostAPI* api);
#ifdef __cplusplus
}
#endif
#endif
