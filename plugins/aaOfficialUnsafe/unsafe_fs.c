#define BUILDING_PLUGIN
#include "lume_plugin.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
LumeHostAPI* g_api = NULL;
void get_workspace_path(char* out_path, const char* filename) {
    char exe_path[MAX_PATH];
    GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    char* last_slash = strrchr(exe_path, '\\');
    if (last_slash) *(last_slash + 1) = '\0';
    strcat(exe_path, "workspace\\");
    CreateDirectoryA(exe_path, NULL);
    strcpy(out_path, exe_path);
    if (filename) {
        strcat(out_path, filename);
    }
}
static int l_fs_write(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    const char* content = luaL_checkstring(L, 2);
    char path[MAX_PATH];
    get_workspace_path(path, filename);
    FILE* f = fopen(path, "wb");
    if (!f) {
        lua_pushboolean(L, 0);
        return 1;
    }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    lua_pushboolean(L, 1);
    return 1;
}
static int l_fs_read(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    char path[MAX_PATH];
    get_workspace_path(path, filename);
    FILE* f = fopen(path, "rb");
    if (!f) {
        lua_pushstring(L, "");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        lua_pushstring(L, "");
        return 1;
    }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    lua_pushstring(L, buf);
    free(buf);
    return 1;
}
static int l_fs_execute(lua_State* L) {
    const char* filename = luaL_checkstring(L, 1);
    const char* args = luaL_optstring(L, 2, "");
    char path[MAX_PATH];
    get_workspace_path(path, filename);
    HINSTANCE res = ShellExecuteA(NULL, "open", path, args, NULL, SW_SHOWNORMAL);
    lua_pushboolean(L, (intptr_t)res > 32 ? 1 : 0);
    return 1;
}
LUME_PLUGIN_EXPORT int lume_plugin_init(lua_State* L, LumeHostAPI* api) {
    g_api = api;
    lua_register(L, "fs_write", l_fs_write);
    lua_register(L, "fs_read", l_fs_read);
    lua_register(L, "fs_execute", l_fs_execute);
    if (g_api->set_status) {
        g_api->set_status("WARNING: Unsafe FS Plugin Active!");
    }
    return 0;
}
LUME_PLUGIN_EXPORT void lume_plugin_shutdown() {
}