#define BUILDING_PLUGIN
#include "../lume_plugin.h"
#include <string>
static LumeHostAPI* g_api = nullptr;
static RECT getWorkArea() {
    RECT wa;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    return wa;
}
static void getScreenSize(int* w, int* h) {
    *w = GetSystemMetrics(SM_CXSCREEN);
    *h = GetSystemMetrics(SM_CYSCREEN);
}
static int l_windowResize(lua_State* L) {
    int w = (int)luaL_checkinteger(L, 1);
    int h = (int)luaL_checkinteger(L, 2);
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (hwnd) {
        SetWindowPos(hwnd, nullptr, 0, 0, w, h,
            SWP_NOMOVE | SWP_NOZORDER);
    }
    return 0;
}
static int l_windowMove(lua_State* L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (hwnd) {
        SetWindowPos(hwnd, nullptr, x, y, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER);
    }
    return 0;
}
static int l_windowCenter(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    RECT wa = getWorkArea();
    RECT wr;
    GetWindowRect(hwnd, &wr);
    int ww = wr.right - wr.left;
    int wh = wr.bottom - wr.top;
    int x = wa.left + (wa.right - wa.left - ww) / 2;
    int y = wa.top + (wa.bottom - wa.top - wh) / 2;
    SetWindowPos(hwnd, nullptr, x, y, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER);
    return 0;
}
static int l_windowLeft(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_RESTORE);
    RECT wa = getWorkArea();
    int w = (wa.right - wa.left) / 2;
    int h = wa.bottom - wa.top;
    SetWindowPos(hwnd, nullptr, wa.left, wa.top, w, h,
        SWP_NOZORDER);
    return 0;
}
static int l_windowRight(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_RESTORE);
    RECT wa = getWorkArea();
    int w = (wa.right - wa.left) / 2;
    int h = wa.bottom - wa.top;
    SetWindowPos(hwnd, nullptr, wa.left + w, wa.top, w, h,
        SWP_NOZORDER);
    return 0;
}
static int l_windowTop(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_RESTORE);
    RECT wa = getWorkArea();
    int w = wa.right - wa.left;
    int h = (wa.bottom - wa.top) / 2;
    SetWindowPos(hwnd, nullptr, wa.left, wa.top, w, h,
        SWP_NOZORDER);
    return 0;
}
static int l_windowBottom(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_RESTORE);
    RECT wa = getWorkArea();
    int w = wa.right - wa.left;
    int h = (wa.bottom - wa.top) / 2;
    SetWindowPos(hwnd, nullptr, wa.left, wa.top + h, w, h,
        SWP_NOZORDER);
    return 0;
}
static int l_windowTopLeft(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_RESTORE);
    RECT wa = getWorkArea();
    int w = (wa.right - wa.left) / 2;
    int h = (wa.bottom - wa.top) / 2;
    SetWindowPos(hwnd, nullptr, wa.left, wa.top, w, h,
        SWP_NOZORDER);
    return 0;
}
static int l_windowTopRight(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_RESTORE);
    RECT wa = getWorkArea();
    int w = (wa.right - wa.left) / 2;
    int h = (wa.bottom - wa.top) / 2;
    SetWindowPos(hwnd, nullptr, wa.left + w, wa.top, w, h,
        SWP_NOZORDER);
    return 0;
}
static int l_windowBottomLeft(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_RESTORE);
    RECT wa = getWorkArea();
    int w = (wa.right - wa.left) / 2;
    int h = (wa.bottom - wa.top) / 2;
    SetWindowPos(hwnd, nullptr, wa.left, wa.top + h, w, h,
        SWP_NOZORDER);
    return 0;
}
static int l_windowBottomRight(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    ShowWindow(hwnd, SW_RESTORE);
    RECT wa = getWorkArea();
    int w = (wa.right - wa.left) / 2;
    int h = (wa.bottom - wa.top) / 2;
    SetWindowPos(hwnd, nullptr, wa.left + w, wa.top + h, w, h,
        SWP_NOZORDER);
    return 0;
}
static int l_windowMaximize(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (hwnd) ShowWindow(hwnd, SW_MAXIMIZE);
    return 0;
}
static int l_windowMinimize(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (hwnd) ShowWindow(hwnd, SW_MINIMIZE);
    return 0;
}
static int l_windowRestore(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (hwnd) ShowWindow(hwnd, SW_RESTORE);
    return 0;
}
static LONG g_savedStyle = 0;
static RECT g_savedRect = {};
static bool g_isFullscreen = false;
static int l_windowFullscreen(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (!hwnd) return 0;
    if (!g_isFullscreen) {
        g_savedStyle = GetWindowLongA(hwnd, GWL_STYLE);
        GetWindowRect(hwnd, &g_savedRect);
        int sw, sh;
        getScreenSize(&sw, &sh);
        SetWindowLongA(hwnd, GWL_STYLE,
            g_savedStyle & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowPos(hwnd, HWND_TOP, 0, 0, sw, sh,
            SWP_FRAMECHANGED);
        g_isFullscreen = true;
    } else {
        SetWindowLongA(hwnd, GWL_STYLE, g_savedStyle);
        SetWindowPos(hwnd, nullptr,
            g_savedRect.left, g_savedRect.top,
            g_savedRect.right - g_savedRect.left,
            g_savedRect.bottom - g_savedRect.top,
            SWP_NOZORDER | SWP_FRAMECHANGED);
        g_isFullscreen = false;
    }
    return 0;
}
static int l_windowSetTitle(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (hwnd) SetWindowTextA(hwnd, title);
    return 0;
}
static int l_windowGetSize(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (hwnd) {
        RECT r;
        GetWindowRect(hwnd, &r);
        lua_pushinteger(L, r.right - r.left);
        lua_pushinteger(L, r.bottom - r.top);
    } else {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
    }
    return 2;
}
static int l_windowGetPos(lua_State* L) {
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    if (hwnd) {
        RECT r;
        GetWindowRect(hwnd, &r);
        lua_pushinteger(L, r.left);
        lua_pushinteger(L, r.top);
    } else {
        lua_pushinteger(L, 0);
        lua_pushinteger(L, 0);
    }
    return 2;
}
static int l_windowGetMonitorSize(lua_State* L) {
    int w, h;
    getScreenSize(&w, &h);
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}
extern "C" __declspec(dllexport)
int lume_plugin_init(lua_State* L, LumeHostAPI* api) {
    g_api = api;
    lua_register(L, "windowResize",         l_windowResize);
    lua_register(L, "windowMove",           l_windowMove);
    lua_register(L, "windowCenter",         l_windowCenter);
    lua_register(L, "windowLeft",           l_windowLeft);
    lua_register(L, "windowRight",          l_windowRight);
    lua_register(L, "windowTop",            l_windowTop);
    lua_register(L, "windowBottom",         l_windowBottom);
    lua_register(L, "windowTopLeft",        l_windowTopLeft);
    lua_register(L, "windowTopRight",       l_windowTopRight);
    lua_register(L, "windowBottomLeft",     l_windowBottomLeft);
    lua_register(L, "windowBottomRight",    l_windowBottomRight);
    lua_register(L, "windowMaximize",       l_windowMaximize);
    lua_register(L, "windowMinimize",       l_windowMinimize);
    lua_register(L, "windowRestore",        l_windowRestore);
    lua_register(L, "windowFullscreen",     l_windowFullscreen);
    lua_register(L, "windowSetTitle",       l_windowSetTitle);
    lua_register(L, "windowGetSize",        l_windowGetSize);
    lua_register(L, "windowGetPos",         l_windowGetPos);
    lua_register(L, "windowGetMonitorSize", l_windowGetMonitorSize);
    OutputDebugStringA("[Plugin] window_plugin loaded\n");
    return 0;
}