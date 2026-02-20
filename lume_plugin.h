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
    HWND (*get_main_hwnd)(void);
    void (*invalidate_content)(void);
    void (*set_status)(const char* text);
    void (*navigate_to)(const char* url);
    int api_version;
} LumeHostAPI;
#ifdef BUILDING_PLUGIN
#define LUME_PLUGIN_EXPORT __declspec(dllexport)
#else
#define LUME_PLUGIN_EXPORT __declspec(dllimport)
#endif
typedef int (*lume_plugin_init_fn)(lua_State* L, LumeHostAPI* api);
#endif
