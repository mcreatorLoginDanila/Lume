#define BUILDING_PLUGIN
#include "../lume_plugin.h"
#include <string>
static LumeHostAPI* g_api = nullptr;
static std::wstring utf8_to_wide(const char* utf8) {
    if (!utf8 || !*utf8) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], len);
    if (!result.empty() && result.back() == L'\0')
        result.pop_back();
    return result;
}
static int l_msgBox(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const char* text  = luaL_checkstring(L, 2);
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    bool has_table = lua_istable(L, 3);
    bool has_yes = false;
    bool has_no  = false;
    bool has_ok  = false;
    if (has_table) {
        lua_getfield(L, 3, "yes");
        has_yes = lua_isfunction(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 3, "no");
        has_no = lua_isfunction(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 3, "ok");
        has_ok = lua_isfunction(L, -1);
        lua_pop(L, 1);
    }
    std::wstring wtitle = utf8_to_wide(title);
    std::wstring wtext  = utf8_to_wide(text);
    int result;
    if (has_yes || has_no) {
        result = MessageBoxW(
            hwnd,
            wtext.c_str(),
            wtitle.c_str(),
            MB_YESNO | MB_ICONQUESTION
        );
        if (result == IDYES && has_yes && has_table) {
            lua_getfield(L, 3, "yes");
            if (lua_pcall(L, 0, 0, 0) != 0) {
                const char* err = lua_tostring(L, -1);
                OutputDebugStringA(err ? err : "msgBox yes callback error");
                OutputDebugStringA("\n");
                lua_pop(L, 1);
            }
        } else if (result == IDNO && has_no && has_table) {
            lua_getfield(L, 3, "no");
            if (lua_pcall(L, 0, 0, 0) != 0) {
                const char* err = lua_tostring(L, -1);
                OutputDebugStringA(err ? err : "msgBox no callback error");
                OutputDebugStringA("\n");
                lua_pop(L, 1);
            }
        }
    } else {
        result = MessageBoxW(
            hwnd,
            wtext.c_str(),
            wtitle.c_str(),
            MB_OK | MB_ICONINFORMATION
        );
        if (has_ok && has_table) {
            lua_getfield(L, 3, "ok");
            if (lua_pcall(L, 0, 0, 0) != 0) {
                const char* err = lua_tostring(L, -1);
                OutputDebugStringA(err ? err : "msgBox ok callback error");
                OutputDebugStringA("\n");
                lua_pop(L, 1);
            }
        }
    }
    if (result == IDYES)      lua_pushstring(L, "yes");
    else if (result == IDNO)  lua_pushstring(L, "no");
    else                      lua_pushstring(L, "ok");
    return 1;
}
static int l_msgBoxYesNoCancel(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const char* text  = luaL_checkstring(L, 2);
    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    bool has_table = lua_istable(L, 3);
    std::wstring wtitle = utf8_to_wide(title);
    std::wstring wtext  = utf8_to_wide(text);
    int result = MessageBoxW(
        hwnd,
        wtext.c_str(),
        wtitle.c_str(),
        MB_YESNOCANCEL | MB_ICONQUESTION
    );
    if (has_table) {
        const char* field = nullptr;
        if (result == IDYES)         field = "yes";
        else if (result == IDNO)     field = "no";
        else if (result == IDCANCEL) field = "cancel";
        if (field) {
            lua_getfield(L, 3, field);
            if (lua_isfunction(L, -1)) {
                if (lua_pcall(L, 0, 0, 0) != 0) {
                    const char* err = lua_tostring(L, -1);
                    OutputDebugStringA(err ? err : "msgBox callback error");
                    lua_pop(L, 1);
                }
            } else {
                lua_pop(L, 1);
            }
        }
    }
    if (result == IDYES)           lua_pushstring(L, "yes");
    else if (result == IDNO)       lua_pushstring(L, "no");
    else if (result == IDCANCEL)   lua_pushstring(L, "cancel");
    else                           lua_pushstring(L, "unknown");
    return 1;
}
struct InputBoxData {
    std::wstring title;
    std::wstring prompt;
    std::wstring defaultText;
    std::wstring result;
    bool accepted;
};
static INT_PTR CALLBACK InputBoxDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* data = reinterpret_cast<InputBoxData*>(lp);
        SetWindowLongPtrW(hDlg, GWLP_USERDATA, (LONG_PTR)data);
        SetWindowTextW(hDlg, data->title.c_str());
        CreateWindowW(L"STATIC", data->prompt.c_str(),
            WS_CHILD | WS_VISIBLE,
            10, 10, 360, 20, hDlg, nullptr,
            nullptr, nullptr);
        HWND hEdit = CreateWindowW(L"EDIT", data->defaultText.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 35, 360, 24, hDlg, (HMENU)200,
            nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            210, 70, 75, 28, hDlg, (HMENU)IDOK,
            nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE,
            295, 70, 75, 28, hDlg, (HMENU)IDCANCEL,
            nullptr, nullptr);
        SetFocus(hEdit);
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            auto* data = reinterpret_cast<InputBoxData*>(
                GetWindowLongPtrW(hDlg, GWLP_USERDATA));
            wchar_t buf[4096] = {};
            GetDlgItemTextW(hDlg, 200, buf, 4096);
            data->result = buf;
            data->accepted = true;
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL) {
            auto* data = reinterpret_cast<InputBoxData*>(
                GetWindowLongPtrW(hDlg, GWLP_USERDATA));
            data->accepted = false;
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
static int l_inputBox(lua_State* L) {
    const char* title   = luaL_checkstring(L, 1);
    const char* prompt  = luaL_checkstring(L, 2);
    const char* deftext = luaL_optstring(L, 3, "");
    InputBoxData data;
    data.title       = utf8_to_wide(title);
    data.prompt      = utf8_to_wide(prompt);
    data.defaultText = utf8_to_wide(deftext);
    data.accepted    = false;

    HWND hwnd = g_api ? g_api->get_main_hwnd() : nullptr;
    #pragma pack(push, 4)
    struct {
        DWORD style;
        DWORD dwExtendedStyle;
        WORD  cdit;
        short x, y, cx, cy;
        WORD  menu;
        WORD  windowClass;
        WORD  title;
    } dlgTemplate = {};
    #pragma pack(pop)
    dlgTemplate.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlgTemplate.cx = 200;
    dlgTemplate.cy = 70;
    INT_PTR res = DialogBoxIndirectParamW(
        GetModuleHandle(nullptr),
        reinterpret_cast<LPCDLGTEMPLATEW>(&dlgTemplate),
        hwnd,
        InputBoxDlgProc,
        reinterpret_cast<LPARAM>(&data)
    );
    if (data.accepted) {
        int len = WideCharToMultiByte(CP_UTF8, 0,
            data.result.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(len, 0);
        WideCharToMultiByte(CP_UTF8, 0,
            data.result.c_str(), -1, &utf8[0], len, nullptr, nullptr);
        if (!utf8.empty() && utf8.back() == '\0')
            utf8.pop_back();
        lua_pushstring(L, utf8.c_str());
    } else {
        lua_pushnil(L);
    }
    return 1;
}
extern "C" __declspec(dllexport)
int lume_plugin_init(lua_State* L, LumeHostAPI* api) {
    g_api = api;
    lua_register(L, "msgBox",            l_msgBox);
    lua_register(L, "msgBoxYesNoCancel", l_msgBoxYesNoCancel);
    lua_register(L, "inputBox",          l_inputBox);
    OutputDebugStringA("[Plugin] msgbox_plugin loaded: "
                       "msgBox, msgBoxYesNoCancel, inputBox\n");
    return 0;
}
