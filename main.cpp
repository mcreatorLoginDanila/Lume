#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <winhttp.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <functional>
#include <algorithm>
#include <memory>
#include <cmath>
#include <cstring>
#include <cctype>
#include <thread>
#include <mutex>
extern "C" {
#include "lib/lua.h"
#include "lib/lauxlib.h"
#include "lib/lualib.h"
}
#include "lume_plugin.h"
#include <objidl.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#include <GL/gl.h>
#include <GL/glu.h>
typedef HGLRC(WINAPI* PFN_wglCreateContext)(HDC);
typedef BOOL(WINAPI* PFN_wglMakeCurrent)(HDC, HGLRC);
typedef BOOL(WINAPI* PFN_wglDeleteContext)(HGLRC);
static double g_perfFreqInv = 0.0;
static LARGE_INTEGER g_perfStart = {};
void initHighResTimer() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    g_perfFreqInv = 1.0 / (double)freq.QuadPart;
    QueryPerformanceCounter(&g_perfStart);
}
double getHighResTime() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - g_perfStart.QuadPart) * g_perfFreqInv;
}
namespace GLLoader {
static HMODULE hGL = nullptr;
static PFN_wglCreateContext pfn_wglCreateContext = nullptr;
static PFN_wglMakeCurrent pfn_wglMakeCurrent = nullptr;
static PFN_wglDeleteContext pfn_wglDeleteContext = nullptr;
static bool g_loaded = false;
bool load() {
    if (g_loaded) return hGL != nullptr;
    g_loaded = true;
    char sysDir[MAX_PATH] = {};
    GetSystemDirectoryA(sysDir, MAX_PATH);
    std::string glPath = std::string(sysDir) + "\\opengl32.dll";
    hGL = LoadLibraryA(glPath.c_str());
    if (!hGL) return false;
    pfn_wglCreateContext = (PFN_wglCreateContext)GetProcAddress(hGL, "wglCreateContext");
    pfn_wglMakeCurrent = (PFN_wglMakeCurrent)GetProcAddress(hGL, "wglMakeCurrent");
    pfn_wglDeleteContext = (PFN_wglDeleteContext)GetProcAddress(hGL, "wglDeleteContext");
    if (!pfn_wglCreateContext || !pfn_wglMakeCurrent || !pfn_wglDeleteContext) {
        FreeLibrary(hGL);
        hGL = nullptr;
        return false;
    }
    return true;
}
void unload() {
    if (hGL) {
        FreeLibrary(hGL);
        hGL = nullptr;
    }
    g_loaded = false;
}
bool available() { return hGL != nullptr; }
}
void navigateTo(const std::string& url);
void invalidateContent();
HWND g_mainWnd = nullptr;
HWND g_addressBar = nullptr;
HWND g_statusBar = nullptr;
const int TOOLBAR_H = 40;
const int STATUS_H = 24;
#define ID_ADDR 1001
#define ID_GO   1002
#define ID_BACK 1003
#define ID_FWD  1004
#define ID_REF  1005
ULONG_PTR g_gdiplusToken;
static HDC g_backDC = nullptr;
static HBITMAP g_backBmp = nullptr;
static HBITMAP g_backOld = nullptr;
static int g_backW = 0, g_backH = 0;
static bool g_contentDirty = true;
static bool g_mouseCaptured = false;
static bool g_ignoreWarpMouse = false;
static std::string g_capturedCanvasId;
static int g_capturedCanvasX = 0, g_capturedCanvasY = 0;
static int g_capturedCanvasW = 0, g_capturedCanvasH = 0;
static int g_mouseDeltaX = 0, g_mouseDeltaY = 0;
static bool g_fullscreenCanvas = false;
static std::string g_fullscreenCanvasId;
static RECT g_preFullscreenRect = {};
static LONG g_preFullscreenStyle = 0;

void ensureBackbuffer(HDC hdc, int w, int h) {
    if (g_backDC && g_backW == w && g_backH == h) return;
    if (g_backDC) {
        SelectObject(g_backDC, g_backOld);
        DeleteDC(g_backDC);
        g_backDC = nullptr;
    }
    if (g_backBmp) {
        DeleteObject(g_backBmp);
        g_backBmp = nullptr;
    }
    g_backDC = CreateCompatibleDC(hdc);
    g_backBmp = CreateCompatibleBitmap(hdc, w, h);
    g_backOld = (HBITMAP)SelectObject(g_backDC, g_backBmp);
    g_backW = w;
    g_backH = h;
    g_contentDirty = true;
}
void cleanupBackbuffer() {
    if (g_backDC) {
        SelectObject(g_backDC, g_backOld);
        DeleteDC(g_backDC);
        g_backDC = nullptr;
    }
    if (g_backBmp) {
        DeleteObject(g_backBmp);
        g_backBmp = nullptr;
    }
    g_backOld = nullptr;
    g_backW = g_backH = 0;
}
void invalidateContent() {
    if (!g_mainWnd) return;
    g_contentDirty = true;
    RECT cr;
    GetClientRect(g_mainWnd, &cr);
    RECT contentRect = { 0, TOOLBAR_H, cr.right, cr.bottom - STATUS_H };
    InvalidateRect(g_mainWnd, &contentRect, FALSE);
}
void invalidateRect(int x, int y, int w, int h) {
    if (!g_mainWnd) return;
    RECT r = { x, TOOLBAR_H + y, x + w, TOOLBAR_H + h };
    InvalidateRect(g_mainWnd, &r, FALSE);
}
void invalidateGLCanvasRect(int x, int y, int w, int h) {
    if (!g_mainWnd) return;
    RECT r = { x, TOOLBAR_H + y, x + w, TOOLBAR_H + y + h };
    InvalidateRect(g_mainWnd, &r, FALSE);
}
void setStatus(const std::string& t) {
    if (g_statusBar) SetWindowTextA(g_statusBar, t.c_str());
}
void releaseMouse() {
    if (g_mouseCaptured) {
        g_mouseCaptured = false;
        g_ignoreWarpMouse = false;
        g_capturedCanvasId.clear();
        ReleaseCapture();
        ShowCursor(TRUE);
        ClipCursor(nullptr);
    }
}
namespace Script {
    void fireCanvasClick(const std::string& canvasId, int x, int y, int button);
}
namespace GLCanvas {
    struct GLView;
    std::shared_ptr<GLView> find(const std::string& id);
    bool registerClass(HINSTANCE hInst);
    void beginLayoutPass();
    void endLayoutPass();
    void place(const std::string& id, int x, int y, int w, int h);
    bool beginRender(const std::string& id, int w, int h);
    void endRender(const std::string& id);
    void refresh(const std::string& id);
    void moveToFullscreen(const std::string& id);
    void restoreFromFullscreen(const std::string& id);
    void destroyAll();
}
void exitFullscreenCanvas() {
    if (!g_fullscreenCanvas) return;
    std::string prevId = g_fullscreenCanvasId;
    g_fullscreenCanvas = false;
    g_fullscreenCanvasId.clear();
    SetWindowLongA(g_mainWnd, GWL_STYLE, g_preFullscreenStyle);
    SetWindowPos(g_mainWnd, HWND_NOTOPMOST,
        g_preFullscreenRect.left, g_preFullscreenRect.top,
        g_preFullscreenRect.right - g_preFullscreenRect.left,
        g_preFullscreenRect.bottom - g_preFullscreenRect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    ShowWindow(g_addressBar, SW_SHOW);
    ShowWindow(g_statusBar, SW_SHOW);
    HWND hBack = GetDlgItem(g_mainWnd, ID_BACK);
    HWND hFwd = GetDlgItem(g_mainWnd, ID_FWD);
    HWND hRef = GetDlgItem(g_mainWnd, ID_REF);
    HWND hGo = GetDlgItem(g_mainWnd, ID_GO);
    if (hBack) ShowWindow(hBack, SW_SHOW);
    if (hFwd)  ShowWindow(hFwd, SW_SHOW);
    if (hRef)  ShowWindow(hRef, SW_SHOW);
    if (hGo)   ShowWindow(hGo, SW_SHOW);
    if (!prevId.empty()) {
        GLCanvas::restoreFromFullscreen(prevId);
    }
    g_contentDirty = true;
    InvalidateRect(g_mainWnd, nullptr, FALSE);
}
void enterFullscreenCanvas(const std::string& canvasId) {
    if (g_fullscreenCanvas) return;
    g_fullscreenCanvas = true;
    g_fullscreenCanvasId = canvasId;
    GetWindowRect(g_mainWnd, &g_preFullscreenRect);
    g_preFullscreenStyle = GetWindowLongA(g_mainWnd, GWL_STYLE);
    HMONITOR hMon = MonitorFromWindow(g_mainWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoA(hMon, &mi);
    SetWindowLongA(g_mainWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(g_mainWnd, HWND_TOPMOST,
        mi.rcMonitor.left, mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    ShowWindow(g_addressBar, SW_HIDE);
    ShowWindow(g_statusBar, SW_HIDE);
    HWND hBack = GetDlgItem(g_mainWnd, ID_BACK);
    HWND hFwd = GetDlgItem(g_mainWnd, ID_FWD);
    HWND hRef = GetDlgItem(g_mainWnd, ID_REF);
    HWND hGo = GetDlgItem(g_mainWnd, ID_GO);
    if (hBack) ShowWindow(hBack, SW_HIDE);
    if (hFwd)  ShowWindow(hFwd, SW_HIDE);
    if (hRef)  ShowWindow(hRef, SW_HIDE);
    if (hGo)   ShowWindow(hGo, SW_HIDE);
    GLCanvas::moveToFullscreen(canvasId);
    InvalidateRect(g_mainWnd, nullptr, FALSE);
}
namespace FontCache {
struct Key {
    int size;
    bool bold, italic;
    std::string face;
    bool operator<(const Key& o) const {
        if (size != o.size) return size < o.size;
        if (bold != o.bold) return bold < o.bold;
        if (italic != o.italic) return italic < o.italic;
        return face < o.face;
    }
};
static std::map<Key, HFONT> g_cache;
HFONT get(int sz, bool b = false, bool i = false, const char* f = "Segoe UI") {
    Key k{ sz, b, i, f };
    auto it = g_cache.find(k);
    if (it != g_cache.end()) return it->second;

    HFONT font = CreateFontA(-sz, 0, 0, 0,
        b ? FW_BOLD : FW_NORMAL,
        i, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, f);
    g_cache[k] = font;
    return font;
}

void clear() {
    for (auto& kv : g_cache) DeleteObject(kv.second);
    g_cache.clear();
}
}
namespace Plugins {
struct LoadedPlugin {
    std::string name, path;
    HMODULE hModule = nullptr;
    lume_plugin_init_fn initFn = nullptr;
};
static std::vector<LoadedPlugin> g_plugins;
static LumeHostAPI g_hostAPI = {};
static HWND hostGetMainHwnd() { return g_mainWnd; }
static void hostInvalidateContent() { invalidateContent(); }
static void hostSetStatus(const char* t) { setStatus(t ? t : ""); }
static void hostNavigateTo(const char* u) { if (u) navigateTo(u); }
void initHostAPI() {
    g_hostAPI.get_main_hwnd = hostGetMainHwnd;
    g_hostAPI.invalidate_content = hostInvalidateContent;
    g_hostAPI.set_status = hostSetStatus;
    g_hostAPI.navigate_to = hostNavigateTo;
    g_hostAPI.api_version = 2;

    g_hostAPI.p_luaL_checknumber = luaL_checknumber;
    g_hostAPI.p_luaL_checkinteger = luaL_checkinteger;
    g_hostAPI.p_luaL_optinteger = luaL_optinteger;
    g_hostAPI.p_luaL_optlstring = luaL_optlstring;
    g_hostAPI.p_luaL_checklstring = luaL_checklstring;
    g_hostAPI.p_lua_pushnumber = lua_pushnumber;
    g_hostAPI.p_lua_pushboolean = lua_pushboolean;
    g_hostAPI.p_lua_pushstring = lua_pushstring;
    g_hostAPI.p_lua_pushcclosure = lua_pushcclosure;
    g_hostAPI.p_lua_setglobal = lua_setglobal;
}
void discoverPlugins() {
    char ep[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, ep, MAX_PATH);
    std::string dir = ep;
    auto ls = dir.find_last_of("\\/");
    dir = (ls != std::string::npos) ? dir.substr(0, ls + 1) : ".\\";
    std::string pd = dir + "plugins\\";
    CreateDirectoryA(pd.c_str(), nullptr);

    WIN32_FIND_DATAA fd = {};
    HANDLE hf = FindFirstFileA((pd + "*.dll").c_str(), &fd);
    if (hf == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        LoadedPlugin p;
        p.name = fd.cFileName;
        p.path = pd + fd.cFileName;
        p.hModule = LoadLibraryA(p.path.c_str());
        if (!p.hModule) continue;
        p.initFn = (lume_plugin_init_fn)GetProcAddress(p.hModule, "lume_plugin_init");
        if (!p.initFn) {
            FreeLibrary(p.hModule);
            continue;
        }
        g_plugins.push_back(p);
    } while (FindNextFileA(hf, &fd));
    FindClose(hf);
}
void initAllPlugins(lua_State* L) {
    for (auto& p : g_plugins)
        if (p.initFn) p.initFn(L, &g_hostAPI);
}
void unloadAll() {
    for (auto& p : g_plugins) {
        if (!p.hModule) continue;
        auto sf = (void(*)())GetProcAddress(p.hModule, "lume_plugin_shutdown");
        if (sf) sf();
        FreeLibrary(p.hModule);
        p.hModule = nullptr;
    }
    g_plugins.clear();
}
}
namespace HTP {
struct Color {
    int r = 255, g = 255, b = 255, a = 255;
    static Color fromHex(const std::string& hex) {
        Color c;
        std::string h = hex;
        if (!h.empty() && h[0] == '#') h = h.substr(1);
        try {
            if (h.length() >= 6) {
                c.r = std::stoi(h.substr(0, 2), 0, 16);
                c.g = std::stoi(h.substr(2, 2), 0, 16);
                c.b = std::stoi(h.substr(4, 2), 0, 16);
            }
            if (h.length() >= 8) c.a = std::stoi(h.substr(6, 2), 0, 16);
        }
        catch (...) {}
        return c;
    }
    COLORREF cr() const { return RGB(r, g, b); }
};
enum class EType {
    PAGE, BLOCK, TEXT, IMAGE, LINK, BUTTON, INPUT_FIELD, DIVIDER, LIST, ITEM, BR, SCRIPT, CANVAS, ROW, COLUMN, GL_CANVAS, UNKNOWN
};
struct Props {
    std::map<std::string, std::string> d;
    std::string get(const std::string& k, const std::string& def = "") const {
        auto i = d.find(k);
        return i != d.end() ? i->second : def;
    }
    int getInt(const std::string& k, int def = 0) const {
        auto i = d.find(k);
        if (i != d.end()) {
            try { return std::stoi(i->second); }
            catch (...) {}
        }
        return def;
    }
    bool getBool(const std::string& k, bool def = false) const {
        auto i = d.find(k);
        if (i != d.end()) return i->second == "true" || i->second == "1";
        return def;
    }
    Color getColor(const std::string& k, Color def = { 255,255,255 }) const {
        auto i = d.find(k);
        if (i != d.end()) return Color::fromHex(i->second);
        return def;
    }
};
struct Elem {
    EType type = EType::UNKNOWN;
    std::string tag;
    Props props;
    std::string scriptCode;
    std::vector<std::shared_ptr<Elem>> children;
};
struct Doc {
    std::shared_ptr<Elem> root;
    std::string title;
    Color bg = { 26,26,46 };
};
std::shared_ptr<Elem> findById(std::shared_ptr<Elem> node, const std::string& id) {
    if (!node) return nullptr;
    if (node->props.get("id") == id) return node;
    for (auto& c : node->children) {
        auto r = findById(c, id);
        if (r) return r;
    }
    return nullptr;
}
class Parser {
    std::string src;
    int pos = 0, len = 0;
    void skipWS() {
        while (pos < len) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { pos++; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '/') {
                while (pos < len && src[pos] != '\n') pos++;
                continue;
            }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '*') {
                pos += 2;
                while (pos + 1 < len && !(src[pos] == '*' && src[pos + 1] == '/')) pos++;
                if (pos + 1 < len) pos += 2;
                continue;
            }
            break;
        }
    }
    std::string readIdent() {
        std::string r;
        while (pos < len && (std::isalnum((unsigned char)src[pos]) || src[pos] == '_' || src[pos] == '-'))
            r += src[pos++];
        return r;
    }
    std::string readString() {
        if (pos >= len || src[pos] != '"') return "";
        pos++;
        std::string r;
        r.reserve(64);
        while (pos < len && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < len) {
                pos++;
                switch (src[pos]) {
                case 'n': r += '\n'; break;
                case 't': r += '\t'; break;
                case '"': r += '"'; break;
                case '\\': r += '\\'; break;
                default: r += src[pos]; break;
                }
            }
            else {
                r += src[pos];
            }
            pos++;
        }
        if (pos < len) pos++;
        return r;
    }
    std::string readValue() {
        skipWS();
        if (pos < len && src[pos] == '"') return readString();
        std::string val;
        while (pos < len && src[pos] != ';' && src[pos] != '}') val += src[pos++];
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r' || val.back() == '\n'))
            val.pop_back();
        return val;
    }
    std::string readRawBlock() {
        int depth = 1;
        int start = pos;
        bool inStr = false, inLC = false, inBC = false;
        while (pos < len && depth > 0) {
            char c = src[pos];
            if (inLC) { if (c == '\n') inLC = false; pos++; continue; }
            if (inBC) {
                if (c == '*' && pos + 1 < len && src[pos + 1] == '/') { inBC = false; pos += 2; continue; }
                pos++; continue;
            }
            if (inStr) {
                if (c == '\\') { pos += 2; continue; }
                if (c == '"') inStr = false;
                pos++; continue;
            }
            if (c == '"') { inStr = true; pos++; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '/') { inLC = true; pos += 2; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '*') { inBC = true; pos += 2; continue; }
            if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth == 0) break; }
            pos++;
        }
        std::string code = src.substr(start, pos - start);
        if (pos < len && src[pos] == '}') pos++;
        return code;
    }
    void parseInlineAttrs(std::shared_ptr<Elem>& e) {
        skipWS();
        if (pos >= len || src[pos] != '(') return;
        pos++;
        while (pos < len && src[pos] != ')') {
            skipWS();
            std::string key = readIdent();
            skipWS();
            if (pos < len && src[pos] == '=') {
                pos++;
                skipWS();
                e->props.d[key] = readString();
            }
            skipWS();
            if (pos < len && src[pos] == ',') pos++;
        }
        if (pos < len) pos++;
    }
    EType tagToType(const std::string& t) {
        if (t == "page") return EType::PAGE;
        if (t == "block") return EType::BLOCK;
        if (t == "text") return EType::TEXT;
        if (t == "image") return EType::IMAGE;
        if (t == "link") return EType::LINK;
        if (t == "button") return EType::BUTTON;
        if (t == "input") return EType::INPUT_FIELD;
        if (t == "divider") return EType::DIVIDER;
        if (t == "list") return EType::LIST;
        if (t == "item") return EType::ITEM;
        if (t == "br") return EType::BR;
        if (t == "script") return EType::SCRIPT;
        if (t == "canvas") return EType::CANVAS;
        if (t == "row") return EType::ROW;
        if (t == "column") return EType::COLUMN;
        if (t == "glcanvas") return EType::GL_CANVAS;
        return EType::UNKNOWN;
    }
    std::shared_ptr<Elem> parseElement() {
        skipWS();
        if (pos >= len || src[pos] != '@') return nullptr;
        pos++;
        auto e = std::make_shared<Elem>();
        e->tag = readIdent();
        e->type = tagToType(e->tag);
        parseInlineAttrs(e);
        skipWS();
        if (pos >= len || src[pos] != '{') return e;
        pos++;
        if (e->type == EType::SCRIPT) {
            e->scriptCode = readRawBlock();
            return e;
        }
        while (pos < len) {
            skipWS();
            if (pos >= len) break;
            if (src[pos] == '}') { pos++; break; }
            if (src[pos] == '@') {
                auto ch = parseElement();
                if (ch) e->children.push_back(ch);
            }
            else if (std::isalpha((unsigned char)src[pos]) || src[pos] == '_') {
                std::string key = readIdent();
                skipWS();
                if (pos < len && src[pos] == ':') {
                    pos++;
                    e->props.d[key] = readValue();
                    skipWS();
                    if (pos < len && src[pos] == ';') pos++;
                }
            }
            else pos++;
        }
        return e;
    }
public:
    Doc parse(const std::string& source) {
        Doc doc;
        src = source;
        pos = 0;
        len = (int)src.length();
        auto root = std::make_shared<Elem>();
        root->type = EType::PAGE;
        root->tag = "root";
        while (pos < len) {
            skipWS();
            if (pos >= len) break;
            if (src[pos] == '@') {
                auto e = parseElement();
                if (e) {
                    if (e->type == EType::PAGE) {
                        doc.title = e->props.get("title", "Lume");
                        doc.bg = e->props.getColor("background", { 26,26,46 });
                        for (auto& c : e->children) root->children.push_back(c);
                    }
                    else root->children.push_back(e);
                }
            }
            else pos++;
        }
        doc.root = root;
        return doc;
    }
};
}
std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}
namespace Net {
    struct URL {
        std::string proto, host, path;
        int port = 80;
        static URL parse(const std::string& s) {
            URL u;
            std::string r = s;
            auto pe = r.find("://");
            if (pe != std::string::npos) {
                u.proto = r.substr(0, pe);
                r = r.substr(pe + 3);
            }
            else u.proto = "http";
            auto ps = r.find('/');
            if (ps != std::string::npos) {
                u.path = r.substr(ps);
                r = r.substr(0, ps);
            }
            else u.path = "/";

            auto pp = r.find(':');
            if (pp != std::string::npos) {
                u.host = r.substr(0, pp);
                try { u.port = std::stoi(r.substr(pp + 1)); }
                catch (...) {}
            }
            else {
                u.host = r;
                if (u.proto == "https") u.port = 443;
                else u.port = 80;
            }
            if (u.path.empty()) u.path = "/";
            return u;
        }
    };
struct Resp {
    int code = 0;
    std::string body, error;
    bool ok = false;
};
Resp fetch(const URL& url) {
    Resp resp;
    HINTERNET hSession = WinHttpOpen(L"Lume Browser Engine",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if (!hSession) { resp.error = "WinHttpOpen failed"; return resp; }
    HINTERNET hConnect = WinHttpConnect(hSession, utf8_to_wstring(url.host).c_str(), url.port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); resp.error = "WinHttpConnect failed"; return resp; }
    DWORD flags = (url.proto == "https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", utf8_to_wstring(url.path).c_str(),NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        resp.error = "WinHttpOpenRequest failed"; return resp;
    }
    if (flags & WINHTTP_FLAG_SECURE) {
        DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
    }
    bool bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&dwStatusCode,&dwSize,WINHTTP_NO_HEADER_INDEX);
        resp.code = dwStatusCode;
        resp.ok = (resp.code >= 200 && resp.code < 400);
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            std::vector<char> buffer(dwSize);
            if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                resp.body.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }
    else {
        resp.error = "HTTP Connection failed";
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}
Resp fetchUrl(const std::string& u) { return fetch(URL::parse(u)); }
}
namespace AsyncNet {
    struct DLState {
        int status = 0;
        std::string localPath;
        size_t downloadedBytes = 0;
        size_t totalBytes = 0;
    };
    std::map<std::string, DLState> g_downloads;
    std::mutex g_dlMutex;
    std::string getTempDir() {
        char ep[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, ep, MAX_PATH);
        std::string dir = ep;
        auto ls = dir.find_last_of("\\/");
        dir = (ls != std::string::npos) ? dir.substr(0, ls + 1) : ".\\";
        std::string td = dir + "temp\\";
        CreateDirectoryA(td.c_str(), nullptr);
        return td;
    }
    void startDownload(const std::string& urlStr, const std::string& filename) {
        {
            std::lock_guard<std::mutex> lock(g_dlMutex);
            g_downloads[urlStr] = { 0, "", 0, 0 };
        }
        std::thread([urlStr, filename]() {
            Net::URL url = Net::URL::parse(urlStr);
            std::string outPath = getTempDir() + filename;
            auto setError = [&]() {
                std::lock_guard<std::mutex> lock(g_dlMutex);
                g_downloads[urlStr].status = -1;
                };
            HINTERNET hSession = WinHttpOpen(L"Lume Download Agent", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) { setError(); return; }
            HINTERNET hConnect = WinHttpConnect(hSession, utf8_to_wstring(url.host).c_str(), url.port, 0);
            if (!hConnect) { WinHttpCloseHandle(hSession); setError(); return; }
            DWORD flags = (url.proto == "https") ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", utf8_to_wstring(url.path).c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); setError(); return; }
            if (flags & WINHTTP_FLAG_SECURE) {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
            }
            bool bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);
            if (bResults) {
                DWORD statusCode = 0, sz = sizeof(statusCode);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz, WINHTTP_NO_HEADER_INDEX);
                if (statusCode >= 400) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); setError(); return;}
                DWORD contentLength = 0;
                sz = sizeof(contentLength);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &sz, WINHTTP_NO_HEADER_INDEX);
                {
                    std::lock_guard<std::mutex> lock(g_dlMutex);
                    g_downloads[urlStr].totalBytes = contentLength;
                }
                std::ofstream outFile(outPath, std::ios::binary);
                if (!outFile) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); setError(); return; }
                DWORD dwSize = 0, dwDownloaded = 0;
                do {
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                    if (dwSize == 0) break;
                    std::vector<char> buffer(dwSize);
                    if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                        outFile.write(buffer.data(), dwDownloaded);
                        std::lock_guard<std::mutex> lock(g_dlMutex);
                        g_downloads[urlStr].downloadedBytes += dwDownloaded;
                    }
                } while (dwSize > 0);
                outFile.close();
                {
                    std::lock_guard<std::mutex> lock(g_dlMutex);
                    g_downloads[urlStr].status = 1;
                    g_downloads[urlStr].localPath = outPath;
                }
            }
            else {
                setError();
            }
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            }).detach();
    }
}
namespace Canvas {
struct Buf {
    int w = 0, h = 0;
    HDC dc = 0;
    HBITMAP bmp = 0, old = 0;
    void create(HDC ref, int ww, int hh) {
        cleanup();
        w = ww;
        h = hh;
        dc = CreateCompatibleDC(ref);
        bmp = CreateCompatibleBitmap(ref, w, h);
        old = (HBITMAP)SelectObject(dc, bmp);
        HBRUSH b = CreateSolidBrush(0);
        RECT r = { 0,0,w,h };
        FillRect(dc, &r, b);
        DeleteObject(b);
    }
    void cleanup() {
        if (dc) {
            SelectObject(dc, old);
            DeleteDC(dc);
            dc = 0;
        }
        if (bmp) {
            DeleteObject(bmp);
            bmp = 0;
        }
    }
    ~Buf() { cleanup(); }
};
std::map<std::string, std::shared_ptr<Buf>> g_bufs;
std::shared_ptr<Buf> get(const std::string& id, HDC ref, int w, int h) {
    auto i = g_bufs.find(id);
    if (i != g_bufs.end() && i->second->w == w && i->second->h == h) return i->second;
    auto b = std::make_shared<Buf>();
    b->create(ref, w, h);
    g_bufs[id] = b;
    return b;
}
}
namespace GLCanvas {
    static const char* kClassName = "LumeGLCanvasClass";
    static bool g_classRegistered = false;
    static HINSTANCE g_hInst = nullptr;
    struct GLView {
        std::string id;
        HWND hwnd = nullptr;
        HDC hdc = nullptr;
        HGLRC hglrc = nullptr;
        int x = 0, y = 0, w = 0, h = 0;
        bool visible = false;
        bool valid = false;
        bool touchedThisLayout = false;
        bool fullscreen = false;
    };
    std::map<std::string, std::shared_ptr<GLView>> g_views;
    std::string findIdByHwnd(HWND hwnd) {
        for (auto& kv : g_views) {
            if (kv.second && kv.second->hwnd == hwnd)
                return kv.first;
        }
        return "";
    }
    LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (!g_mainWnd) return 0;
            std::string canvasId = findIdByHwnd(hwnd);
            int localX = GET_X_LPARAM(lp);
            int localY = GET_Y_LPARAM(lp);

            if (!canvasId.empty()) {
                Script::fireCanvasClick(canvasId, localX, localY, 1);

                if (!g_mouseCaptured) {
                    auto v = find(canvasId);
                    if (v && v->valid) {
                        g_mouseCaptured = true;
                        g_ignoreWarpMouse = false;
                        g_capturedCanvasId = canvasId;
                        g_capturedCanvasW = v->w;
                        g_capturedCanvasH = v->h;
                        g_capturedCanvasX = v->x;
                        g_capturedCanvasY = v->y - TOOLBAR_H;
                        SetCapture(g_mainWnd);
                        ShowCursor(FALSE);
                        RECT wr;
                        GetWindowRect(v->hwnd, &wr);
                        ClipCursor(&wr);
                    }
                }
            }
            SetFocus(g_mainWnd);
            return 0;
        }
        case WM_RBUTTONDOWN: {
            if (!g_mainWnd) return 0;
            std::string canvasId = findIdByHwnd(hwnd);
            if (!canvasId.empty()) {
                Script::fireCanvasClick(canvasId, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 2);
            }
            SetFocus(g_mainWnd);
            return 0;
        }
        case WM_MBUTTONDOWN: {
            if (!g_mainWnd) return 0;
            std::string canvasId = findIdByHwnd(hwnd);
            if (!canvasId.empty()) {
                Script::fireCanvasClick(canvasId, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 3);
            }
            SetFocus(g_mainWnd);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_mouseCaptured && g_mainWnd) {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ClientToScreen(hwnd, &pt);
                ScreenToClient(g_mainWnd, &pt);
                SendMessage(g_mainWnd, WM_MOUSEMOVE, wp, MAKELPARAM(pt.x, pt.y));
                return 0;
            }
            break;
        }
        case WM_SETFOCUS:
            if (g_mainWnd) SetFocus(g_mainWnd);
            return 0;
        case WM_NCHITTEST:
            return HTCLIENT;

        default:
            return DefWindowProcA(hwnd, msg, wp, lp);
        }
        return DefWindowProcA(hwnd, msg, wp, lp);
    }
    bool registerClass(HINSTANCE hInst) {
        if (g_classRegistered) return true;
        g_hInst = hInst;
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = CanvasWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClassName;
        wc.hbrBackground = nullptr;

        if (!RegisterClassExA(&wc)) {
            DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS) return false;
        }
        g_classRegistered = true;
        return true;
    }
    bool setupPixelFormat(HDC hdc) {
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        int pf = ChoosePixelFormat(hdc, &pfd);
        if (!pf) return false;
        if (!SetPixelFormat(hdc, pf, &pfd)) return false;
        return true;
    }
    std::shared_ptr<GLView> createView(const std::string& id, int w, int h) {
        if (!g_mainWnd || !g_hInst) return nullptr;
        auto v = std::make_shared<GLView>();
        v->id = id;
        v->w = w;
        v->h = h;
        v->hwnd = CreateWindowExA(
            0,
            kClassName,
            "",
            WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE,
            0, 0, w, h,
            g_mainWnd,
            nullptr,
            g_hInst,
            nullptr
        );
        if (!v->hwnd) return nullptr;
        v->hdc = GetDC(v->hwnd);
        if (!v->hdc) {
            DestroyWindow(v->hwnd);
            v->hwnd = nullptr;
            return nullptr;
        }
        if (!setupPixelFormat(v->hdc)) {
            ReleaseDC(v->hwnd, v->hdc);
            DestroyWindow(v->hwnd);
            v->hdc = nullptr;
            v->hwnd = nullptr;
            return nullptr;
        }
        v->hglrc = wglCreateContext(v->hdc);
        if (!v->hglrc) {
            ReleaseDC(v->hwnd, v->hdc);
            DestroyWindow(v->hwnd);
            v->hdc = nullptr;
            v->hwnd = nullptr;
            return nullptr;
        }
        v->valid = true;
        v->visible = true;
        ShowWindow(v->hwnd, SW_SHOW);
        UpdateWindow(v->hwnd);
        return v;
    }
    void destroyView(std::shared_ptr<GLView>& v) {
        if (!v) return;
        if (v->hglrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(v->hglrc);
            v->hglrc = nullptr;
        }
        if (v->hdc && v->hwnd) {
            ReleaseDC(v->hwnd, v->hdc);
            v->hdc = nullptr;
        }
        if (v->hwnd) {
            DestroyWindow(v->hwnd);
            v->hwnd = nullptr;
        }
        v->valid = false;
    }
    std::shared_ptr<GLView> find(const std::string& id) {
        auto it = g_views.find(id);
        if (it == g_views.end()) return nullptr;
        return it->second;
    }
    std::shared_ptr<GLView> ensure(const std::string& id, int w, int h) {
        auto it = g_views.find(id);
        if (it != g_views.end()) {
            auto v = it->second;
            if (v && v->valid) {
                if (v->w != w || v->h != h) {
                    int oldX = v->x, oldY = v->y;
                    bool oldVis = v->visible;
                    destroyView(v);
                    auto nv = createView(id, w, h);
                    if (!nv) return nullptr;
                    nv->x = oldX;
                    nv->y = oldY;
                    nv->visible = oldVis;
                    MoveWindow(nv->hwnd, nv->x, nv->y, w, h, TRUE);
                    if (oldVis) ShowWindow(nv->hwnd, SW_SHOW);
                    else ShowWindow(nv->hwnd, SW_HIDE);
                    g_views[id] = nv;
                    return nv;
                }
                return v;
            }
        }
        auto v = createView(id, w, h);
        if (!v) return nullptr;
        g_views[id] = v;
        return v;
    }
    void beginLayoutPass() {
        for (auto& kv : g_views) {
            if (kv.second) kv.second->touchedThisLayout = false;
        }
    }
    void place(const std::string& id, int x, int y, int w, int h, int scrollY = 0, int toolbarH = 0) {
        auto v = ensure(id, w, h);
        if (!v || !v->valid) return;
        v->touchedThisLayout = true;
        if (g_fullscreenCanvas && g_fullscreenCanvasId == id) {
            moveToFullscreen(id);
            return;
        }
        bool changed = (v->x != x || v->y != y || v->w != w || v->h != h);
        v->x = x;
        v->y = y;
        v->w = w;
        v->h = h;
        v->visible = true;
        RECT cr;
        GetClientRect(g_mainWnd, &cr);
        int contentTop = toolbarH;
        int contentBot = cr.bottom - STATUS_H;
        bool inView = (y + h > contentTop) && (y < contentBot);

        if (inView) {
            if (changed) {
                MoveWindow(v->hwnd, x, y, w, h, FALSE);
            }
            if (!IsWindowVisible(v->hwnd)) {
                ShowWindow(v->hwnd, SW_SHOWNA);
            }
        }
        else {
            if (IsWindowVisible(v->hwnd)) {
                ShowWindow(v->hwnd, SW_HIDE);
            }
        }
    }
    void endLayoutPass() {
        for (auto& kv : g_views) {
            auto& v = kv.second;
            if (!v || !v->valid) continue;
            if (!v->touchedThisLayout && !g_fullscreenCanvas) {
                v->visible = false;
                ShowWindow(v->hwnd, SW_HIDE);
            }
        }
    }
    bool beginRender(const std::string& id, int w, int h) {
        auto v = ensure(id, w, h);
        if (!v || !v->valid) return false;
        return wglMakeCurrent(v->hdc, v->hglrc) == TRUE;
    }
    void endRender(const std::string& id) {
        auto v = find(id);
        if (!v || !v->valid) return;
        glFlush();
        SwapBuffers(v->hdc);
        wglMakeCurrent(nullptr, nullptr);
    }
    void refresh(const std::string& id) {
        (void)id;
    }
    void setVisible(const std::string& id, bool vis) {
        auto v = find(id);
        if (!v || !v->valid) return;
        v->visible = vis;
        ShowWindow(v->hwnd, vis ? SW_SHOW : SW_HIDE);
    }
    void moveToFullscreen(const std::string& id) {
        auto v = find(id);
        if (!v || !v->valid || !g_mainWnd) return;
        RECT cr;
        GetClientRect(g_mainWnd, &cr);
        v->fullscreen = true;
        v->visible = true;
        MoveWindow(v->hwnd, 0, 0, cr.right, cr.bottom, TRUE);
        ShowWindow(v->hwnd, SW_SHOW);
        BringWindowToTop(v->hwnd);
    }
    void restoreFromFullscreen(const std::string& id) {
        auto v = find(id);
        if (!v || !v->valid) return;
        v->fullscreen = false;
        MoveWindow(v->hwnd, v->x, v->y, v->w, v->h, TRUE);
        if (v->visible) ShowWindow(v->hwnd, SW_SHOW);
    }
    void hideAll() {
        for (auto& kv : g_views) {
            auto& v = kv.second;
            if (v && v->valid) {
                v->visible = false;
                ShowWindow(v->hwnd, SW_HIDE);
            }
        }
    }
    void showAll() {
        for (auto& kv : g_views) {
            auto& v = kv.second;
            if (v && v->valid && v->visible) {
                ShowWindow(v->hwnd, SW_SHOW);
            }
        }
    }
    void destroyAll() {
        for (auto& kv : g_views) {
            auto& v = kv.second;
            destroyView(v);
        }
        g_views.clear();
    }

}
namespace GradientText {
    struct HSV { float h, s, v; };
    static HSV rgb2hsv(HTP::Color c) {
        float r = c.r / 255.0f;
        float g = c.g / 255.0f;
        float b = c.b / 255.0f;
        float max_val = (std::max)(r, (std::max)(g, b));
        float min_val = (std::min)(r, (std::min)(g, b));
        float d = max_val - min_val;
        float h = 0.0f;
        float s = (max_val == 0.0f) ? 0.0f : (d / max_val);
        float v = max_val;
        if (d != 0.0f) {
            if (max_val == r) {
                h = (g - b) / d + (g < b ? 6.0f : 0.0f);
            }
            else if (max_val == g) {
                h = (b - r) / d + 2.0f;
            }
            else {
                h = (r - g) / d + 4.0f;
            }
            h /= 6.0f;
        }
        return { h * 360.0f, s, v };
    }
    static HTP::Color hsv2rgb(HSV hsv) {
        float c = hsv.v * hsv.s;
        float x = c * (1.0f - std::abs(fmodf(hsv.h / 60.0f, 2.0f) - 1.0f));
        float m = hsv.v - c;
        float r = 0, g = 0, b = 0;
        if (hsv.h >= 0 && hsv.h < 60) { r = c; g = x; b = 0; }
        else if (hsv.h >= 60 && hsv.h < 120) { r = x; g = c; b = 0; }
        else if (hsv.h >= 120 && hsv.h < 180) { r = 0; g = c; b = x; }
        else if (hsv.h >= 180 && hsv.h < 240) { r = 0; g = x; b = c; }
        else if (hsv.h >= 240 && hsv.h < 300) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        return {
            (int)((r + m) * 255.0f),
            (int)((g + m) * 255.0f),
            (int)((b + m) * 255.0f),
            255
        };
    }
    static HTP::Color lerpColor(HTP::Color c1, HTP::Color c2, float t) {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        HSV hsv1 = rgb2hsv(c1);
        HSV hsv2 = rgb2hsv(c2);
        float h1 = hsv1.h;
        float h2 = hsv2.h;
        float d = h2 - h1;
        if (d > 180.0f) {
            h1 += 360.0f;
        }
        else if (d < -180.0f) {
            h2 += 360.0f;
        }
        float h = h1 + (h2 - h1) * t;
        if (h >= 360.0f) h -= 360.0f;
        if (h < 0.0f) h += 360.0f;
        float s = hsv1.s + (hsv2.s - hsv1.s) * t;
        float v = hsv1.v + (hsv2.v - hsv1.v) * t;
        return hsv2rgb({ h, s, v });
    }
    void draw(HDC dc, const std::string& text, HFONT font,int x, int y, HTP::Color c1, HTP::Color c2,float shimmer_offset = 0.0f)
    {
        if (text.empty()) return;
        HFONT oldFont = (HFONT)SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SIZE totalSz;
        GetTextExtentPoint32A(dc, text.c_str(), (int)text.length(), &totalSz);
        int W = totalSz.cx;
        int H = totalSz.cy;
        if (W <= 0 || H <= 0) {
            SelectObject(dc, oldFont);
            return;
        }
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = W;
        bmi.bmiHeader.biHeight = -H;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        BYTE* maskBits = nullptr;
        HDC maskDC = CreateCompatibleDC(dc);
        HBITMAP maskBmp = CreateDIBSection(maskDC, &bmi, DIB_RGB_COLORS,(void**)&maskBits, NULL, 0);
        HBITMAP oldMaskBmp = (HBITMAP)SelectObject(maskDC, maskBmp);
        HFONT oldMaskFont = (HFONT)SelectObject(maskDC, font);
        memset(maskBits, 0, W * H * 4);
        SetBkMode(maskDC, TRANSPARENT);
        SetTextColor(maskDC, RGB(255, 255, 255));
        TextOutA(maskDC, 0, 0, text.c_str(), (int)text.length());
        GdiFlush();
        BYTE* outBits = nullptr;
        HDC outDC = CreateCompatibleDC(dc);
        HBITMAP outBmp = CreateDIBSection(outDC, &bmi, DIB_RGB_COLORS,(void**)&outBits, NULL, 0);
        HBITMAP oldOutBmp = (HBITMAP)SelectObject(outDC, outBmp);
        BitBlt(outDC, 0, 0, W, H, dc, x, y, SRCCOPY);
        GdiFlush();
        float span = 2.0f;
        for (int py = 0; py < H; py++) {
            for (int px = 0; px < W; px++) {
                int idx = (py * W + px) * 4;
                BYTE mask = maskBits[idx];
                if (mask == 0) continue;
                float normX = (W > 1) ? (float)px / (float)(W - 1) : 0.0f;
                float t = normX + shimmer_offset;
                t = fmodf(t, span);
                if (t < 0.f) t += span;
                if (t > 1.0f) t = span - t;
                if (t < 0.f) t = 0.f;
                if (t > 1.f) t = 1.f;
                HTP::Color gc = lerpColor(c1, c2, t);
                float alpha = mask / 255.0f;
                BYTE bgB = outBits[idx + 0];
                BYTE bgG = outBits[idx + 1];
                BYTE bgR = outBits[idx + 2];
                outBits[idx + 0] = (BYTE)(bgB + (gc.b - bgB) * alpha);
                outBits[idx + 1] = (BYTE)(bgG + (gc.g - bgG) * alpha);
                outBits[idx + 2] = (BYTE)(bgR + (gc.r - bgR) * alpha);
            }
        }
        BitBlt(dc, x, y, W, H, outDC, 0, 0, SRCCOPY);
        SelectObject(maskDC, oldMaskFont);
        SelectObject(maskDC, oldMaskBmp);
        DeleteObject(maskBmp);
        DeleteDC(maskDC);
        SelectObject(outDC, oldOutBmp);
        DeleteObject(outBmp);
        DeleteDC(outDC);
        SelectObject(dc, oldFont);
    }
}
static void gluPerspective_impl(double fovY, double aspect, double zNear, double zFar) {
    double f = 1.0 / tan(fovY * 3.14159265358979323846 / 360.0);
    double m[16] = {};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0;
    m[14] = (2.0 * zFar * zNear) / (zNear - zFar);
    glMultMatrixd(m);
}
static void gluLookAt_impl(double eyeX, double eyeY, double eyeZ,
    double centerX, double centerY, double centerZ,
    double upX, double upY, double upZ) {
    double fx = centerX - eyeX, fy = centerY - eyeY, fz = centerZ - eyeZ;
    double flen = sqrt(fx * fx + fy * fy + fz * fz);
    if (flen > 0) { fx /= flen; fy /= flen; fz /= flen; }

    double sx = fy * upZ - fz * upY, sy = fz * upX - fx * upZ, sz = fx * upY - fy * upX;
    double slen = sqrt(sx * sx + sy * sy + sz * sz);
    if (slen > 0) { sx /= slen; sy /= slen; sz /= slen; }

    double ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;

    double m[16] = {
        sx, ux, -fx, 0,
        sy, uy, -fy, 0,
        sz, uz, -fz, 0,
        0,  0,   0,  1
    };
    glMultMatrixd(m);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}
extern HTP::Doc g_doc;
namespace Script {
struct InputState {
    std::string text, placeholder;
    int x = 0, y = 0, w = 250, h = 28;
    bool focused = false;
    int cursor = 0;
};
std::map<std::string, InputState> g_inputs;
std::map<std::string, std::string> g_texts;
std::map<std::string, int> g_offsets_y;
std::map<std::string, float> g_shimmer_offsets;
std::map<std::string, std::function<void()>> g_clicks;
std::map<int, int> g_timerRefs;
std::map<std::string, int> g_canvasClickRefs;
std::map<std::string, int> g_canvasMouseMoveRefs;
int g_timerN = 9000;
int g_keyDownRef = LUA_NOREF;
lua_State* g_L = nullptr;
HWND g_hwnd = nullptr;
std::string g_focusId;
static int l_set_prop(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    const char* prop = luaL_checkstring(L, 2);
    const char* val = luaL_checkstring(L, 3);
    auto e = HTP::findById(g_doc.root, id);
    if (e) {
        e->props.d[prop] = val;
        invalidateContent();
    }
    return 0;
}

static int l_set_inner_htp(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    const char* htp = luaL_checkstring(L, 2);
    auto e = HTP::findById(g_doc.root, id);
    if (e) {
        HTP::Parser p;
        HTP::Doc temp = p.parse(htp);
        e->children = temp.root->children;
        invalidateContent();
    }
    return 0;
}
static int l_set_text(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    const char* t = luaL_checkstring(L, 2);
    std::string nv = t;
    auto& c = g_texts[id];
    if (c != nv) {
        c = nv;
        auto i = g_inputs.find(id);
        if (i != g_inputs.end()) i->second.text = nv;
        invalidateContent();
    }
    return 0;
}
static int l_get_text(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    auto i = g_texts.find(id);
    if (i != g_texts.end()) lua_pushstring(L, i->second.c_str());
    else {
        auto j = g_inputs.find(id);
        lua_pushstring(L, j != g_inputs.end() ? j->second.text.c_str() : "");
    }
    return 1;
}
static int l_set_offset_y(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    int o = (int)luaL_checkinteger(L, 2);
    auto& c = g_offsets_y[id];
    if (c != o) {
        c = o;
        invalidateContent();
    }
    return 0;
}
static int l_get_offset_y(lua_State* L) {
    auto i = g_offsets_y.find(luaL_checkstring(L, 1));
    lua_pushinteger(L, i != g_offsets_y.end() ? i->second : 0);
    return 1;
}
static int l_set_shimmer(lua_State* L) {
    g_shimmer_offsets[luaL_checkstring(L, 1)] = (float)luaL_checknumber(L, 2);
    invalidateContent();
    return 0;
}
static int l_on_click(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    std::string sid = id;
    g_clicks[sid] = [sid, ref]() {
        if (!g_L) return;
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, ref);
        if (lua_pcall(g_L, 0, 0, 0) != 0) {
            OutputDebugStringA(lua_tostring(g_L, -1));
            lua_pop(g_L, 1);
        }
    };
    return 0;
}
static int l_get_mouse(lua_State* L) {
    POINT p;
    GetCursorPos(&p);
    if (g_hwnd) ScreenToClient(g_hwnd, &p);
    lua_pushinteger(L, p.x);
    lua_pushinteger(L, p.y - TOOLBAR_H);
    return 2;
}
static int l_get_mouse_delta(lua_State* L) {
    lua_pushinteger(L, g_mouseDeltaX);
    lua_pushinteger(L, g_mouseDeltaY);
    g_mouseDeltaX = 0;
    g_mouseDeltaY = 0;
    return 2;
}
static int l_capture_mouse(lua_State* L) {
    const char* canvasId = luaL_checkstring(L, 1);
    auto v = GLCanvas::find(canvasId);
    if (!v || !v->valid) {
        lua_pushboolean(L, 0);
        return 1;
    }
    g_mouseCaptured = true;
    g_ignoreWarpMouse = false;
    g_capturedCanvasId = canvasId;
    g_capturedCanvasW = v->w;
    g_capturedCanvasH = v->h;
    g_capturedCanvasX = v->x;
    g_capturedCanvasY = v->y - TOOLBAR_H;
    SetCapture(g_hwnd);
    ShowCursor(FALSE);
    RECT wr;
    GetWindowRect(v->hwnd, &wr);
    ClipCursor(&wr);
    lua_pushboolean(L, 1);
    return 1;
}
static int l_release_mouse(lua_State* L) { (void)L; releaseMouse(); return 0; }
static int l_is_mouse_captured(lua_State* L) { lua_pushboolean(L, g_mouseCaptured ? 1 : 0); return 1; }
static int l_fullscreen_canvas(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    bool enter = lua_toboolean(L, 2);
    if (enter) enterFullscreenCanvas(id);
    else exitFullscreenCanvas();
    return 0;
}
static int l_is_fullscreen(lua_State* L) {
    lua_pushboolean(L, g_fullscreenCanvas ? 1 : 0);
    return 1;
}
static int l_window_active(lua_State* L) {
    HWND fg = GetForegroundWindow();
    BOOL active = (fg == g_hwnd) || (GetParent(fg) == g_hwnd);
    lua_pushboolean(L, active ? 1 : 0);
    return 1;
}
static int l_http(lua_State* L) {
    auto r = Net::fetchUrl(luaL_checkstring(L, 1));
    lua_pushstring(L, r.body.c_str());
    lua_pushinteger(L, r.code);
    return 2;
}
static int l_navigate(lua_State* L) { navigateTo(luaL_checkstring(L, 1)); return 0; }
static int l_alert(lua_State* L) { MessageBoxA(g_hwnd, luaL_checkstring(L, 1), "Lume", MB_OK); return 0; }
static int l_refresh(lua_State* L) { (void)L; invalidateContent(); return 0; }
static int l_get_input(lua_State* L) {
    auto i = g_inputs.find(luaL_checkstring(L, 1));
    lua_pushstring(L, i != g_inputs.end() ? i->second.text.c_str() : "");
    return 1;
}
static int l_set_input(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    const char* t = luaL_checkstring(L, 2);
    g_inputs[id].text = t;
    g_inputs[id].cursor = (int)strlen(t);
    invalidateContent();
    return 0;
}
static int l_key_down(lua_State* L) {
    int vk = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0);
    return 1;
}
static int l_cv_clear(lua_State* L) {
    auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1));
    if (i != Canvas::g_bufs.end() && i->second->dc) {
        auto c = HTP::Color::fromHex(luaL_optstring(L, 2, "#000000"));
        HBRUSH b = CreateSolidBrush(c.cr());
        RECT r = { 0,0,i->second->w,i->second->h };
        FillRect(i->second->dc, &r, b);
        DeleteObject(b);
    }
    return 0;
}
static int l_cv_rect(lua_State* L) {
    auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1));
    if (i != Canvas::g_bufs.end() && i->second->dc) {
        int x = (int)luaL_checkinteger(L, 2), y = (int)luaL_checkinteger(L, 3);
        int w = (int)luaL_checkinteger(L, 4), h = (int)luaL_checkinteger(L, 5);
        auto c = HTP::Color::fromHex(luaL_optstring(L, 6, "#ffffff"));
        HBRUSH b = CreateSolidBrush(c.cr());
        RECT r = { x,y,x + w,y + h };
        FillRect(i->second->dc, &r, b);
        DeleteObject(b);
    }
    return 0;
}
static int l_cv_circle(lua_State* L) {
    auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1));
    if (i != Canvas::g_bufs.end() && i->second->dc) {
        int cx = (int)luaL_checkinteger(L, 2), cy = (int)luaL_checkinteger(L, 3), r = (int)luaL_checkinteger(L, 4);
        auto c = HTP::Color::fromHex(luaL_optstring(L, 5, "#ffffff"));
        HBRUSH b = CreateSolidBrush(c.cr());
        HPEN p = CreatePen(PS_SOLID, 1, c.cr());
        auto ob = SelectObject(i->second->dc, b);
        auto op = SelectObject(i->second->dc, p);
        Ellipse(i->second->dc, cx - r, cy - r, cx + r, cy + r);
        SelectObject(i->second->dc, ob);
        SelectObject(i->second->dc, op);
        DeleteObject(b);
        DeleteObject(p);
    }
    return 0;
}
static int l_cv_line(lua_State* L) {
    auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1));
    if (i != Canvas::g_bufs.end() && i->second->dc) {
        int x1 = (int)luaL_checkinteger(L, 2), y1 = (int)luaL_checkinteger(L, 3);
        int x2 = (int)luaL_checkinteger(L, 4), y2 = (int)luaL_checkinteger(L, 5);
        auto c = HTP::Color::fromHex(luaL_optstring(L, 6, "#ffffff"));
        int t = (int)luaL_optinteger(L, 7, 1);
        HPEN p = CreatePen(PS_SOLID, t, c.cr());
        auto op = SelectObject(i->second->dc, p);
        MoveToEx(i->second->dc, x1, y1, 0);
        LineTo(i->second->dc, x2, y2);
        SelectObject(i->second->dc, op);
        DeleteObject(p);
    }
    return 0;
}
static int l_cv_text(lua_State* L) {
    auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1));
    if (i != Canvas::g_bufs.end() && i->second->dc) {
        int x = (int)luaL_checkinteger(L, 2), y = (int)luaL_checkinteger(L, 3);
        const char* txt = luaL_checkstring(L, 4);
        int sz = (int)luaL_optinteger(L, 5, 16);
        auto c = HTP::Color::fromHex(luaL_optstring(L, 6, "#ffffff"));
        HFONT f = FontCache::get(sz);
        auto of = SelectObject(i->second->dc, f);
        SetTextColor(i->second->dc, c.cr());
        SetBkMode(i->second->dc, TRANSPARENT);
        TextOutA(i->second->dc, x, y, txt, (int)strlen(txt));
        SelectObject(i->second->dc, of);
    }
    return 0;
}
static int l_gl_available(lua_State* L) {
    lua_pushboolean(L, 1);
    return 1;
}
static int l_gl_begin(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    int w = (int)luaL_optinteger(L, 2, 0);
    int h = (int)luaL_optinteger(L, 3, 0);
    lua_pushboolean(L, GLCanvas::beginRender(id, w, h) ? 1 : 0);
    return 1;
}
static int l_gl_end(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    GLCanvas::endRender(id);
    return 0;
}
static int l_gl_refresh(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    GLCanvas::refresh(id);
    return 0;
}
static int l_gl_clear(lua_State* L) {
    glClearColor((float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0), (float)luaL_optnumber(L, 4, 1));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return 0;
}
static int l_gl_viewport(lua_State* L) { glViewport((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4)); return 0; }
static int l_gl_color(lua_State* L) { glColor4f((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_optnumber(L, 4, 1)); return 0; }
static int l_gl_begin_prim(lua_State* L) { glBegin((GLenum)luaL_checkinteger(L, 1)); return 0; }
static int l_gl_end_prim(lua_State* L) { (void)L; glEnd(); return 0; }
static int l_gl_vertex2f(lua_State* L) { glVertex2f((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)); return 0; }
static int l_gl_vertex3f(lua_State* L) { glVertex3f((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)); return 0; }
static int l_gl_ortho(lua_State* L) { glOrtho(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_checknumber(L, 6)); return 0; }
static int l_gl_frustum(lua_State* L) { glFrustum(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_checknumber(L, 6)); return 0; }
static int l_gl_load_identity(lua_State* L) { (void)L; glLoadIdentity(); return 0; }
static int l_gl_matrix_mode(lua_State* L) { glMatrixMode((GLenum)luaL_checkinteger(L, 1)); return 0; }
static int l_gl_push_matrix(lua_State* L) { (void)L; glPushMatrix(); return 0; }
static int l_gl_pop_matrix(lua_State* L) { (void)L; glPopMatrix(); return 0; }
static int l_gl_translatef(lua_State* L) { glTranslatef((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)); return 0; }
static int l_gl_rotatef(lua_State* L) { glRotatef((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)); return 0; }
static int l_gl_scalef(lua_State* L) { glScalef((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)); return 0; }
static int l_gl_enable(lua_State* L) { glEnable((GLenum)luaL_checkinteger(L, 1)); return 0; }
static int l_gl_disable(lua_State* L) { glDisable((GLenum)luaL_checkinteger(L, 1)); return 0; }
static int l_gl_line_width(lua_State* L) { glLineWidth((float)luaL_checknumber(L, 1)); return 0; }
static int l_gl_point_size(lua_State* L) { glPointSize((float)luaL_checknumber(L, 1)); return 0; }
static int l_gl_normal3f(lua_State* L) { glNormal3f((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3)); return 0; }
static int l_glu_perspective(lua_State* L) { gluPerspective_impl(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4)); return 0; }
static int l_glu_look_at(lua_State* L) {
    gluLookAt_impl(
        luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3),
        luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_checknumber(L, 6),
        luaL_checknumber(L, 7), luaL_checknumber(L, 8), luaL_checknumber(L, 9)
    );
    return 0;
}
static int l_gl_gen_list(lua_State* L) { lua_pushinteger(L, (int)glGenLists(1)); return 1; }
static int l_gl_new_list(lua_State* L) { glNewList((GLuint)luaL_checkinteger(L, 1), GL_COMPILE); return 0; }
static int l_gl_end_list(lua_State* L) { (void)L; glEndList(); return 0; }
static int l_gl_call_list(lua_State* L) { glCallList((GLuint)luaL_checkinteger(L, 1)); return 0; }
static int l_gl_delete_list(lua_State* L) { glDeleteLists((GLuint)luaL_checkinteger(L, 1), 1); return 0; }
static int l_set_timer(lua_State* L) {
    int ms = (int)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    int tid = ++g_timerN;
    g_timerRefs[tid] = ref;
    SetTimer(g_hwnd, tid, ms, [](HWND, UINT, UINT_PTR id, DWORD) {
        if (!g_hwnd || IsIconic(g_hwnd)) return;

        auto i = g_timerRefs.find((int)id);
        if (i == g_timerRefs.end() || !g_L) return;

        lua_rawgeti(g_L, LUA_REGISTRYINDEX, i->second);
        if (lua_pcall(g_L, 0, 0, 0) != 0) {
            const char* err = lua_tostring(g_L, -1);
            OutputDebugStringA(err ? err : "Lua timer error\n");
            MessageBoxA(g_hwnd, err ? err : "Unknown error", "Lua Timer Error", MB_OK | MB_ICONERROR);
            lua_pop(g_L, 1);
        }
    });
    lua_pushinteger(L, tid);
    return 1;
}
static int l_kill_timer(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    KillTimer(g_hwnd, id);
    auto it = g_timerRefs.find(id);
    if (it != g_timerRefs.end()) {
        if (g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, it->second);
        g_timerRefs.erase(it);
    }
    return 0;
}
static int l_on_key_down(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    if (g_keyDownRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, g_keyDownRef);
    lua_pushvalue(L, 1);
    g_keyDownRef = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}
static int l_get_time(lua_State* L) {
    lua_pushnumber(L, getHighResTime());
    return 1;
}
static int l_get_window_size(lua_State* L) {
    RECT cr;
    GetClientRect(g_hwnd, &cr);
    if (g_fullscreenCanvas) {
        lua_pushinteger(L, cr.right);
        lua_pushinteger(L, cr.bottom);
    }
    else {
        lua_pushinteger(L, cr.right);
        lua_pushinteger(L, cr.bottom - TOOLBAR_H - STATUS_H);
    }
    return 2;
}
void fireCanvasClick(const std::string& canvasId, int x, int y, int button) {
    auto it = g_canvasClickRefs.find(canvasId);
    if (it == g_canvasClickRefs.end() || !g_L) return;
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, it->second);
    lua_pushinteger(g_L, x);
    lua_pushinteger(g_L, y);
    lua_pushinteger(g_L, button);
    if (lua_pcall(g_L, 3, 0, 0) != 0) {
        OutputDebugStringA(lua_tostring(g_L, -1));
        lua_pop(g_L, 1);
    }
}
static int l_on_canvas_click(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    std::string sid = id;
    auto it = g_canvasClickRefs.find(sid);
    if (it != g_canvasClickRefs.end()) {
        luaL_unref(L, LUA_REGISTRYINDEX, it->second);
    }
    lua_pushvalue(L, 2);
    g_canvasClickRefs[sid] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}
static int l_gl_blend_func(lua_State* L) {
    glBlendFunc((GLenum)luaL_checkinteger(L, 1), (GLenum)luaL_checkinteger(L, 2));
    return 0;
}
static int l_glu_sphere(lua_State* L) {
    double radius = luaL_checknumber(L, 1);
    int slices = (int)luaL_checkinteger(L, 2);
    int stacks = (int)luaL_checkinteger(L, 3);
    GLUquadric* q = gluNewQuadric();
    gluSphere(q, radius, slices, stacks);
    gluDeleteQuadric(q);
    return 0;
}
static int l_gl_bind_texture(lua_State* L) {
    glBindTexture(GL_TEXTURE_2D, (GLuint)luaL_checkinteger(L, 1)); return 0;
}
static int l_gl_text_coord2f(lua_State* L) {
    glTexCoord2f((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)); return 0;
}
static int l_gl_load_texture(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    std::wstring wpath(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &wpath[0], len);
    Gdiplus::Bitmap bmp(wpath.c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        lua_pushnil(L);
        return 1;
    }
    int w = bmp.GetWidth();
    int h = bmp.GetHeight();
    std::vector<unsigned char> pixels;
    try {
        pixels.resize(w * h * 4);
    }
    catch (...) {
        lua_pushnil(L);
        return 1;
    }
    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData bmpData;
    bmp.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData);
    const unsigned char* src = (const unsigned char*)bmpData.Scan0;
    int stride = bmpData.Stride;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * stride + x * 4;
            int p = (y * w + x) * 4;
            pixels[p + 0] = src[i + 2];
            pixels[p + 1] = src[i + 1];
            pixels[p + 2] = src[i + 0];
            pixels[p + 3] = src[i + 3];
        }
    }
    bmp.UnlockBits(&bmpData);
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    lua_pushinteger(L, texID);
    return 1;
}
static int l_gl_load_obj(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    std::ifstream file(path);
    if (!file) {
        lua_pushnil(L);
        return 1;
    }
    std::vector<float> verts;
    GLuint listId = glGenLists(1);
    glNewList(listId, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    std::string line;
    while (std::getline(file, line)) {
        if (line.length() >= 2 && line.substr(0, 2) == "v ") {
            float x, y, z;
            if (sscanf(line.c_str(), "v %f %f %f", &x, &y, &z) == 3) {
                verts.push_back(x);
                verts.push_back(y);
                verts.push_back(z);
            }
        }
        else if (line.length() >= 2 && line.substr(0, 2) == "f ") {
            std::istringstream iss(line.substr(2));
            std::string token;
            std::vector<int> face_verts;
            while (iss >> token) {
                int v = 0;
                if (sscanf(token.c_str(), "%d", &v) == 1) {
                    face_verts.push_back(v);
                }
            }
            if (face_verts.size() >= 3) {
                glVertex3f(verts[(face_verts[0] - 1) * 3], verts[(face_verts[0] - 1) * 3 + 1], verts[(face_verts[0] - 1) * 3 + 2]);
                glVertex3f(verts[(face_verts[1] - 1) * 3], verts[(face_verts[1] - 1) * 3 + 1], verts[(face_verts[1] - 1) * 3 + 2]);
                glVertex3f(verts[(face_verts[2] - 1) * 3], verts[(face_verts[2] - 1) * 3 + 1], verts[(face_verts[2] - 1) * 3 + 2]);
                if (face_verts.size() >= 4) {
                    glVertex3f(verts[(face_verts[0] - 1) * 3], verts[(face_verts[0] - 1) * 3 + 1], verts[(face_verts[0] - 1) * 3 + 2]);
                    glVertex3f(verts[(face_verts[2] - 1) * 3], verts[(face_verts[2] - 1) * 3 + 1], verts[(face_verts[2] - 1) * 3 + 2]);
                    glVertex3f(verts[(face_verts[3] - 1) * 3], verts[(face_verts[3] - 1) * 3 + 1], verts[(face_verts[3] - 1) * 3 + 2]);
                }
            }
        }
    }
    glEnd();
    glEndList();
    lua_pushinteger(L, listId);
    return 1;
}
static int l_gl_delete_texture(lua_State* L) {
    GLuint texID = (GLuint)luaL_checkinteger(L, 1);
    glDeleteTextures(1, &texID);
    return 0;
}
static int l_gl_tex_parameteri(lua_State* L) {
    glTexParameteri((GLenum)luaL_checkinteger(L, 1), (GLenum)luaL_checkinteger(L, 2), (GLint)luaL_checkinteger(L, 3));
    return 0;
}
static int l_gl_lightfv(lua_State* L) {
    GLenum light = (GLenum)luaL_checkinteger(L, 1);
    GLenum pname = (GLenum)luaL_checkinteger(L, 2);
    GLfloat params[4] = {
        (GLfloat)luaL_checknumber(L, 3),
        (GLfloat)luaL_checknumber(L, 4),
        (GLfloat)luaL_checknumber(L, 5),
        (GLfloat)luaL_optnumber(L, 6, 1.0)
    };
    glLightfv(light, pname, params);
    return 0;
}
static int l_gl_materialfv(lua_State* L) {
    GLenum face = (GLenum)luaL_checkinteger(L, 1);
    GLenum pname = (GLenum)luaL_checkinteger(L, 2);
    GLfloat params[4] = {
        (GLfloat)luaL_checknumber(L, 3),
        (GLfloat)luaL_optnumber(L, 4, 0.0),
        (GLfloat)luaL_optnumber(L, 5, 0.0),
        (GLfloat)luaL_optnumber(L, 6, 1.0)
    };
    glMaterialfv(face, pname, params);
    return 0;
}
static int l_download_async(lua_State* L) {
    std::string url = luaL_checkstring(L, 1);
    std::string fname = luaL_checkstring(L, 2);
    AsyncNet::startDownload(url, fname);
    return 0;
}
static int l_download_status(lua_State* L) {
    std::string url = luaL_checkstring(L, 1);
    std::lock_guard<std::mutex> lock(AsyncNet::g_dlMutex);
    auto it = AsyncNet::g_downloads.find(url);
    if (it != AsyncNet::g_downloads.end()) {
        lua_pushinteger(L, it->second.status);
        lua_pushstring(L, it->second.localPath.c_str());
        lua_pushinteger(L, (lua_Integer)it->second.downloadedBytes);
        lua_pushinteger(L, (lua_Integer)it->second.totalBytes);
        return 4;
    }
    lua_pushinteger(L, -404);
    return 1;
}
void registerGLConstants(lua_State* L) {
    struct { const char* n; int v; } cs[] = {
        {"GL_POINTS",GL_POINTS},{"GL_LINES",GL_LINES},{"GL_LINE_STRIP",GL_LINE_STRIP},{"GL_LINE_LOOP",GL_LINE_LOOP},
        {"GL_TRIANGLES",GL_TRIANGLES},{"GL_TRIANGLE_STRIP",GL_TRIANGLE_STRIP},{"GL_TRIANGLE_FAN",GL_TRIANGLE_FAN},
        {"GL_QUADS",GL_QUADS},{"GL_POLYGON",GL_POLYGON},{"GL_MODELVIEW",GL_MODELVIEW},{"GL_PROJECTION",GL_PROJECTION},
        {"GL_DEPTH_TEST",GL_DEPTH_TEST},{"GL_BLEND",GL_BLEND},{"GL_LINE_SMOOTH",GL_LINE_SMOOTH},
        {"GL_LIGHTING",0x0B50},{"GL_LIGHT0",0x4000},{"GL_COLOR_MATERIAL",0x0B57},{"GL_CULL_FACE",0x0B44},
        {"VK_LEFT",VK_LEFT},{"VK_RIGHT",VK_RIGHT},{"VK_UP",VK_UP},{"VK_DOWN",VK_DOWN},
        {"VK_W",0x57},{"VK_A",0x41},{"VK_S",0x53},{"VK_D",0x44},
        {"VK_SPACE",VK_SPACE},{"VK_SHIFT",VK_SHIFT},{"VK_CONTROL",VK_CONTROL},
        {"VK_ESCAPE",VK_ESCAPE},{"VK_RETURN",VK_RETURN},
        {"VK_Q",0x51},{"VK_E",0x45},{"VK_R",0x52},{"VK_F",0x46},
        {"VK_LBUTTON",VK_LBUTTON},{"VK_F11",VK_F11},{"VK_P",0x50},
        {"GL_SRC_ALPHA", 0x0302},
        {"GL_ONE_MINUS_SRC_ALPHA", 0x0303},
        {"GL_ONE", 1},
        {"GL_ZERO", 0},
        {"GL_TEXTURE_2D", 0x0DE1},
        {"GL_TEXTURE_MIN_FILTER", 0x2801},
        {"GL_TEXTURE_MAG_FILTER", 0x2800},
        {"GL_NEAREST", 0x2600},
        {"GL_LINEAR", 0x2601},
        {"GL_POSITION", 0x1203},
        {"GL_AMBIENT", 0x1200},
        {"GL_DIFFUSE", 0x1201},
        {"GL_SPECULAR", 0x1202},
        {"GL_SHININESS", 0x1601},
        {"GL_FRONT", 0x0404},
        {"GL_BACK", 0x0405},
        {"GL_FRONT_AND_BACK", 0x0408},
        {"GL_LIGHT1", 0x4001},
        {"GL_LIGHT2", 0x4002},
        {nullptr,0}
    };
    for (int i = 0; cs[i].n; i++) {
        lua_pushinteger(L, cs[i].v);
        lua_setglobal(L, cs[i].n);
    }
}
void init() {
    if (g_L) lua_close(g_L);
    g_L = luaL_newstate();
    luaL_requiref(g_L, "_G", luaopen_base, 1); lua_pop(g_L, 1);
    luaL_requiref(g_L, "table", luaopen_table, 1); lua_pop(g_L, 1);
    luaL_requiref(g_L, "string", luaopen_string, 1); lua_pop(g_L, 1);
    luaL_requiref(g_L, "math", luaopen_math, 1); lua_pop(g_L, 1);
    lua_register(g_L, "set_prop", l_set_prop);
    lua_register(g_L, "set_inner_htp", l_set_inner_htp);
    lua_register(g_L, "set_text", l_set_text);
    lua_register(g_L, "get_text", l_get_text);
    lua_register(g_L, "set_offset_y", l_set_offset_y);
    lua_register(g_L, "get_offset_y", l_get_offset_y);
    lua_register(g_L, "set_shimmer", l_set_shimmer);
    lua_register(g_L, "on_click", l_on_click);
    lua_register(g_L, "get_mouse_pos", l_get_mouse);
    lua_register(g_L, "get_mouse_delta", l_get_mouse_delta);
    lua_register(g_L, "capture_mouse", l_capture_mouse);
    lua_register(g_L, "release_mouse", l_release_mouse);
    lua_register(g_L, "is_mouse_captured", l_is_mouse_captured);
    lua_register(g_L, "fullscreen_canvas", l_fullscreen_canvas);
    lua_register(g_L, "is_fullscreen", l_is_fullscreen);
    lua_register(g_L, "window_active", l_window_active);
    lua_register(g_L, "http_request", l_http);
    lua_register(g_L, "navigate", l_navigate);
    lua_register(g_L, "alert", l_alert);
    lua_register(g_L, "refresh", l_refresh);
    lua_register(g_L, "get_input", l_get_input);
    lua_register(g_L, "set_input", l_set_input);
    lua_register(g_L, "key_down", l_key_down);
    lua_register(g_L, "canvas_clear", l_cv_clear);
    lua_register(g_L, "canvas_rect", l_cv_rect);
    lua_register(g_L, "canvas_circle", l_cv_circle);
    lua_register(g_L, "canvas_line", l_cv_line);
    lua_register(g_L, "canvas_text", l_cv_text);
    lua_register(g_L, "set_timer", l_set_timer);
    lua_register(g_L, "kill_timer", l_kill_timer);
    lua_register(g_L, "on_key_down", l_on_key_down);
    lua_register(g_L, "gl_available", l_gl_available);
    lua_register(g_L, "gl_begin_render", l_gl_begin);
    lua_register(g_L, "gl_end_render", l_gl_end);
    lua_register(g_L, "gl_refresh", l_gl_refresh);
    lua_register(g_L, "gl_clear", l_gl_clear);
    lua_register(g_L, "gl_viewport", l_gl_viewport);
    lua_register(g_L, "gl_color", l_gl_color);
    lua_register(g_L, "gl_begin", l_gl_begin_prim);
    lua_register(g_L, "gl_end", l_gl_end_prim);
    lua_register(g_L, "gl_vertex2f", l_gl_vertex2f);
    lua_register(g_L, "gl_vertex3f", l_gl_vertex3f);
    lua_register(g_L, "gl_ortho", l_gl_ortho);
    lua_register(g_L, "gl_frustum", l_gl_frustum);
    lua_register(g_L, "gl_load_identity", l_gl_load_identity);
    lua_register(g_L, "gl_matrix_mode", l_gl_matrix_mode);
    lua_register(g_L, "gl_push_matrix", l_gl_push_matrix);
    lua_register(g_L, "gl_pop_matrix", l_gl_pop_matrix);
    lua_register(g_L, "gl_translatef", l_gl_translatef);
    lua_register(g_L, "gl_rotatef", l_gl_rotatef);
    lua_register(g_L, "gl_scalef", l_gl_scalef);
    lua_register(g_L, "gl_enable", l_gl_enable);
    lua_register(g_L, "gl_disable", l_gl_disable);
    lua_register(g_L, "gl_line_width", l_gl_line_width);
    lua_register(g_L, "gl_point_size", l_gl_point_size);
    lua_register(g_L, "gl_normal3f", l_gl_normal3f);
    lua_register(g_L, "glu_perspective", l_glu_perspective);
    lua_register(g_L, "glu_look_at", l_glu_look_at);
    lua_register(g_L, "gl_gen_list", l_gl_gen_list);
    lua_register(g_L, "gl_new_list", l_gl_new_list);
    lua_register(g_L, "gl_end_list", l_gl_end_list);
    lua_register(g_L, "gl_call_list", l_gl_call_list);
    lua_register(g_L, "gl_delete_list", l_gl_delete_list);
    lua_register(g_L, "get_time", l_get_time);
    lua_register(g_L, "get_window_size", l_get_window_size);
    lua_register(g_L, "on_canvas_click", l_on_canvas_click);
    lua_register(g_L, "gl_blend_func", l_gl_blend_func);
    lua_register(g_L, "glu_sphere", l_glu_sphere);
    lua_register(g_L, "gl_bind_texture", l_gl_bind_texture);
    lua_register(g_L, "gl_text_coord2f", l_gl_text_coord2f);
    lua_register(g_L, "gl_load_texture", l_gl_load_texture);
    lua_register(g_L, "gl_load_obj", l_gl_load_obj);
    lua_register(g_L, "download_async", l_download_async);
    lua_register(g_L, "download_status", l_download_status);
    lua_register(g_L, "gl_delete_texture", l_gl_delete_texture);
    lua_register(g_L, "gl_tex_parameteri", l_gl_tex_parameteri);
    lua_register(g_L, "gl_lightfv", l_gl_lightfv);
    lua_register(g_L, "gl_materialfv", l_gl_materialfv);
    registerGLConstants(g_L);
    Plugins::initAllPlugins(g_L);
}
void exec(const std::string& code) {
    if (!g_L) init();
    if (luaL_dostring(g_L, code.c_str()) != 0) {
        const char* e = lua_tostring(g_L, -1);
        OutputDebugStringA(e ? e : "lua err");
        OutputDebugStringA("\n");
        MessageBoxA(g_hwnd, e ? e : "Unknown error", "Lua Error", MB_OK | MB_ICONERROR);
        lua_pop(g_L, 1);
    }
}
void fireClick(const std::string& id) {
    auto i = g_clicks.find(id);
    if (i != g_clicks.end()) i->second();
}
void reset() {
    releaseMouse();
    if (g_fullscreenCanvas) exitFullscreenCanvas();
    for (auto& kv : g_timerRefs) {
        KillTimer(g_hwnd, kv.first);
        if (g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, kv.second);
    }
    for (auto& kv : g_canvasClickRefs) {
        if (g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, kv.second);
    }
    g_canvasClickRefs.clear();
    g_timerRefs.clear();
    g_timerN = 9000;
    g_clicks.clear();
    g_texts.clear();
    g_offsets_y.clear();
    g_shimmer_offsets.clear();
    g_inputs.clear();
    g_focusId.clear();
    if (g_keyDownRef != LUA_NOREF && g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, g_keyDownRef);
    g_keyDownRef = LUA_NOREF;
    Canvas::g_bufs.clear();
    GLCanvas::destroyAll();
    if (g_L) {
        lua_close(g_L);
        g_L = nullptr;
    }
}
}
static std::string g_curUrl;
std::string resolveUrl(const std::string& url, const std::string& baseUrl) {
    if (url.find("://") != std::string::npos || url.substr(0, 6) == "about:") return url;
    if (url.substr(0, 7) == "file://") return url;
    if (url.find('/') != std::string::npos && url.find("://") == std::string::npos) {
        std::string proto = "http";
        auto pe = baseUrl.find("://");
        if (pe != std::string::npos) proto = baseUrl.substr(0, pe);
        if (!url.empty() && url[0] == '/') {
            if (pe != std::string::npos) {
                std::string rest = baseUrl.substr(pe + 3);
                auto slash = rest.find('/');
                std::string host = (slash != std::string::npos) ? rest.substr(0, slash) : rest;
                return proto + "://" + host + url;
            }
        }
        return proto + "://" + url;
    }
    if (baseUrl.find("://") != std::string::npos) {
        std::string proto = baseUrl.substr(0, baseUrl.find("://"));
        std::string rest = baseUrl.substr(baseUrl.find("://") + 3);
        auto lastSlash = rest.rfind('/');
        if (lastSlash != std::string::npos) return proto + "://" + rest.substr(0, lastSlash + 1) + url;
        return proto + "://" + rest + "/" + url;
    }
    return "http://" + url;
}
namespace Render {
struct Hit {
    RECT r;
    std::string url, action, elemId;
    bool isInput = false, isBtn = false, isGLCanvas = false;
    std::string canvasId;
    int canvasW = 0, canvasH = 0;
    int zIndex = 0;
};
class Engine {
    struct RenderCmd {
        int zIndex;
        std::function<void()> drawCall;
    };
    std::vector<RenderCmd> renderQueue;
    int scrollY = 0, contentH = 0, curY = 0;
    std::vector<Hit>* pH = nullptr;
    HDC refDC = nullptr;
    void fillRR(HDC dc, int x, int y, int w, int h, HTP::Color c, int rad = 5) {
        HBRUSH b = CreateSolidBrush(c.cr());
        HPEN p = CreatePen(PS_SOLID, 1, c.cr());
        auto ob = SelectObject(dc, b);
        auto op = SelectObject(dc, p);
        RoundRect(dc, x, y, x + w, y + h, rad, rad);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(b);
        DeleteObject(p);
    }
    int estH(std::shared_ptr<HTP::Elem> e) {
        if (!e) return 0;
        switch (e->type) {
        case HTP::EType::TEXT:
        case HTP::EType::ITEM: return e->props.getInt("size", 16) + 8;
        case HTP::EType::LINK: return e->props.getInt("size", 16) + 8;
        case HTP::EType::BUTTON: return e->props.getInt("height", 35) + 8;
        case HTP::EType::INPUT_FIELD: return e->props.getInt("height", 28) + 8;
        case HTP::EType::DIVIDER: return e->props.getInt("margin", 10) * 2 + e->props.getInt("thickness", 1);
        case HTP::EType::IMAGE: return e->props.getInt("height", 100) + 8;
        case HTP::EType::BR: return e->props.getInt("size", 16);
        case HTP::EType::CANVAS:
        case HTP::EType::GL_CANVAS: return e->props.getInt("height", 200) + 8;
        case HTP::EType::SCRIPT: return 0;
        case HTP::EType::BLOCK:
        case HTP::EType::COLUMN: {
            int h = e->props.getInt("padding", 10) * 2 + e->props.getInt("margin", 5) * 2;
            for (auto& c : e->children) h += estH(c);
            return h;
        }
        case HTP::EType::ROW: {
            int m = 0;
            for (auto& c : e->children) m = (std::max)(m, estH(c));
            return m + e->props.getInt("margin", 5) * 2;
        }
        case HTP::EType::LIST: {
            int h = 0;
            for (auto& c : e->children) h += estH(c);
            return h;
        }
        default: return 20;
        }
    }
    int estW(std::shared_ptr<HTP::Elem> e, int mw) {
        if (!e) return mw;
        switch (e->type) {
        case HTP::EType::BUTTON: return e->props.getInt("width", 120);
        case HTP::EType::INPUT_FIELD: return e->props.getInt("width", 250);
        case HTP::EType::IMAGE: return e->props.getInt("width", 100);
        case HTP::EType::CANVAS:
        case HTP::EType::GL_CANVAS: return e->props.getInt("width", 400);
        default: return mw;
        }
    }
    void draw(HDC dc, std::shared_ptr<HTP::Elem> e, int x, int y, int mw) {
        if (!e) return;
        switch (e->type) {
        case HTP::EType::PAGE:
        case HTP::EType::UNKNOWN: {
            int cy = y;
            for (auto& c : e->children) {
                draw(dc, c, x, cy, mw);
                cy = curY;
            }
            break;
        }
        case HTP::EType::SCRIPT:
            curY = y;
            break;
        case HTP::EType::ROW: {
            int mar = e->props.getInt("margin", 5);
            int pad = e->props.getInt("padding", 0);
            int gap = e->props.getInt("gap", 10);
            int nc = 0, fw = 0, fc = 0;
            for (auto& c : e->children) {
                nc++;
                int w = c->props.getInt("width", 0);
                if (w > 0) fw += w; else fc++;
            }
            int avail = mw - mar * 2 - pad * 2 - gap * (std::max)(nc - 1, 0);
            int flexW = fc > 0 ? (avail - fw) / fc : 0;
            if (flexW < 50) flexW = 50;
            int cx = x + mar + pad;
            int ry = y + mar;
            int maxH = 0;
            for (auto& c : e->children) {
                int cw = c->props.getInt("width", flexW);
                int sv = curY;
                curY = ry;
                draw(dc, c, cx, ry, cw);
                int ch = curY - ry;
                if (ch > maxH) maxH = ch;
                cx += cw + gap;
                curY = sv;
            }
            curY = ry + maxH + mar;
            break;
        }
        case HTP::EType::COLUMN: {
            int pad = e->props.getInt("padding", 5);
            int rad = e->props.getInt("border-radius", 4);
            std::string bg = e->props.get("background", "");
            int z = e->props.getInt("z-index", 0);
            auto realHeight = std::make_shared<int>(0);
            if (!bg.empty()) {
                renderQueue.push_back({ z, [=]() {
                    fillRR(dc, x, y - scrollY, mw, *realHeight, HTP::Color::fromHex(bg), rad);
                } });
            }
            curY = y + pad;
            for (auto& c : e->children) {
                draw(dc, c, x + pad, curY, mw - pad * 2);
            }
            *realHeight = (curY - y) + pad;
            curY += pad;
            break;
        }
        case HTP::EType::BLOCK: {
            int pad = e->props.getInt("padding", 10);
            int mar = e->props.getInt("margin", 5);
            int rad = e->props.getInt("border-radius", 6);
            std::string bg = e->props.get("background", "");
            std::string align = e->props.get("align", "left");
            int z = e->props.getInt("z-index", 0);
            auto realHeight = std::make_shared<int>(0);
            int startY = y + mar;
            if (!bg.empty()) {
                renderQueue.push_back({ z, [=]() {
                    fillRR(dc, x + mar, startY - scrollY, mw - mar * 2, *realHeight, HTP::Color::fromHex(bg), rad);
                } });
            }
            curY = startY + pad;
            for (auto& c : e->children) {
                int cx = x + mar + pad;
                int cmw = mw - (mar + pad) * 2;
                if (align == "center") {
                    int ew = estW(c, cmw);
                    cx = x + (mw - ew) / 2;
                }
                draw(dc, c, cx, curY, cmw);
            }
            *realHeight = (curY - startY) + pad;
            curY += pad + mar;
            break;
        }
        case HTP::EType::TEXT: {
            std::string id = e->props.get("id", "");
            std::string ct = e->props.get("content", "");
            if (!id.empty()) {
                auto i = Script::g_texts.find(id);
                if (i != Script::g_texts.end()) ct = i->second;
                else Script::g_texts[id] = ct;
            }
            int sz = e->props.getInt("size", 16);
            auto col = e->props.getColor("color", { 255,255,255 });
            std::string grad = e->props.get("gradient", "");
            int offset_y = 0;
            if (!id.empty()) {
                auto oi = Script::g_offsets_y.find(id);
                if (oi != Script::g_offsets_y.end()) offset_y = oi->second;
            }
            float shimmer = 0.0f;
            if (!id.empty()) {
                auto si = Script::g_shimmer_offsets.find(id);
                if (si != Script::g_shimmer_offsets.end()) shimmer = si->second;
            }
            int z = e->props.getInt("z-index", 0);
            HFONT f = FontCache::get(sz,
                e->props.getBool("bold"),
                e->props.getBool("italic"),
                e->props.get("font", "Segoe UI").c_str());
            int drawY = y - scrollY + offset_y;
            int th = 0;
            if (grad.empty()) {
                auto of = SelectObject(dc, f);
                RECT rcCalc = { x, drawY, x + mw, drawY + 1000 };
                DrawTextA(dc, ct.c_str(), -1, &rcCalc, DT_WORDBREAK | DT_CALCRECT);
                th = rcCalc.bottom - rcCalc.top;
                SelectObject(dc, of);
            }
            renderQueue.push_back({ z, [=]() {
                if (!grad.empty()) {
                    GradientText::draw(dc, ct, f, x, drawY, col, HTP::Color::fromHex(grad), shimmer);
                }
                else {
                    auto of = SelectObject(dc, f);
                    SetTextColor(dc, col.cr());
                    SetBkMode(dc, TRANSPARENT);
                    RECT rcDraw = { x, drawY, x + mw, drawY + th };
                    DrawTextA(dc, ct.c_str(), -1, &rcDraw, DT_WORDBREAK);
                    SelectObject(dc, of);
                }
            } });
            curY = y + (grad.empty() ? th : sz) + 4;
            break;
        }
        case HTP::EType::LINK: {
            std::string ct = e->props.get("content", "[link]");
            std::string url = resolveUrl(e->props.get("url", ""), g_curUrl);
            int sz = e->props.getInt("size", 16);
            auto col = e->props.getColor("color", { 10,189,227 });
            int z = e->props.getInt("z-index", 0);
            HFONT f = FontCache::get(sz);
            auto of = SelectObject(dc, f);
            SIZE ts;
            GetTextExtentPoint32A(dc, ct.c_str(), (int)ct.length(), &ts);
            SelectObject(dc, of);
            renderQueue.push_back({ z, [=]() {
                auto of2 = SelectObject(dc, f);
                SetTextColor(dc, col.cr());
                SetBkMode(dc, TRANSPARENT);
                RECT rc = { x, y - scrollY, x + ts.cx, y - scrollY + ts.cy };
                DrawTextA(dc, ct.c_str(), -1, &rc, 0);
                HPEN p = CreatePen(PS_SOLID, 1, col.cr());
                auto op = SelectObject(dc, p);
                MoveToEx(dc, x, y - scrollY + ts.cy - 1, 0);
                LineTo(dc, x + ts.cx, y - scrollY + ts.cy - 1);
                SelectObject(dc, op);
                DeleteObject(p);
                SelectObject(dc, of2);
            } });
            if (pH) {
                Hit h;
                h.r = { x, y - scrollY, x + ts.cx, y - scrollY + ts.cy };
                h.url = url;
                pH->push_back(h);
            }
            curY = y + ts.cy + 4;
            break;
        }
        case HTP::EType::BUTTON: {
            std::string id = e->props.get("id", "");
            std::string ct = e->props.get("content", "Button");
            std::string act = e->props.get("action", "");
            std::string url = e->props.get("url", act);
            if (!url.empty()) url = resolveUrl(url, g_curUrl);
            int bw = e->props.getInt("width", 120);
            int bh = e->props.getInt("height", 35);
            int sz = e->props.getInt("size", 14);
            int rad = e->props.getInt("border-radius", 6);
            auto bg = e->props.getColor("background", { 233,69,96 });
            auto col = e->props.getColor("color", { 255,255,255 });
            int z = e->props.getInt("z-index", 0);
            int bx = x, by = y - scrollY;
            renderQueue.push_back({ z, [=]() {
                fillRR(dc, bx, by, bw, bh, bg, rad);
                HFONT f = FontCache::get(sz, true);
                auto of = SelectObject(dc, f);
                SetTextColor(dc, col.cr());
                SetBkMode(dc, TRANSPARENT);
                RECT rc = { bx, by, bx + bw, by + bh };
                DrawTextA(dc, ct.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(dc, of);
            } });
            if (pH) {
                Hit h;
                h.r = { bx, by, bx + bw, by + bh };
                h.url = url;
                h.action = act;
                h.elemId = id;
                h.isBtn = true;
                pH->push_back(h);
            }
            curY = y + bh + 8;
            break;
        }
        case HTP::EType::INPUT_FIELD: {
            std::string id = e->props.get("id", "inp_" + std::to_string(y));
            std::string ph = e->props.get("placeholder", "");
            int iw = e->props.getInt("width", 250);
            int ih = e->props.getInt("height", 28);
            int z = e->props.getInt("z-index", 0);
            auto& inp = Script::g_inputs[id];
            if (inp.placeholder.empty() && !ph.empty()) inp.placeholder = ph;
            inp.x = x; inp.y = y - scrollY; inp.w = iw; inp.h = ih;
            int ix = x, iy = y - scrollY;
            bool foc = (Script::g_focusId == id);
            std::string textToDraw = inp.text;
            std::string phToDraw = inp.placeholder;
            int cursorToDraw = inp.cursor;
            renderQueue.push_back({ z, [=]() {
                HBRUSH br = CreateSolidBrush(foc ? RGB(50, 50, 80) : RGB(40, 40, 60));
                HPEN pn = CreatePen(PS_SOLID, foc ? 2 : 1, foc ? RGB(10, 189, 227) : RGB(100, 100, 140));
                auto ob = SelectObject(dc, br);
                auto op = SelectObject(dc, pn);
                RoundRect(dc, ix, iy, ix + iw, iy + ih, 4, 4);
                SelectObject(dc, ob);
                SelectObject(dc, op);
                DeleteObject(br);
                DeleteObject(pn);
                HFONT f = FontCache::get(14);
                auto of = SelectObject(dc, f);
                SetBkMode(dc, TRANSPARENT);
                if (!textToDraw.empty()) {
                    SetTextColor(dc, RGB(230, 230, 240));
                    RECT rc = { ix + 6, iy + 2, ix + iw - 4, iy + ih - 2 };
                    DrawTextA(dc, textToDraw.c_str(), -1, &rc, DT_VCENTER | DT_SINGLELINE);
                    if (foc) {
                        int cp = (std::min)(cursorToDraw, (int)textToDraw.length());
                        SIZE ts;
                        GetTextExtentPoint32A(dc, textToDraw.c_str(), cp, &ts);
                        HPEN cpen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                        auto ocp = SelectObject(dc, cpen);
                        MoveToEx(dc, ix + 6 + ts.cx, iy + 4, 0);
                        LineTo(dc, ix + 6 + ts.cx, iy + ih - 4);
                        SelectObject(dc, ocp);
                        DeleteObject(cpen);
                    }
                }
                else if (!phToDraw.empty()) {
                    SetTextColor(dc, RGB(120, 120, 140));
                    RECT rc = { ix + 6, iy + 2, ix + iw - 4, iy + ih - 2 };
                    DrawTextA(dc, phToDraw.c_str(), -1, &rc, DT_VCENTER | DT_SINGLELINE);
                }
                SelectObject(dc, of);
            } });
            if (pH) {
                Hit h;
                h.r = { ix, iy, ix + iw, iy + ih };
                h.elemId = id;
                h.isInput = true;
                pH->push_back(h);
            }
            curY = y + ih + 8;
            break;
        }
        case HTP::EType::CANVAS: {
            std::string id = e->props.get("id", "cv_" + std::to_string(y));
            int cw = e->props.getInt("width", 400);
            int ch = e->props.getInt("height", 200);
            auto bdr = e->props.getColor("border-color", { 80,80,100 });
            int z = e->props.getInt("z-index", 0);
            auto cb = Canvas::get(id, refDC, cw, ch);
            int cx = x, cy = y - scrollY;
            renderQueue.push_back({ z, [=]() {
                HPEN p = CreatePen(PS_SOLID, 1, bdr.cr());
                auto op = SelectObject(dc, p);
                auto nb = (HBRUSH)GetStockObject(NULL_BRUSH);
                auto ob = SelectObject(dc, nb);
                Rectangle(dc, cx - 1, cy - 1, cx + cw + 1, cy + ch + 1);
                SelectObject(dc, ob);
                SelectObject(dc, op);
                DeleteObject(p);
                if (cb->dc) BitBlt(dc, cx, cy, cw, ch, cb->dc, 0, 0, SRCCOPY);
            } });
            curY = y + ch + 8;
            break;
        }
        case HTP::EType::GL_CANVAS: {
            std::string id = e->props.get("id", "glcv_" + std::to_string(y));
            int cw = e->props.getInt("width", 400);
            int ch = e->props.getInt("height", 200);
            auto bdr = e->props.getColor("border-color", { 80,80,100 });
            int z = e->props.getInt("z-index", 0);
            int cx = x, cy = y - scrollY;
            renderQueue.push_back({ z, [=]() {
                HPEN p = CreatePen(PS_SOLID, 1, bdr.cr());
                auto op = SelectObject(dc, p);
                auto nb = (HBRUSH)GetStockObject(NULL_BRUSH);
                auto ob = SelectObject(dc, nb);
                Rectangle(dc, cx - 1, cy - 1, cx + cw + 1, cy + ch + 1);
                SelectObject(dc, ob);
                SelectObject(dc, op);
                DeleteObject(p);
                if (!GLLoader::available()) {
                    HBRUSH fb = CreateSolidBrush(RGB(40, 40, 60));
                    RECT fr = { cx, cy, cx + cw, cy + ch };
                    FillRect(dc, &fr, fb);
                    DeleteObject(fb);
                    HFONT f = FontCache::get(14);
                    auto of = SelectObject(dc, f);
                    SetTextColor(dc, RGB(150, 150, 170));
                    SetBkMode(dc, TRANSPARENT);
                    RECT rc = { cx, cy, cx + cw, cy + ch };
                    DrawTextA(dc, "No OpenGL", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(dc, of);
                }
            } });
            auto glv = GLCanvas::ensure(id, cw, ch);
            if (glv && glv->valid) {
                GLCanvas::place(id, cx, TOOLBAR_H + cy, cw, ch, scrollY, TOOLBAR_H);
            }
            if (pH) {
                Hit h;
                h.r = { cx,cy,cx + cw,cy + ch };
                h.isGLCanvas = true;
                h.canvasId = id;
                h.canvasW = cw;
                h.canvasH = ch;
                pH->push_back(h);
            }
            curY = y + ch + 8;
            break;
        }
        case HTP::EType::DIVIDER: {
            auto col = e->props.getColor("color", { 60,60,80 });
            int t = e->props.getInt("thickness", 1);
            int m = e->props.getInt("margin", 10);
            int z = e->props.getInt("z-index", 0);
            renderQueue.push_back({ z, [=]() {
                HPEN p = CreatePen(PS_SOLID, t, col.cr());
                auto op = SelectObject(dc, p);
                MoveToEx(dc, x, y + m - scrollY, 0);
                LineTo(dc, x + mw, y + m - scrollY);
                SelectObject(dc, op);
                DeleteObject(p);
            } });
            curY = y + m * 2 + t;
            break;
        }
        case HTP::EType::LIST: {
            int cy = y;
            for (auto& c : e->children) {
                draw(dc, c, x + 20, cy, mw - 20);
                cy = curY;
            }
            break;
        }
        case HTP::EType::ITEM: {
            std::string ct = e->props.get("content", "");
            int sz = e->props.getInt("size", 14);
            auto col = e->props.getColor("color", { 200,200,200 });
            int z = e->props.getInt("z-index", 0);
            int by = y - scrollY + sz / 2 - 2;
            HFONT f = FontCache::get(sz);
            auto of = SelectObject(dc, f);
            RECT rcCalc = { x, y - scrollY, x + mw, y - scrollY + 200 };
            DrawTextA(dc, ct.c_str(), -1, &rcCalc, DT_WORDBREAK | DT_CALCRECT);
            int th = rcCalc.bottom - rcCalc.top;
            SelectObject(dc, of);
            renderQueue.push_back({ z, [=]() {
                HBRUSH b = CreateSolidBrush(col.cr());
                HPEN pn = CreatePen(PS_SOLID, 1, col.cr());
                auto ob = SelectObject(dc, b);
                auto op = SelectObject(dc, pn);
                Ellipse(dc, x - 12, by, x - 4, by + 6);
                SelectObject(dc, ob);
                SelectObject(dc, op);
                DeleteObject(b);
                DeleteObject(pn);

                auto of2 = SelectObject(dc, f);
                SetTextColor(dc, col.cr());
                SetBkMode(dc, TRANSPARENT);
                RECT rcDraw = { x, y - scrollY, x + mw, y - scrollY + th };
                DrawTextA(dc, ct.c_str(), -1, &rcDraw, DT_WORDBREAK);
                SelectObject(dc, of2);
            } });
            curY = y + th + 4;
            break;
        }
        case HTP::EType::BR:
            curY = y + e->props.getInt("size", 16);
            break;
        case HTP::EType::IMAGE: {
            int iw = e->props.getInt("width", 100);
            int ih = e->props.getInt("height", 100);
            std::string alt = e->props.get("alt", "[image]");
            auto bdr = e->props.getColor("border-color", { 80,80,100 });
            int z = e->props.getInt("z-index", 0);
            renderQueue.push_back({ z, [=]() {
                HBRUSH b = CreateSolidBrush(RGB(50, 50, 70));
                HPEN p = CreatePen(PS_SOLID, 1, bdr.cr());
                auto ob = SelectObject(dc, b);
                auto op = SelectObject(dc, p);
                Rectangle(dc, x, y - scrollY, x + iw, y - scrollY + ih);
                SelectObject(dc, ob);
                SelectObject(dc, op);
                DeleteObject(b);
                DeleteObject(p);

                HFONT f = FontCache::get(12);
                auto of = SelectObject(dc, f);
                SetTextColor(dc, RGB(150, 150, 170));
                SetBkMode(dc, TRANSPARENT);
                RECT rc = { x + 4, y - scrollY + 4, x + iw - 4, y - scrollY + ih - 4 };
                DrawTextA(dc, alt.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(dc, of);
            } });
            curY = y + ih + 8;
            break;
        }
        default:
            break;
        }
        if (curY > contentH + scrollY) contentH = curY - scrollY;
    }
public:
    int totalH() const { return contentH; }
    void setScroll(int y) { scrollY = (std::max)(0, y); }
    int getScroll() const { return scrollY; }
    std::vector<Hit> hits;
    void render(HDC tdc, int w, int h, HTP::Doc& doc) {
        contentH = 0;
        curY = 10;
        hits.clear();
        pH = &hits;
        refDC = tdc;
        renderQueue.clear();
        HBRUSH bg = CreateSolidBrush(doc.bg.cr());
        RECT r = { 0,0,w,h };
        FillRect(tdc, &r, bg);
        DeleteObject(bg);
        draw(tdc, doc.root, 10, 10, w - 20);
        std::stable_sort(renderQueue.begin(), renderQueue.end(), [](const RenderCmd& a, const RenderCmd& b) {
            return a.zIndex < b.zIndex;
            });
        for (auto& cmd : renderQueue) {
            cmd.drawCall();
        }
    }
};
};
namespace Pages {
    std::string readF(const std::string& p) {
        std::ifstream f(p);
        if (!f) return "";
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

    std::string home() {
        auto c = readF("home.htp");
        if (!c.empty()) return c;
        return R"htp(
@page { title:"Lume - Start"; background:"#09090b"; }
@block { align:"center"; padding:50; background:"#09090b"; margin:0;
  @br{size:20;}
  @text { id:"logo"; content:"LUME"; size:86; color:"#ff0000"; gradient:"#0000ff"; bold:"true"; }
  @br { size:5; }
  @text { content:"The Custom Lightweight Engine"; size:18; color:"#71717a"; italic:"true"; }
  @br { size:40; }
  @row { gap:10;
     @input { id:"url_box"; placeholder:"Type address (e.g. about:info or file://...)"; width:420; height:40; }
     @button { id:"btn_go"; content:"Navigate"; width:110; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; size:15; }
  }
  @br{size:8;}
  @text { id:"search_err"; content:""; size:14; color:"#ef4444"; }
}
@block { align:"left"; padding:30; margin:10; background:"#09090b";
    @row { gap:20;
        @column { background:"#18181b"; padding:25; border-radius:10; width:280;
            @text { content:"3D Graphics"; size:20; color:"#fafafa"; bold:"true"; }
            @divider { color:"#27272a"; thickness:1; margin:15; }
            @text { content:"Hardware accelerated OpenGL rendering built directly into the UI layer."; size:15; color:"#a1a1aa"; }
            @br { size:20; }
            @button { content:"Launch Demo"; width:140; height:35; background:"#3f3f46"; color:"#ffffff"; border-radius:6; url:"about:gldemo"; }
            @br { size:30; } 
        }
        @column { background:"#18181b"; padding:25; border-radius:10; width:280;
            @text { content:"Native Plugins"; size:20; color:"#fafafa"; bold:"true"; }
            @divider { color:"#27272a"; thickness:1; margin:15; }
            @text { content:"Extend functionality using the dynamic C++ DLL system. Build anything."; size:15; color:"#a1a1aa"; }
            @br { size:20; }
            @button { content:"View Plugins"; width:140; height:35; background:"#3f3f46"; color:"#ffffff"; border-radius:6; url:"about:plugindemo"; }
            @br { size:30; }
        }
        @column { background:"#18181b"; padding:25; border-radius:10; width:280;
            @text { content:"About"; size:20; color:"#fafafa"; bold:"true"; }
            @divider { color:"#27272a"; thickness:1; margin:15; }
            @text { content:"Learn about the Lua 5.4 integration, HTP tags, and custom networking."; size:15; color:"#a1a1aa"; }
            @br { size:20; }
            @button { content:"Read More"; width:140; height:35; background:"#3f3f46"; color:"#ffffff"; border-radius:6; url:"about:info"; }
            @br { size:30; }
        }
    }
}
@script {
    local function hsv_to_hex(h, s, v)
        local c = v * s
        local x = c * (1 - math.abs((h / 60) % 2 - 1))
        local m = v - c
        local r, g, b = 0, 0, 0
        if h < 60 then r, g, b = c, x, 0
        elseif h < 120 then r, g, b = x, c, 0
        elseif h < 180 then r, g, b = 0, c, x
        elseif h < 240 then r, g, b = 0, x, c
        elseif h < 300 then r, g, b = x, 0, c
        else r, g, b = c, 0, x end
        r = math.floor((r + m) * 255)
        g = math.floor((g + m) * 255)
        b = math.floor((b + m) * 255)
        return string.format("#%02X%02X%02X", r, g, b)
    end
    local hue = 0
    local spread = 150
    set_timer(16, function()
        hue = hue + 2
        if hue >= 360 then hue = hue - 360 end
        local hex1 = hsv_to_hex(hue % 360, 1.0, 1.0)
        local hex2 = hsv_to_hex((hue + spread) % 360, 1.0, 1.0)
        set_prop('logo', 'color', hex1)
        set_prop('logo', 'gradient', hex2)
    end)
    local function do_search()
        local query = get_input('url_box')
        if query and query ~= "" then
            set_text('search_err', '')
            navigate(query)
        else
            set_text('search_err', 'Please enter a valid URL')
        end
    end
    on_click('btn_go', do_search)
    on_key_down(function(vk)
        if vk == 13 then do_search() end
    end)
}
)htp";
    }

    std::string about() {
        auto c = readF("about_browser.htp");
        if (!c.empty()) return c;
        return R"htp(
@page { title:"Lume - About"; background:"#09090b"; }
@block { align:"center"; padding:40; background:"#09090b"; margin:0;
  @text { content:"About Lume"; size:46; color:"#fafafa"; bold:"true"; }
  @br { size:5; }
  @text { content:"The architecture behind the custom engine."; size:18; color:"#a1a1aa"; italic:"true"; }
}
@block { align:"center"; padding:20; margin:0; background:"#09090b";
    @row { gap:20;
        @column { background:"#18181b"; padding:25; border-radius:10;
            @text { content:"[*] Custom Rendering"; size:20; color:"#fafafa"; bold:"true"; }
            @divider { color:"#27272a"; thickness:1; margin:15; }
            @text { content:"No CEF or Chromium overhead. Everything is drawn natively using WinAPI, GDI+, and custom layout algorithms."; size:15; color:"#a1a1aa"; }
        }
        @column { background:"#18181b"; padding:25; border-radius:10;
            @text { content:"[+] Lua 5.4 Powered"; size:20; color:"#fafafa"; bold:"true"; }
            @divider { color:"#27272a"; thickness:1; margin:15; }
            @text { content:"Fast and lightweight scripting. Direct bindings to DOM elements, inputs, and canvases without JS bloat."; size:15; color:"#a1a1aa"; }
        }
        @column { background:"#18181b"; padding:25; border-radius:10;
            @text { content:"[>] OpenGL Integration"; size:20; color:"#fafafa"; bold:"true"; }
            @divider { color:"#27272a"; thickness:1; margin:15; }
            @text { content:"Hardware-accelerated canvases embedded directly into the UI flow with full mouse capture support."; size:15; color:"#a1a1aa"; }
        }
        @column { background:"#18181b"; padding:25; border-radius:10;
            @text { content:"[~] Native Plugins"; size:20; color:"#fafafa"; bold:"true"; }
            @divider { color:"#27272a"; thickness:1; margin:15; }
            @text { content:"Extend the engine dynamically using C++ DLLs. Add new network protocols, physics, or OS integrations."; size:15; color:"#a1a1aa"; }
        }
    }
    @br { size:40; }
    @button { content:"Back to Home"; width:180; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; url:"about:home"; size:15; }
}
)htp";
    }

    std::string glDemo() {
        return R"htp(
@page { title:"Lume - 3D Engine"; background:"#09090b"; }
@block { align:"center"; padding:30; background:"#09090b"; margin:0;
  @text { content:"Hardware Acceleration"; size:36; color:"#fafafa"; bold:"true"; }
  @br { size:5; }
  @text { content:"Click canvas to capture mouse | ESC to release | F11 for Fullscreen"; size:15; color:"#71717a"; }
}
@block { align:"center"; padding:0; margin:10; background:"#09090b";
  @column { background:"#18181b"; padding:25; border-radius:12; width:650;
    @glcanvas { id:"gl1"; width:600; height:400; border-color:"#27272a"; }
    @br { size:20; }
    @row { gap:20;
      @button { id:"btn_spin"; content:"Toggle Animation"; width:160; height:35; background:"#3f3f46"; color:"#ffffff"; border-radius:6; }
      @text { id:"gl_status"; content:"Status: Animated"; size:15; color:"#10b981"; }
    }
  }
  @br { size:30; }
  @button { content:"Back to Home"; width:180; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; url:"about:home"; size:15; }
}
@script {
  local animating = true 
  local tid = nil
  local function rgl()
    if not gl_available() then return end
    if not gl_begin_render('gl1', 600, 400) then return end
    local time = get_time()
    if not animating then time = 0 end
    local curW, curH = 600, 400
    if is_fullscreen() then
        curW, curH = get_window_size()
    end
    if curH == 0 then curH = 1 end
    gl_viewport(0, 0, curW, curH)
    gl_matrix_mode(GL_PROJECTION)
    gl_load_identity()
    glu_perspective(50, curW / curH, 0.1, 100)
    gl_matrix_mode(GL_MODELVIEW)
    gl_load_identity()
    glu_look_at(0, 2.5, 7, 0, 0, 0, 0, 1, 0)
    gl_clear(0.04, 0.04, 0.06, 1)
    gl_enable(GL_DEPTH_TEST)
    gl_enable(GL_BLEND)
    gl_blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    gl_push_matrix()
    local grid_offset = (time * 3.0) % 1.0
    gl_translatef(0, -1.5, grid_offset)
    gl_line_width(2.0)
    gl_begin(GL_LINES)
    for i = -15, 15 do
        gl_color(0.9, 0.2, 0.6, 0.4)
        gl_vertex3f(i, 0, -15)
        gl_vertex3f(i, 0, 15)
        local alpha = 1.0 - (math.abs(i) / 15.0)
        if alpha < 0 then alpha = 0 end
        gl_color(0.1, 0.8, 0.9, alpha * 0.6)
        gl_vertex3f(-15, 0, i)
        gl_vertex3f(15, 0, i)
    end
    gl_end()
    gl_pop_matrix()
    gl_push_matrix()
    local float_y = math.sin(time * 2.5) * 0.4
    gl_translatef(0, float_y + 0.5, 0)
    gl_rotatef(time * 40, 0, 1, 0)
    gl_rotatef(time * 15, 1, 0, 0)
    gl_begin(GL_TRIANGLES)
    gl_color(0.9, 0.2, 0.6, 0.6) gl_vertex3f(0,1.5,0) gl_vertex3f(-1,0,1) gl_vertex3f(1,0,1)
    gl_color(0.7, 0.1, 0.8, 0.6) gl_vertex3f(0,1.5,0) gl_vertex3f(1,0,1) gl_vertex3f(1,0,-1)
    gl_color(0.5, 0.0, 0.9, 0.6) gl_vertex3f(0,1.5,0) gl_vertex3f(1,0,-1) gl_vertex3f(-1,0,-1)
    gl_color(0.8, 0.0, 0.5, 0.6) gl_vertex3f(0,1.5,0) gl_vertex3f(-1,0,-1) gl_vertex3f(-1,0,1)
    gl_color(0.1, 0.8, 0.9, 0.6) gl_vertex3f(0,-1.5,0) gl_vertex3f(1,0,1) gl_vertex3f(-1,0,1)
    gl_color(0.1, 0.6, 0.9, 0.6) gl_vertex3f(0,-1.5,0) gl_vertex3f(1,0,-1) gl_vertex3f(1,0,1)
    gl_color(0.1, 0.4, 0.9, 0.6) gl_vertex3f(0,-1.5,0) gl_vertex3f(-1,0,-1) gl_vertex3f(1,0,-1)
    gl_color(0.1, 0.5, 0.7, 0.6) gl_vertex3f(0,-1.5,0) gl_vertex3f(-1,0,1) gl_vertex3f(-1,0,-1)
    gl_end()
    gl_disable(GL_DEPTH_TEST)
    gl_line_width(2.0)
    gl_color(1, 1, 1, 0.9)
    gl_begin(GL_LINES)
    local pts = { {0,1.5,0}, {0,-1.5,0}, {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1} }
    local edges = { {1,3},{1,4},{1,5},{1,6}, {2,3},{2,4},{2,5},{2,6}, {3,4},{4,6},{6,5},{5,3} }
    for i=1, #edges do
        gl_vertex3f(pts[edges[i][1]][1], pts[edges[i][1]][2], pts[edges[i][1]][3])
        gl_vertex3f(pts[edges[i][2]][1], pts[edges[i][2]][2], pts[edges[i][2]][3])
    end
    gl_end()
    gl_pop_matrix()
    gl_end_render('gl1') 
    gl_refresh('gl1')
  end
  rgl()
  tid = set_timer(16, rgl)
  on_click('btn_spin', function()
    animating = not animating
    if animating then 
      set_text('gl_status', 'Status: Animated')
      if not tid then tid = set_timer(16, rgl) end
    else 
      set_text('gl_status', 'Status: Paused') 
      if tid then kill_timer(tid) tid = nil end 
      rgl() 
    end
  end)
  on_key_down(function(vk)
    if vk == VK_F11 then
        fullscreen_canvas('gl1', not is_fullscreen())
    end
  end)
}
)htp";
    }
    std::string pluginDemo() {
        return R"htp(
@page { title:"Lume - Plugins"; background:"#09090b"; }
@block { align:"center"; padding:40; background:"#09090b"; margin:0;
  @text { content:"Native Plugin Bridge"; size:36; color:"#fafafa"; bold:"true"; }
  @br { size:5; }
  @text { content:"Triggering OS-level calls via dynamically loaded C++ DLLs."; size:15; color:"#71717a"; }
}
@block { align:"center"; padding:0; margin:10; background:"#09090b";
  @column { background:"#18181b"; padding:30; border-radius:12; width:400;
    @text { content:"Test Windows Message Box"; size:18; color:"#fafafa"; bold:"true"; }
    @divider { color:"#27272a"; thickness:1; margin:15; }
    @text { content:"This button uses Lua to call a C++ plugin function that interacts with the Windows API natively."; size:14; color:"#a1a1aa"; }
    @br { size:25; }
    @button { id:"btn_yesno"; content:"Execute Plugin"; width:200; height:40; background:"#3b82f6"; color:"#ffffff"; border-radius:6; size:15; }
    @br { size:20; }
    @text { id:"result"; content:"Waiting for input..."; size:14; color:"#f59e0b"; }
  }
  @br { size:30; }
  @button { content:"Back to Home"; width:180; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; url:"about:home"; size:15; }
}

@script {
  on_click('btn_yesno', function()
    if msgBox then
      msgBox('Lume Engine', 'Do you want to proceed with this native action?', {
        yes = function() set_text('result', 'Result: User clicked YES') end,
        no = function() set_text('result', 'Result: User clicked NO') end
      })
    else 
      set_text('result', 'Error: Plugin DLL not found in /plugins folder.') 
    end
  end)
}
)htp";
    }
    std::string error(const std::string& e, const std::string& u) {
        return "@page{title:\"Error\";background:#1a0000;}\n"
            "@block{align:center;padding:40;margin:30;background:#2a1010;border-radius:10;\n"
            "  @text{content:\"Error\";size:32;color:#ff4444;bold:true;}\n"
            "  @br{size:15;}\n"
            "  @text{content:\"" + e + "\";size:16;color:#ff8888;}\n"
            "  @text{content:\"" + u + "\";size:14;color:#aa6666;}\n"
            "  @br{size:20;}\n"
            "  @link{content:\"Home\";url:\"about:home\";color:#0abde3;size:16;}\n}\n";
    }
}
HTP::Doc g_doc;
Render::Engine g_ren;
std::vector<std::string> g_hist;
int g_histPos = -1;
std::string buildTextHtp(const std::string& rawText) {
    std::string htp = "@page { title:\"Text Document\"; background:\"#1e1e1e\"; }\n";
    htp += "@column { padding:10; background:\"#1e1e1e\"; }\n";

    std::istringstream iss(rawText);
    std::string line;
    while (std::getline(iss, line)) {
        std::string escaped;
        for (char c : line) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\r') continue;
            else escaped += c;
        }

        if (escaped.empty()) {
            htp += "  @br { size:14; }\n";
        }
        else {
            htp += "  @text { content:\"" + escaped + "\"; size:14; color:\"#cccccc\"; font:\"Consolas\"; }\n";
        }
    }
    return htp;
}
void loadContent(const std::string& content, const std::string& url, bool isInternalHtp = false) {
    std::string finalContent = content;
    if (!isInternalHtp) {
        std::string path = url;
        auto q = path.find('?');
        if (q != std::string::npos) path = path.substr(0, q);
        auto slash = path.find_last_of('/');
        if (slash != std::string::npos) path = path.substr(slash + 1);
        bool isText = false;
        if (!path.empty()) {
            auto dot = path.find_last_of('.');
            if (dot == std::string::npos) {
                isText = true;
            }
            else {
                std::string ext = path.substr(dot);
                for (char& c : ext) c = tolower(c);
                if (ext == ".txt" || ext == ".md" || ext == ".json" || ext == ".log" || ext == ".cpp" || ext == ".h") {
                    isText = true;
                }
            }
        }
        if (isText) finalContent = buildTextHtp(content);
    }
    Script::reset();
    HTP::Parser p;
    g_doc = p.parse(finalContent);
    g_curUrl = url;
    g_ren.setScroll(0);
    std::function<void(std::shared_ptr<HTP::Elem>)> run;
    run = [&](std::shared_ptr<HTP::Elem> e) {
        if (!e) return;
        if (e->type == HTP::EType::SCRIPT && !e->scriptCode.empty()) Script::exec(e->scriptCode);
        for (auto& c : e->children) run(c);
        };
    run(g_doc.root);
    SetWindowTextA(g_mainWnd, (g_doc.title + " - Lume").c_str());
    SetWindowTextA(g_addressBar, url.c_str());
    invalidateContent();
}
void loadFile(const std::string& fp) {
    std::string path = fp;
    if (path.length() > 7 && path.substr(0, 7) == "file://") path = path.substr(7);
    std::ifstream f(path);
    if (!f) {
        loadContent(Pages::error("Not found", path), "file://" + path, true);
        return;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    f.close();
    loadContent(ss.str(), "file://" + path);
    setStatus("Loaded: " + path);
}
void navigateTo(const std::string& url) {
    std::string u = resolveUrl(url, g_curUrl);
    if (g_histPos >= 0 && g_histPos < (int)g_hist.size() - 1)
        g_hist.resize(g_histPos + 1);
    auto nav = [&](const std::string& c, const std::string& nu) {
        g_hist.push_back(nu);
        g_histPos = (int)g_hist.size() - 1;
        loadContent(c, nu, true);
        setStatus("Ready");
        };
    if (u == "about:home" || u == "about:blank" || u.empty()) { nav(Pages::home(), "about:home"); return;}
    if (u == "about:info") { nav(Pages::about(), u); return;}
    if (u == "about:gldemo") { nav(Pages::glDemo(), u); return;}
    if (u == "about:plugindemo") { nav(Pages::pluginDemo(), u); return;}
    if (u.length() > 7 && u.substr(0, 7) == "file://") {
        g_hist.push_back(u);
        g_histPos = (int)g_hist.size() - 1;
        loadFile(u);
        return;
    }
    if (u.find("://") == std::string::npos) {
        if (u.length() > 4 && u.substr(u.length() - 4) == ".htp") {
            std::ifstream t(u);
            if (t) {
                t.close();
                g_hist.push_back("file://" + u);
                g_histPos = (int)g_hist.size() - 1;
                loadFile(u);
                return;
            }
        }
        u = "http://" + u;
    }
    if ((u.length() > 7 && u.substr(0, 7) == "http://") ||
        (u.length() > 8 && u.substr(0, 8) == "https://")) {
        setStatus("Loading...");
        auto r = Net::fetch(Net::URL::parse(u));
        g_hist.push_back(u);
        g_histPos = (int)g_hist.size() - 1;
        if (r.ok && !r.body.empty()) {
            loadContent(r.body, u);
            setStatus("OK");
        }
        else {
            loadContent(Pages::error(r.error, u), u, true);
            setStatus("Err");
        }
        return;
    }
    loadContent(Pages::error("Unknown protocol", u), u, true);
}
void histNav(const std::string& u) {
    if (u == "about:home") loadContent(Pages::home(), u, true);
    else if (u == "about:info") loadContent(Pages::about(), u, true);
    else if (u == "about:gldemo") loadContent(Pages::glDemo(), u, true);
    else if (u == "about:plugindemo") loadContent(Pages::pluginDemo(), u, true);
    else if (u.length() > 7 && u.substr(0, 7) == "file://") loadFile(u);
    else {
        auto r = Net::fetch(Net::URL::parse(u));
        if (r.ok) loadContent(r.body, u);
        else loadContent(Pages::error(r.error, u), u, true);
    }
}
void goBack() { if (g_histPos > 0) { g_histPos--; histNav(g_hist[g_histPos]); } }
void goFwd() { if (g_histPos < (int)g_hist.size() - 1) { g_histPos++; histNav(g_hist[g_histPos]); } }
WNDPROC g_origAddr = 0;
LRESULT CALLBACK AddrProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        char b[2048] = {};
        GetWindowTextA(h, b, 2048);
        navigateTo(b);
        return 0;
    }
    if (m == WM_CHAR && w == VK_RETURN) return 0;
    return CallWindowProcA(g_origAddr, h, m, w, l);
}
LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        Script::g_hwnd = hw;

        CreateWindowA("BUTTON", "<", WS_CHILD | WS_VISIBLE, 4, 6, 30, 28, hw, (HMENU)ID_BACK, 0, 0);
        CreateWindowA("BUTTON", ">", WS_CHILD | WS_VISIBLE, 38, 6, 30, 28, hw, (HMENU)ID_FWD, 0, 0);
        CreateWindowA("BUTTON", "R", WS_CHILD | WS_VISIBLE, 72, 6, 30, 28, hw, (HMENU)ID_REF, 0, 0);
        g_addressBar = CreateWindowA("EDIT", "about:home", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 108, 8, 500, 24, hw, (HMENU)ID_ADDR, 0, 0);
        CreateWindowA("BUTTON", "Go", WS_CHILD | WS_VISIBLE, 614, 6, 40, 28, hw, (HMENU)ID_GO, 0, 0);
        g_statusBar = CreateWindowA("STATIC", "Ready", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 800, STATUS_H, hw, 0, 0, 0);

        HFONT uf = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        SendMessage(g_addressBar, WM_SETFONT, (WPARAM)uf, 1);
        SendMessage(g_statusBar, WM_SETFONT, (WPARAM)uf, 1);

        g_origAddr = (WNDPROC)SetWindowLongPtrA(g_addressBar, GWLP_WNDPROC, (LONG_PTR)AddrProc);
        navigateTo("about:home");
        return 0;
    }
    case WM_SIZE: {
        if (g_fullscreenCanvas) {
            GLCanvas::moveToFullscreen(g_fullscreenCanvasId);
            g_contentDirty = true;
            InvalidateRect(hw, nullptr, FALSE);
            return 0;
        }
        int w = LOWORD(lp), h = HIWORD(lp);
        int aw = w - 108 - 50 - 10;
        if (aw < 100) aw = 100;
        MoveWindow(g_addressBar, 108, 8, aw, 24, TRUE);
        HWND gb = GetDlgItem(hw, ID_GO);
        if (gb) MoveWindow(gb, 108 + aw + 4, 6, 40, 28, TRUE);
        MoveWindow(g_statusBar, 0, h - STATUS_H, w, STATUS_H, TRUE);
        g_contentDirty = true;
        InvalidateRect(hw, nullptr, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hw, &ps);
        RECT cr;
        GetClientRect(hw, &cr);
        if (g_fullscreenCanvas) {
            HBRUSH bb = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &cr, bb);
            DeleteObject(bb);
        }
        else {
            int w = cr.right;
            int h = cr.bottom - TOOLBAR_H - STATUS_H;
            if (h < 1) h = 1;
            if (ps.rcPaint.top < TOOLBAR_H) {
                HBRUSH tb = CreateSolidBrush(RGB(30, 30, 50));
                RECT tr = { ps.rcPaint.left, 0, ps.rcPaint.right, TOOLBAR_H };
                FillRect(hdc, &tr, tb);
                DeleteObject(tb);
            }
            if (ps.rcPaint.bottom > TOOLBAR_H) {
                ensureBackbuffer(hdc, w, h);
                if (g_contentDirty) {
                    GLCanvas::beginLayoutPass();
                    g_ren.render(g_backDC, w, h, g_doc);
                    GLCanvas::endLayoutPass();
                    g_contentDirty = false;
                }
                int srcTop = (std::max)(0, (int)ps.rcPaint.top - TOOLBAR_H);
                int srcBot = (std::min)(h, (int)ps.rcPaint.bottom - TOOLBAR_H);
                int srcLeft = (std::max)(0, (int)ps.rcPaint.left);
                int srcRight = (std::min)(w, (int)ps.rcPaint.right);
                if (srcBot > srcTop && srcRight > srcLeft) {
                    BitBlt(hdc,
                        srcLeft, TOOLBAR_H + srcTop,
                        srcRight - srcLeft, srcBot - srcTop,
                        g_backDC,
                        srcLeft, srcTop,
                        SRCCOPY);
                }
            }
        }
        EndPaint(hw, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        if (g_mouseCaptured) {
            if (g_ignoreWarpMouse) {
                g_ignoreWarpMouse = false;
                return 0;
            }
            int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
            int centerX = g_capturedCanvasX + g_capturedCanvasW / 2;
            int centerY = TOOLBAR_H + g_capturedCanvasY + g_capturedCanvasH / 2;
            if (g_fullscreenCanvas) {
                RECT cr;
                GetClientRect(hw, &cr);
                centerX = cr.right / 2;
                centerY = cr.bottom / 2;
            }
            int dx = mx - centerX;
            int dy = my - centerY;
            if (dx != 0 || dy != 0) {
                g_mouseDeltaX += dx;
                g_mouseDeltaY += dy;
                POINT c = { centerX, centerY };
                ClientToScreen(hw, &c);
                g_ignoreWarpMouse = true;
                SetCursorPos(c.x, c.y);
            }
        }
        return 0;
    case WM_MOUSEWHEEL: {
        if (g_fullscreenCanvas) return 0;
        int d = GET_WHEEL_DELTA_WPARAM(wp);
        int s = g_ren.getScroll() - d / 2;
        if (s < 0) s = 0;
        RECT cr;
        GetClientRect(hw, &cr);
        int mx = g_ren.totalH() - (cr.bottom - TOOLBAR_H - STATUS_H);
        if (mx < 0) mx = 0;
        if (s > mx) s = mx;
        if (s != g_ren.getScroll()) {
            g_ren.setScroll(s);
            invalidateContent();
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (g_fullscreenCanvas || g_mouseCaptured) {
            SetFocus(hw);
            return 0;
        }
        int mx = LOWORD(lp);
        int my = HIWORD(lp) - TOOLBAR_H;
        Render::Hit clickedHit;
        bool found = false;
        for (const auto& h : g_ren.hits) {
            if (mx >= h.r.left && mx <= h.r.right && my >= h.r.top && my <= h.r.bottom) {
                clickedHit = h;
                found = true;
                break;
            }
        }
        bool ci = false;
        if (found) {
            if (clickedHit.isGLCanvas) {
                int localX = mx - clickedHit.r.left;
                int localY = my - clickedHit.r.top;
                Script::fireCanvasClick(clickedHit.canvasId, localX, localY, 1);
                auto v = GLCanvas::find(clickedHit.canvasId);
                if (v && v->valid) {
                    g_mouseCaptured = true;
                    g_ignoreWarpMouse = false;
                    g_capturedCanvasId = clickedHit.canvasId;
                    g_capturedCanvasW = v->w;
                    g_capturedCanvasH = v->h;
                    g_capturedCanvasX = v->x;
                    g_capturedCanvasY = v->y - TOOLBAR_H;
                    SetCapture(hw);
                    ShowCursor(FALSE);
                    RECT wr;
                    GetWindowRect(v->hwnd, &wr);
                    ClipCursor(&wr);
                }
                SetFocus(hw);
                return 0;
            }
            if (clickedHit.isInput) {
                Script::g_focusId = clickedHit.elemId;
                Script::g_inputs[clickedHit.elemId].focused = true;
                Script::g_inputs[clickedHit.elemId].cursor = (int)Script::g_inputs[clickedHit.elemId].text.length();
                ci = true;
                invalidateContent();
            }
            else {
                if (clickedHit.isBtn && !clickedHit.elemId.empty()) {
                    Script::fireClick(clickedHit.elemId);
                    invalidateContent();
                    if (!clickedHit.url.empty()) navigateTo(clickedHit.url);
                    return 0;
                }
                if (!clickedHit.url.empty()) {
                    navigateTo(clickedHit.url);
                    return 0;
                }
            }
        }
        if (!ci) {
            Script::g_focusId.clear();
            for (auto& kv : Script::g_inputs) kv.second.focused = false;
            invalidateContent();
        }
        SetFocus(hw);
        return 0;
    }
    case WM_CHAR: {
        if (Script::g_focusId.empty()) break;
        auto it = Script::g_inputs.find(Script::g_focusId);
        if (it != Script::g_inputs.end()) {
            auto& inp = it->second;
            char c = (char)wp;
            if (c == '\b') {
                if (inp.cursor > 0 && !inp.text.empty()) {
                    inp.text.erase(inp.cursor - 1, 1);
                    inp.cursor--;
                }
            }
            else if (c == '\r' || c == '\n') {
                inp.focused = false;
                Script::g_focusId.clear();
            }
            else if (c >= 32) {
                inp.text.insert(inp.cursor, 1, c);
                inp.cursor++;
            }
            invalidateContent();
            return 0;
        }
        break;
    }
    case WM_KEYDOWN: {
        if (wp == VK_ESCAPE) {
            if (g_mouseCaptured) { releaseMouse(); return 0; }
            if (g_fullscreenCanvas) { exitFullscreenCanvas(); return 0; }
        }
        if (wp == VK_F11) {
            if (g_fullscreenCanvas) exitFullscreenCanvas();
            else {
                for (auto& h : g_ren.hits) {
                    if (h.isGLCanvas) { enterFullscreenCanvas(h.canvasId); break; }
                }
            }
            return 0;
        }
        if (Script::g_keyDownRef != LUA_NOREF && Script::g_L && GetFocus() == hw && Script::g_focusId.empty()) {
            lua_rawgeti(Script::g_L, LUA_REGISTRYINDEX, Script::g_keyDownRef);
            lua_pushinteger(Script::g_L, (int)wp);
            if (lua_pcall(Script::g_L, 1, 0, 0) != 0) {
                OutputDebugStringA(lua_tostring(Script::g_L, -1));
                lua_pop(Script::g_L, 1);
            }
        }
        if (!Script::g_focusId.empty() && GetFocus() == hw) {
            auto it = Script::g_inputs.find(Script::g_focusId);
            if (it != Script::g_inputs.end()) {
                auto& inp = it->second;
                if (wp == VK_LEFT) { if (inp.cursor > 0) inp.cursor--; invalidateContent(); return 0; }
                if (wp == VK_RIGHT) { if (inp.cursor < (int)inp.text.length()) inp.cursor++; invalidateContent(); return 0; }
                if (wp == VK_HOME) { inp.cursor = 0; invalidateContent(); return 0; }
                if (wp == VK_END) { inp.cursor = (int)inp.text.length(); invalidateContent(); return 0; }
                if (wp == VK_DELETE) {
                    if (inp.cursor < (int)inp.text.length()) {
                        inp.text.erase(inp.cursor, 1);
                        invalidateContent();
                    }
                    return 0;
                }
                if (wp == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    if (OpenClipboard(hw)) {
                        HANDLE h = GetClipboardData(CF_TEXT);
                        if (h) {
                            const char* cl = (const char*)GlobalLock(h);
                            if (cl) {
                                std::string p = cl;
                                p.erase(std::remove(p.begin(), p.end(), '\r'), p.end());
                                p.erase(std::remove(p.begin(), p.end(), '\n'), p.end());
                                inp.text.insert(inp.cursor, p);
                                inp.cursor += (int)p.length();
                                GlobalUnlock(h);
                            }
                        }
                        CloseClipboard();
                    }
                    invalidateContent();
                    return 0;
                }
            }
        }
        if (wp == VK_F5) navigateTo(g_curUrl);
        else if (wp == VK_BACK && Script::g_focusId.empty() && GetFocus() != g_addressBar) goBack();

        return 0;
    }
    case WM_SETCURSOR:
        if (g_mouseCaptured) {
            SetCursor(nullptr);
            return TRUE;
        }
        if (LOWORD(lp) == HTCLIENT) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hw, &pt);
            pt.y -= TOOLBAR_H;
            bool ov = false;
            for (auto& h : g_ren.hits) {
                if (pt.x >= h.r.left && pt.x <= h.r.right && pt.y >= h.r.top && pt.y <= h.r.bottom) {
                    if (!h.url.empty() || h.isBtn || h.isGLCanvas) { ov = true; break; }
                    if (h.isInput) {
                        SetCursor(LoadCursor(0, IDC_IBEAM));
                        return TRUE;
                    }
                }
            }
            SetCursor(LoadCursor(0, ov ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_GO) {
            char b[2048] = {};
            GetWindowTextA(g_addressBar, b, 2048);
            navigateTo(b);
        }
        else if (id == ID_BACK) goBack();
        else if (id == ID_FWD) goFwd();
        else if (id == ID_REF) navigateTo(g_curUrl);
        return 0;
    }
    case WM_DROPFILES: {
        HDROP hd = (HDROP)wp;
        char fp[MAX_PATH] = {};
        DragQueryFileA(hd, 0, fp, MAX_PATH);
        DragFinish(hd);
        std::string p = fp;
        auto slash = p.find_last_of("\\/");
        std::string filename = (slash != std::string::npos) ? p.substr(slash + 1) : p;
        auto dot = filename.find_last_of('.');

        if (dot == std::string::npos) {
            loadFile(p);
        }
        else {
            std::string ext = filename.substr(dot);
            for (char& c : ext) c = tolower(c);
            if (ext == ".htp" || ext == ".txt" || ext == ".md" || ext == ".json" || ext == ".cpp" || ext == ".h" || ext == ".log") {
                loadFile(p);
            }
        }
        return 0;
    }
    case WM_TIMER:
        return 0;
    case WM_DESTROY:
        releaseMouse();
        if (g_fullscreenCanvas) exitFullscreenCanvas();
        Script::reset();
        Plugins::unloadAll();
        cleanupBackbuffer();
        FontCache::clear();
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hw, msg, wp, lp);
}
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmd, int show) {
    INITCOMMONCONTROLSEX ic = { sizeof(ic),ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&ic);
    initHighResTimer();
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    GLLoader::load();
    Plugins::initHostAPI();
    Plugins::discoverPlugins();
    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszClassName = "LumeClass";
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    RegisterClassExA(&wc);
    GLCanvas::registerClass(hI);
    g_mainWnd = CreateWindowExA(
        WS_EX_ACCEPTFILES,
        "LumeClass",
        "Lume",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        0, 0, hI, 0);
    if (!g_mainWnd) return 1;
    ShowWindow(g_mainWnd, show);
    UpdateWindow(g_mainWnd);
    if (cmd && strlen(cmd) > 0) {
        std::string a = cmd;
        if (!a.empty() && a.front() == '"') a.erase(a.begin());
        if (!a.empty() && a.back() == '"') a.pop_back();
        if (!a.empty()) navigateTo(a);
    }
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    Plugins::unloadAll();
    cleanupBackbuffer();
    FontCache::clear();
    GLLoader::unload();
    return (int)msg.wParam;
}
