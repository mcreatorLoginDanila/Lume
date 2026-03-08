#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
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
extern "C" {
#include "lib/lua.h"
#include "lib/lauxlib.h"
#include "lib/lualib.h"
}
#include "lume_plugin.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "opengl32.lib")
#include <GL/gl.h>

typedef HGLRC(WINAPI* PFN_wglCreateContext)(HDC);
typedef BOOL(WINAPI* PFN_wglMakeCurrent)(HDC, HGLRC);
typedef BOOL(WINAPI* PFN_wglDeleteContext)(HGLRC);

namespace GLLoader {
static HMODULE hGL = nullptr;
static PFN_wglCreateContext pfn_wglCreateContext = nullptr;
static PFN_wglMakeCurrent pfn_wglMakeCurrent = nullptr;
static PFN_wglDeleteContext pfn_wglDeleteContext = nullptr;
static bool g_loaded = false;
bool load() {
    if (g_loaded) return hGL != nullptr;
    g_loaded = true;
    hGL = LoadLibraryA("opengl32.dll");
    if (!hGL) return false;
    pfn_wglCreateContext = (PFN_wglCreateContext)GetProcAddress(hGL, "wglCreateContext");
    pfn_wglMakeCurrent = (PFN_wglMakeCurrent)GetProcAddress(hGL, "wglMakeCurrent");
    pfn_wglDeleteContext = (PFN_wglDeleteContext)GetProcAddress(hGL, "wglDeleteContext");
    if (!pfn_wglCreateContext || !pfn_wglMakeCurrent || !pfn_wglDeleteContext) {
        FreeLibrary(hGL); hGL = nullptr; return false;
    }
    return true;
}
void unload() { if (hGL) { FreeLibrary(hGL); hGL = nullptr; } g_loaded = false; }
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

static HDC g_backDC = nullptr;
static HBITMAP g_backBmp = nullptr;
static HBITMAP g_backOld = nullptr;
static int g_backW = 0, g_backH = 0;
static bool g_contentDirty = true;

void ensureBackbuffer(HDC hdc, int w, int h) {
    if (g_backDC && g_backW == w && g_backH == h) return;
    if (g_backDC) { SelectObject(g_backDC, g_backOld); DeleteDC(g_backDC); }
    if (g_backBmp) DeleteObject(g_backBmp);
    g_backDC = CreateCompatibleDC(hdc);
    g_backBmp = CreateCompatibleBitmap(hdc, w, h);
    g_backOld = (HBITMAP)SelectObject(g_backDC, g_backBmp);
    g_backW = w; g_backH = h;
    g_contentDirty = true;
}

void cleanupBackbuffer() {
    if (g_backDC) { SelectObject(g_backDC, g_backOld); DeleteDC(g_backDC); g_backDC = nullptr; }
    if (g_backBmp) { DeleteObject(g_backBmp); g_backBmp = nullptr; }
    g_backOld = nullptr;
}

void invalidateContent() {
    if (!g_mainWnd) return;
    g_contentDirty = true;
    RECT cr; GetClientRect(g_mainWnd, &cr);
    RECT contentRect = { 0, TOOLBAR_H, cr.right, cr.bottom - STATUS_H };
    InvalidateRect(g_mainWnd, &contentRect, FALSE);
}

void invalidateRect(int x, int y, int w, int h) {
    if (!g_mainWnd) return;
    RECT r = { x, TOOLBAR_H + y, x + w, TOOLBAR_H + h };
    InvalidateRect(g_mainWnd, &r, FALSE);
}

void setStatus(const std::string& t) {
    if (g_statusBar) SetWindowTextA(g_statusBar, t.c_str());
}

// ============ Font Cache ============
namespace FontCache {
struct Key {
    int size; bool bold, italic; std::string face;
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
    HFONT font = CreateFontA(-sz, 0, 0, 0, b ? FW_BOLD : FW_NORMAL, i, 0, 0,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, f);
    g_cache[k] = font;
    return font;
}
void clear() { for (auto& kv : g_cache) DeleteObject(kv.second); g_cache.clear(); }
}

// ============ Plugins ============
namespace Plugins {
struct LoadedPlugin { std::string name, path; HMODULE hModule = nullptr; lume_plugin_init_fn initFn = nullptr; };
static std::vector<LoadedPlugin> g_plugins;
static LumeHostAPI g_hostAPI = {};
static HWND hostGetMainHwnd() { return g_mainWnd; }
static void hostInvalidateContent() { invalidateContent(); }
static void hostSetStatus(const char* t) { setStatus(t ? t : ""); }
static void hostNavigateTo(const char* u) { if (u) navigateTo(u); }
void initHostAPI() {
    g_hostAPI.get_main_hwnd = hostGetMainHwnd; g_hostAPI.invalidate_content = hostInvalidateContent;
    g_hostAPI.set_status = hostSetStatus; g_hostAPI.navigate_to = hostNavigateTo; g_hostAPI.api_version = 1;
}
void discoverPlugins() {
    char ep[MAX_PATH] = {}; GetModuleFileNameA(nullptr, ep, MAX_PATH);
    std::string dir = ep; auto ls = dir.find_last_of("\\/");
    dir = (ls != std::string::npos) ? dir.substr(0, ls + 1) : ".\\";
    std::string pd = dir + "plugins\\"; CreateDirectoryA(pd.c_str(), nullptr);
    WIN32_FIND_DATAA fd = {}; HANDLE hf = FindFirstFileA((pd + "*.dll").c_str(), &fd);
    if (hf == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        LoadedPlugin p; p.name = fd.cFileName; p.path = pd + fd.cFileName;
        p.hModule = LoadLibraryA(p.path.c_str()); if (!p.hModule) continue;
        p.initFn = (lume_plugin_init_fn)GetProcAddress(p.hModule, "lume_plugin_init");
        if (!p.initFn) { FreeLibrary(p.hModule); continue; }
        g_plugins.push_back(p);
    } while (FindNextFileA(hf, &fd));
    FindClose(hf);
}
void initAllPlugins(lua_State* L) { for (auto& p : g_plugins) if (p.initFn) p.initFn(L, &g_hostAPI); }
void unloadAll() {
    for (auto& p : g_plugins) {
        if (!p.hModule) continue;
        auto sf = (void(*)())GetProcAddress(p.hModule, "lume_plugin_shutdown"); if (sf) sf();
        FreeLibrary(p.hModule); p.hModule = nullptr;
    }
    g_plugins.clear();
}
}

namespace HTP {
struct Color {
    int r = 255, g = 255, b = 255, a = 255;
    static Color fromHex(const std::string& hex) {
        Color c; std::string h = hex;
        if (!h.empty() && h[0] == '#') h = h.substr(1);
        try {
            if (h.length() >= 6) { c.r = std::stoi(h.substr(0, 2), 0, 16); c.g = std::stoi(h.substr(2, 2), 0, 16); c.b = std::stoi(h.substr(4, 2), 0, 16); }
            if (h.length() >= 8) c.a = std::stoi(h.substr(6, 2), 0, 16);
        } catch (...) {}
        return c;
    }
    COLORREF cr() const { return RGB(r, g, b); }
};
enum class EType { PAGE, BLOCK, TEXT, IMAGE, LINK, BUTTON, INPUT_FIELD, DIVIDER, LIST, ITEM, BR, SCRIPT, CANVAS, ROW, COLUMN, GL_CANVAS, UNKNOWN };
struct Props {
    std::map<std::string, std::string> d;
    std::string get(const std::string& k, const std::string& def = "") const { auto i = d.find(k); return i != d.end() ? i->second : def; }
    int getInt(const std::string& k, int def = 0) const { auto i = d.find(k); if (i != d.end()) { try { return std::stoi(i->second); } catch (...) {} } return def; }
    bool getBool(const std::string& k, bool def = false) const { auto i = d.find(k); if (i != d.end()) return i->second == "true" || i->second == "1"; return def; }
    Color getColor(const std::string& k, Color def = { 255,255,255 }) const { auto i = d.find(k); if (i != d.end()) return Color::fromHex(i->second); return def; }
};
struct Elem {
    EType type = EType::UNKNOWN; std::string tag; Props props; std::string scriptCode;
    std::vector<std::shared_ptr<Elem>> children;
};
struct Doc { std::shared_ptr<Elem> root; std::string title; Color bg = { 26,26,46 }; };

class Parser {
    std::string src; int pos = 0, len = 0;
    void skipWS() {
        while (pos < len) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { pos++; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '/') { while (pos < len && src[pos] != '\n') pos++; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '*') { pos += 2; while (pos + 1 < len && !(src[pos] == '*' && src[pos + 1] == '/')) pos++; if (pos + 1 < len) pos += 2; continue; }
            break;
        }
    }
    std::string readIdent() { std::string r; while (pos < len && (std::isalnum((unsigned char)src[pos]) || src[pos] == '_' || src[pos] == '-')) r += src[pos++]; return r; }
    std::string readString() {
        if (pos >= len || src[pos] != '"') return ""; pos++;
        std::string r; r.reserve(64);
        while (pos < len && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < len) { pos++; switch (src[pos]) { case 'n':r += '\n'; break; case 't':r += '\t'; break; case '"':r += '"'; break; case '\\':r += '\\'; break; default:r += src[pos]; } }
            else r += src[pos]; pos++;
        }
        if (pos < len) pos++; return r;
    }
    std::string readValue() {
        skipWS(); if (pos < len && src[pos] == '"') return readString();
        std::string val; while (pos < len && src[pos] != ';' && src[pos] != '}') val += src[pos++];
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\r' || val.back() == '\n')) val.pop_back();
        return val;
    }
    std::string readRawBlock() {
        int depth = 1, start = pos; bool inStr = false, inLC = false, inBC = false;
        while (pos < len && depth > 0) {
            char c = src[pos];
            if (inLC) { if (c == '\n') inLC = false; pos++; continue; }
            if (inBC) { if (c == '*' && pos + 1 < len && src[pos + 1] == '/') { inBC = false; pos += 2; continue; } pos++; continue; }
            if (inStr) { if (c == '\\') { pos += 2; continue; } if (c == '"') inStr = false; pos++; continue; }
            if (c == '"') { inStr = true; pos++; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '/') { inLC = true; pos += 2; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '*') { inBC = true; pos += 2; continue; }
            if (c == '{') depth++; else if (c == '}') { depth--; if (depth == 0) break; }
            pos++;
        }
        std::string code = src.substr(start, pos - start); if (pos < len && src[pos] == '}') pos++; return code;
    }
    void parseInlineAttrs(std::shared_ptr<Elem>& e) {
        skipWS(); if (pos >= len || src[pos] != '(') return; pos++;
        while (pos < len && src[pos] != ')') { skipWS(); std::string key = readIdent(); skipWS(); if (pos < len && src[pos] == '=') { pos++; skipWS(); e->props.d[key] = readString(); } skipWS(); if (pos < len && src[pos] == ',') pos++; }
        if (pos < len) pos++;
    }
    EType tagToType(const std::string& t) {
        if (t == "page") return EType::PAGE; if (t == "block") return EType::BLOCK; if (t == "text") return EType::TEXT;
        if (t == "image") return EType::IMAGE; if (t == "link") return EType::LINK; if (t == "button") return EType::BUTTON;
        if (t == "input") return EType::INPUT_FIELD; if (t == "divider") return EType::DIVIDER; if (t == "list") return EType::LIST;
        if (t == "item") return EType::ITEM; if (t == "br") return EType::BR; if (t == "script") return EType::SCRIPT;
        if (t == "canvas") return EType::CANVAS; if (t == "row") return EType::ROW; if (t == "column") return EType::COLUMN;
        if (t == "glcanvas") return EType::GL_CANVAS; return EType::UNKNOWN;
    }
    std::shared_ptr<Elem> parseElement() {
        skipWS(); if (pos >= len || src[pos] != '@') return nullptr; pos++;
        auto e = std::make_shared<Elem>(); e->tag = readIdent(); e->type = tagToType(e->tag);
        parseInlineAttrs(e); skipWS(); if (pos >= len || src[pos] != '{') return e; pos++;
        if (e->type == EType::SCRIPT) { e->scriptCode = readRawBlock(); return e; }
        while (pos < len) {
            skipWS(); if (pos >= len) break; if (src[pos] == '}') { pos++; break; }
            if (src[pos] == '@') { auto ch = parseElement(); if (ch) e->children.push_back(ch); }
            else if (std::isalpha((unsigned char)src[pos]) || src[pos] == '_') {
                std::string key = readIdent(); skipWS();
                if (pos < len && src[pos] == ':') { pos++; e->props.d[key] = readValue(); skipWS(); if (pos < len && src[pos] == ';') pos++; }
            } else pos++;
        }
        return e;
    }
public:
    Doc parse(const std::string& source) {
        Doc doc; src = source; pos = 0; len = (int)src.length();
        auto root = std::make_shared<Elem>(); root->type = EType::PAGE; root->tag = "root";
        while (pos < len) {
            skipWS(); if (pos >= len) break;
            if (src[pos] == '@') {
                auto e = parseElement();
                if (e) { if (e->type == EType::PAGE) { doc.title = e->props.get("title", "Lume"); doc.bg = e->props.getColor("background", { 26,26,46 }); for (auto& c : e->children) root->children.push_back(c); } else root->children.push_back(e); }
            } else pos++;
        }
        doc.root = root; return doc;
    }
};
}

namespace Net {
struct URL { std::string proto, host, path; int port = 8080;
    static URL parse(const std::string& s) {
        URL u; std::string r = s; auto pe = r.find("://");
        if (pe != std::string::npos) { u.proto = r.substr(0, pe); r = r.substr(pe + 3); } else u.proto = "htp";
        auto ps = r.find('/'); if (ps != std::string::npos) { u.path = r.substr(ps); r = r.substr(0, ps); } else u.path = "/";
        auto pp = r.find(':'); if (pp != std::string::npos) { u.host = r.substr(0, pp); try { u.port = std::stoi(r.substr(pp + 1)); } catch (...) {} } else { u.host = r; }
        if (u.path == "/" || u.path.empty()) u.path = "/index.htp"; return u;
    }
};
struct Resp { int code = 0; std::string body, error; bool ok = false; };
Resp fetch(const URL& url) {
    Resp resp; addrinfo hints = {}, *res = nullptr; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(url.host.c_str(), std::to_string(url.port).c_str(), &hints, &res) != 0) { resp.error = "DNS fail"; return resp; }
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) { freeaddrinfo(res); resp.error = "Socket fail"; return resp; }
    DWORD to = 5000; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to)); setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&to, sizeof(to));
    if (connect(s, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) { freeaddrinfo(res); closesocket(s); resp.error = "Connect fail"; return resp; }
    freeaddrinfo(res);
    std::string req = "GET " + url.path + " HTTP/1.1\r\nHost: " + url.host + "\r\nUser-Agent: Lume\r\nConnection: close\r\n\r\n";
    send(s, req.c_str(), (int)req.length(), 0);
    std::string raw; raw.reserve(8192); char buf[4096]; int n;
    while ((n = recv(s, buf, sizeof(buf) - 1, 0)) > 0) raw.append(buf, n);
    closesocket(s);
    if (raw.empty()) { resp.error = "Empty"; return resp; }
    auto he = raw.find("\r\n\r\n");
    if (he == std::string::npos) { resp.body = std::move(raw); resp.ok = true; return resp; }
    resp.body = raw.substr(he + 4);
    auto fl = raw.find("\r\n"); auto sl = raw.substr(0, fl); auto sp = sl.find(' ');
    if (sp != std::string::npos) { try { resp.code = std::stoi(sl.substr(sp + 1, 3)); } catch (...) {} }
    resp.ok = (resp.code >= 200 && resp.code < 400);
    if (!resp.ok) resp.error = "HTTP " + std::to_string(resp.code);
    return resp;
}
Resp fetchUrl(const std::string& u) { return fetch(URL::parse(u)); }
}

namespace Canvas {
struct Buf {
    int w = 0, h = 0; HDC dc = 0; HBITMAP bmp = 0, old = 0;
    void create(HDC ref, int ww, int hh) {
        cleanup(); w = ww; h = hh; dc = CreateCompatibleDC(ref);
        bmp = CreateCompatibleBitmap(ref, w, h); old = (HBITMAP)SelectObject(dc, bmp);
        HBRUSH b = CreateSolidBrush(0); RECT r = { 0,0,w,h }; FillRect(dc, &r, b); DeleteObject(b);
    }
    void cleanup() { if (dc) { SelectObject(dc, old); DeleteDC(dc); dc = 0; } if (bmp) { DeleteObject(bmp); bmp = 0; } }
    ~Buf() { cleanup(); }
};
std::map<std::string, std::shared_ptr<Buf>> g_bufs;
std::shared_ptr<Buf> get(const std::string& id, HDC ref, int w, int h) {
    auto i = g_bufs.find(id); if (i != g_bufs.end() && i->second->w == w && i->second->h == h) return i->second;
    auto b = std::make_shared<Buf>(); b->create(ref, w, h); g_bufs[id] = b; return b;
}
}

namespace GLCanvas {
struct GLBuf {
    std::string id;
    int w = 0, h = 0;
    HDC memDC = nullptr;
    HBITMAP dib = nullptr, oldBmp = nullptr;
    HGLRC hglrc = nullptr;
    unsigned char* pixels = nullptr;
    bool valid = false;
    int screenX = 0, screenY = 0;
    void create(int ww, int hh) {
        cleanup(); w = ww; h = hh;
        HDC screenDC = GetDC(nullptr);
        memDC = CreateCompatibleDC(screenDC);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w; bmi.bmiHeader.biHeight = h;
        bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        dib = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, (void**)&pixels, nullptr, 0);
        if (!dib) { DeleteDC(memDC); memDC = nullptr; ReleaseDC(nullptr, screenDC); return; }
        oldBmp = (HBITMAP)SelectObject(memDC, dib);
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL;
        pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24;
        int pf = ChoosePixelFormat(memDC, &pfd);
        if (!pf || !SetPixelFormat(memDC, pf, &pfd)) {
            SelectObject(memDC, oldBmp); DeleteObject(dib); DeleteDC(memDC);
            memDC = nullptr; dib = nullptr; ReleaseDC(nullptr, screenDC); return;
        }
        hglrc = GLLoader::pfn_wglCreateContext(memDC);
        if (!hglrc) {
            SelectObject(memDC, oldBmp); DeleteObject(dib); DeleteDC(memDC);
            memDC = nullptr; dib = nullptr; ReleaseDC(nullptr, screenDC); return;
        }
        ReleaseDC(nullptr, screenDC);
        valid = true;
    }
    void makeCurrent() { if (valid) GLLoader::pfn_wglMakeCurrent(memDC, hglrc); }
    void finish() { if (!valid) return; glFinish(); GdiFlush(); }
    void blitTo(HDC targetDC, int x, int y) {
        if (!valid || !memDC) return;
        BitBlt(targetDC, x, y, w, h, memDC, 0, 0, SRCCOPY);
        screenX = x; screenY = y;
    }
    void cleanup() {
        if (hglrc && GLLoader::available()) { GLLoader::pfn_wglMakeCurrent(nullptr, nullptr); GLLoader::pfn_wglDeleteContext(hglrc); hglrc = nullptr; }
        if (memDC) { if (oldBmp) SelectObject(memDC, oldBmp); DeleteDC(memDC); memDC = nullptr; }
        if (dib) { DeleteObject(dib); dib = nullptr; }
        pixels = nullptr; valid = false;
    }
    ~GLBuf() { cleanup(); }
};
std::map<std::string, std::shared_ptr<GLBuf>> g_bufs;
std::shared_ptr<GLBuf> get(const std::string& id, int w, int h) {
    auto i = g_bufs.find(id);
    if (i != g_bufs.end() && i->second->w == w && i->second->h == h && i->second->valid) return i->second;
    auto b = std::make_shared<GLBuf>(); b->id = id; b->create(w, h); g_bufs[id] = b; return b;
}
void clearAll() { g_bufs.clear(); }
}

namespace Script {
struct InputState { std::string text, placeholder; int x = 0, y = 0, w = 250, h = 28; bool focused = false; int cursor = 0; };
std::map<std::string, InputState> g_inputs;
std::map<std::string, std::string> g_texts;
std::map<std::string, std::function<void()>> g_clicks;
std::map<int, int> g_timerRefs;
int g_timerN = 9000;
int g_keyDownRef = LUA_NOREF;
lua_State* g_L = nullptr;
HWND g_hwnd = nullptr;
std::string g_focusId;
static int l_set_text(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    const char* t = luaL_checkstring(L, 2);
    std::string newVal = t;
    auto& cur = g_texts[id];
    if (cur != newVal) {
        cur = newVal;
        auto i = g_inputs.find(id);
        if (i != g_inputs.end()) i->second.text = newVal;
        invalidateContent();
    }
    return 0;
}
static int l_get_text(lua_State* L) { auto i = g_texts.find(luaL_checkstring(L, 1)); if (i != g_texts.end()) lua_pushstring(L, i->second.c_str()); else { auto j = g_inputs.find(luaL_checkstring(L, 1)); lua_pushstring(L, j != g_inputs.end() ? j->second.text.c_str() : ""); } return 1; }
static int l_on_click(lua_State* L) {
    const char* id = luaL_checkstring(L, 1); luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2); int ref = luaL_ref(L, LUA_REGISTRYINDEX); std::string sid = id;
    g_clicks[sid] = [sid, ref]() { if (!g_L) return; lua_rawgeti(g_L, LUA_REGISTRYINDEX, ref); if (lua_pcall(g_L, 0, 0, 0) != 0) { OutputDebugStringA(lua_tostring(g_L, -1)); lua_pop(g_L, 1); } };
    return 0;
}
static int l_get_mouse(lua_State* L) { POINT p; GetCursorPos(&p); if (g_hwnd) ScreenToClient(g_hwnd, &p); lua_pushinteger(L, p.x); lua_pushinteger(L, p.y - TOOLBAR_H); return 2; }
static int l_http(lua_State* L) { auto r = Net::fetchUrl(luaL_checkstring(L, 1)); lua_pushstring(L, r.body.c_str()); lua_pushinteger(L, r.code); return 2; }
static int l_navigate(lua_State* L) { navigateTo(luaL_checkstring(L, 1)); return 0; }
static int l_alert(lua_State* L) { MessageBoxA(g_hwnd, luaL_checkstring(L, 1), "Lume", MB_OK); return 0; }
static int l_refresh(lua_State* L) { (void)L; invalidateContent(); return 0; }
static int l_get_input(lua_State* L) { auto i = g_inputs.find(luaL_checkstring(L, 1)); lua_pushstring(L, i != g_inputs.end() ? i->second.text.c_str() : ""); return 1; }
static int l_set_input(lua_State* L) { const char* id = luaL_checkstring(L, 1); const char* t = luaL_checkstring(L, 2); g_inputs[id].text = t; g_inputs[id].cursor = (int)strlen(t); invalidateContent(); return 0; }
static int l_key_down(lua_State* L) {
    int vk = (int)luaL_checkinteger(L, 1);
    HWND fg = GetForegroundWindow();
    if (fg != g_hwnd) {
        HWND parent = GetParent(fg);
        if (parent != g_hwnd) {
            lua_pushboolean(L, 0);
            return 1;
        }
    }
    lua_pushboolean(L, (GetAsyncKeyState(vk) & 0x8000) ? 1 : 0);
    return 1;
}
static int l_cv_clear(lua_State* L) { auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1)); if (i != Canvas::g_bufs.end() && i->second->dc) { auto c = HTP::Color::fromHex(luaL_optstring(L, 2, "#000000")); HBRUSH b = CreateSolidBrush(c.cr()); RECT r = { 0,0,i->second->w,i->second->h }; FillRect(i->second->dc, &r, b); DeleteObject(b); } return 0; }
static int l_cv_rect(lua_State* L) { auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1)); if (i != Canvas::g_bufs.end() && i->second->dc) { int x = (int)luaL_checkinteger(L, 2), y = (int)luaL_checkinteger(L, 3), w = (int)luaL_checkinteger(L, 4), h = (int)luaL_checkinteger(L, 5); auto c = HTP::Color::fromHex(luaL_optstring(L, 6, "#ffffff")); HBRUSH b = CreateSolidBrush(c.cr()); RECT r = { x,y,x + w,y + h }; FillRect(i->second->dc, &r, b); DeleteObject(b); } return 0; }
static int l_cv_circle(lua_State* L) { auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1)); if (i != Canvas::g_bufs.end() && i->second->dc) { int cx = (int)luaL_checkinteger(L, 2), cy = (int)luaL_checkinteger(L, 3), r = (int)luaL_checkinteger(L, 4); auto c = HTP::Color::fromHex(luaL_optstring(L, 5, "#ffffff")); HBRUSH b = CreateSolidBrush(c.cr()); HPEN p = CreatePen(PS_SOLID, 1, c.cr()); auto ob = SelectObject(i->second->dc, b); auto op = SelectObject(i->second->dc, p); Ellipse(i->second->dc, cx - r, cy - r, cx + r, cy + r); SelectObject(i->second->dc, ob); SelectObject(i->second->dc, op); DeleteObject(b); DeleteObject(p); } return 0; }
static int l_cv_line(lua_State* L) { auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1)); if (i != Canvas::g_bufs.end() && i->second->dc) { int x1 = (int)luaL_checkinteger(L, 2), y1 = (int)luaL_checkinteger(L, 3), x2 = (int)luaL_checkinteger(L, 4), y2 = (int)luaL_checkinteger(L, 5); auto c = HTP::Color::fromHex(luaL_optstring(L, 6, "#ffffff")); int t = (int)luaL_optinteger(L, 7, 1); HPEN p = CreatePen(PS_SOLID, t, c.cr()); auto op = SelectObject(i->second->dc, p); MoveToEx(i->second->dc, x1, y1, 0); LineTo(i->second->dc, x2, y2); SelectObject(i->second->dc, op); DeleteObject(p); } return 0; }
static int l_cv_text(lua_State* L) { auto i = Canvas::g_bufs.find(luaL_checkstring(L, 1)); if (i != Canvas::g_bufs.end() && i->second->dc) { int x = (int)luaL_checkinteger(L, 2), y = (int)luaL_checkinteger(L, 3); const char* txt = luaL_checkstring(L, 4); int sz = (int)luaL_optinteger(L, 5, 16); auto c = HTP::Color::fromHex(luaL_optstring(L, 6, "#ffffff")); HFONT f = FontCache::get(sz); auto of = SelectObject(i->second->dc, f); SetTextColor(i->second->dc, c.cr()); SetBkMode(i->second->dc, TRANSPARENT); TextOutA(i->second->dc, x, y, txt, (int)strlen(txt)); SelectObject(i->second->dc, of); } return 0; }
static int l_gl_available(lua_State* L) { lua_pushboolean(L, GLLoader::available()); return 1; }
static int l_gl_begin(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    int w = (int)luaL_optinteger(L, 2, 0), h = (int)luaL_optinteger(L, 3, 0);
    auto i = GLCanvas::g_bufs.find(id);
    if (i != GLCanvas::g_bufs.end() && i->second->valid) { i->second->makeCurrent(); lua_pushboolean(L, 1); }
    else if (w > 0 && h > 0) { auto b = GLCanvas::get(id, w, h); if (b && b->valid) { b->makeCurrent(); lua_pushboolean(L, 1); } else lua_pushboolean(L, 0); }
    else lua_pushboolean(L, 0);
    return 1;
}
static int l_gl_end(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    auto i = GLCanvas::g_bufs.find(id);
    if (i != GLCanvas::g_bufs.end() && i->second->valid) {
        i->second->finish();
        GLLoader::pfn_wglMakeCurrent(nullptr, nullptr);
    }
    return 0;
}
static int l_gl_refresh(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    auto it = GLCanvas::g_bufs.find(id);
    if (it == GLCanvas::g_bufs.end() || !it->second->valid) return 0;
    auto& glb = it->second;
    if (g_backDC) {
        glb->blitTo(g_backDC, glb->screenX, glb->screenY);
    }
    if (g_mainWnd) {
        HDC hdc = GetDC(g_mainWnd);
        if (hdc) {
            BitBlt(hdc, glb->screenX, TOOLBAR_H + glb->screenY,
                glb->w, glb->h,
                glb->memDC, 0, 0, SRCCOPY);
            ReleaseDC(g_mainWnd, hdc);
        }
    }
    return 0;
}
static int l_gl_clear(lua_State* L) { glClearColor((float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0), (float)luaL_optnumber(L, 4, 1)); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); return 0; }
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

static int l_set_timer(lua_State* L) {
    int ms = (int)luaL_checkinteger(L, 1); luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2); int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    int tid = ++g_timerN; g_timerRefs[tid] = ref;
    SetTimer(g_hwnd, tid, ms, [](HWND, UINT, UINT_PTR id, DWORD) {
        auto i = g_timerRefs.find((int)id); if (i != g_timerRefs.end() && g_L) { lua_rawgeti(g_L, LUA_REGISTRYINDEX, i->second); if (lua_pcall(g_L, 0, 0, 0) != 0) lua_pop(g_L, 1); }
    });
    lua_pushinteger(L, tid); return 1;
}
static int l_kill_timer(lua_State* L) { int id = (int)luaL_checkinteger(L, 1); KillTimer(g_hwnd, id); auto it = g_timerRefs.find(id); if (it != g_timerRefs.end()) { if (g_L) luaL_unref(L, LUA_REGISTRYINDEX, it->second); g_timerRefs.erase(it); } return 0; }
static int l_on_key_down(lua_State* L) { luaL_checktype(L, 1, LUA_TFUNCTION); if (g_keyDownRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, g_keyDownRef); lua_pushvalue(L, 1); g_keyDownRef = luaL_ref(L, LUA_REGISTRYINDEX); return 0; }

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
        {nullptr,0}
    };
    for (int i = 0; cs[i].n; i++) { lua_pushinteger(L, cs[i].v); lua_setglobal(L, cs[i].n); }
}

void init() {
    if (g_L) lua_close(g_L);
    g_L = luaL_newstate(); luaL_openlibs(g_L);
    lua_register(g_L, "set_text", l_set_text); lua_register(g_L, "get_text", l_get_text);
    lua_register(g_L, "on_click", l_on_click); lua_register(g_L, "get_mouse_pos", l_get_mouse);
    lua_register(g_L, "http_request", l_http); lua_register(g_L, "navigate", l_navigate);
    lua_register(g_L, "alert", l_alert); lua_register(g_L, "refresh", l_refresh);
    lua_register(g_L, "get_input", l_get_input); lua_register(g_L, "set_input", l_set_input);
    lua_register(g_L, "key_down", l_key_down);
    lua_register(g_L, "canvas_clear", l_cv_clear); lua_register(g_L, "canvas_rect", l_cv_rect);
    lua_register(g_L, "canvas_circle", l_cv_circle); lua_register(g_L, "canvas_line", l_cv_line);
    lua_register(g_L, "canvas_text", l_cv_text);
    lua_register(g_L, "set_timer", l_set_timer); lua_register(g_L, "kill_timer", l_kill_timer);
    lua_register(g_L, "on_key_down", l_on_key_down);
    lua_register(g_L, "gl_available", l_gl_available);
    lua_register(g_L, "gl_begin_render", l_gl_begin);
    lua_register(g_L, "gl_end_render", l_gl_end);
    lua_register(g_L, "gl_refresh", l_gl_refresh);
    lua_register(g_L, "gl_clear", l_gl_clear); lua_register(g_L, "gl_viewport", l_gl_viewport);
    lua_register(g_L, "gl_color", l_gl_color);
    lua_register(g_L, "gl_begin", l_gl_begin_prim); lua_register(g_L, "gl_end", l_gl_end_prim);
    lua_register(g_L, "gl_vertex2f", l_gl_vertex2f); lua_register(g_L, "gl_vertex3f", l_gl_vertex3f);
    lua_register(g_L, "gl_ortho", l_gl_ortho); lua_register(g_L, "gl_frustum", l_gl_frustum);
    lua_register(g_L, "gl_load_identity", l_gl_load_identity);
    lua_register(g_L, "gl_matrix_mode", l_gl_matrix_mode);
    lua_register(g_L, "gl_push_matrix", l_gl_push_matrix); lua_register(g_L, "gl_pop_matrix", l_gl_pop_matrix);
    lua_register(g_L, "gl_translatef", l_gl_translatef); lua_register(g_L, "gl_rotatef", l_gl_rotatef);
    lua_register(g_L, "gl_scalef", l_gl_scalef);
    lua_register(g_L, "gl_enable", l_gl_enable); lua_register(g_L, "gl_disable", l_gl_disable);
    lua_register(g_L, "gl_line_width", l_gl_line_width); lua_register(g_L, "gl_point_size", l_gl_point_size);
    lua_register(g_L, "gl_normal3f", l_gl_normal3f);
    registerGLConstants(g_L);
    Plugins::initAllPlugins(g_L);
}

void exec(const std::string& code) {
    if (!g_L) init();
    if (luaL_dostring(g_L, code.c_str()) != 0) {
        const char* e = lua_tostring(g_L, -1);
        OutputDebugStringA(e ? e : "lua err"); OutputDebugStringA("\n");
        MessageBoxA(g_hwnd, e ? e : "Unknown error", "Lua Error", MB_OK | MB_ICONERROR);
        lua_pop(g_L, 1);
    }
}
void fireClick(const std::string& id) { auto i = g_clicks.find(id); if (i != g_clicks.end()) i->second(); }
void reset() {
    for (auto& kv : g_timerRefs) { KillTimer(g_hwnd, kv.first); if (g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, kv.second); }
    g_timerRefs.clear(); g_timerN = 9000; g_clicks.clear(); g_texts.clear(); g_inputs.clear(); g_focusId.clear();
    if (g_keyDownRef != LUA_NOREF && g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, g_keyDownRef);
    g_keyDownRef = LUA_NOREF; Canvas::g_bufs.clear(); GLCanvas::clearAll();
    if (g_L) { lua_close(g_L); g_L = nullptr; }
}
}

namespace Render {
struct Hit { RECT r; std::string url, action, elemId; bool isInput = false, isBtn = false; };
class Engine {
    int scrollY = 0, contentH = 0, curY = 0;
    std::vector<Hit>* pH = nullptr;
    HDC refDC = nullptr;
    void fillRR(HDC dc, int x, int y, int w, int h, HTP::Color c, int rad = 5) {
        HBRUSH b = CreateSolidBrush(c.cr()); HPEN p = CreatePen(PS_SOLID, 1, c.cr());
        auto ob = SelectObject(dc, b); auto op = SelectObject(dc, p);
        RoundRect(dc, x, y, x + w, y + h, rad, rad);
        SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
    }
    int estH(std::shared_ptr<HTP::Elem> e) {
        if (!e) return 0;
        switch (e->type) {
        case HTP::EType::TEXT: case HTP::EType::ITEM: return e->props.getInt("size", 16) + 8;
        case HTP::EType::LINK: return e->props.getInt("size", 16) + 8;
        case HTP::EType::BUTTON: return e->props.getInt("height", 35) + 8;
        case HTP::EType::INPUT_FIELD: return e->props.getInt("height", 28) + 8;
        case HTP::EType::DIVIDER: return e->props.getInt("margin", 10) * 2 + e->props.getInt("thickness", 1);
        case HTP::EType::IMAGE: return e->props.getInt("height", 100) + 8;
        case HTP::EType::BR: return e->props.getInt("size", 16);
        case HTP::EType::CANVAS: case HTP::EType::GL_CANVAS: return e->props.getInt("height", 200) + 8;
        case HTP::EType::SCRIPT: return 0;
        case HTP::EType::BLOCK: case HTP::EType::COLUMN: { int h = e->props.getInt("padding", 10) * 2 + e->props.getInt("margin", 5) * 2; for (auto& c : e->children) h += estH(c); return h; }
        case HTP::EType::ROW: { int m = 0; for (auto& c : e->children) m = (std::max)(m, estH(c)); return m + e->props.getInt("margin", 5) * 2; }
        case HTP::EType::LIST: { int h = 0; for (auto& c : e->children) h += estH(c); return h; }
        default: return 20;
        }
    }
    int estW(std::shared_ptr<HTP::Elem> e, int mw) {
        if (!e) return mw;
        switch (e->type) {
        case HTP::EType::BUTTON: return e->props.getInt("width", 120);
        case HTP::EType::INPUT_FIELD: return e->props.getInt("width", 250);
        case HTP::EType::IMAGE: return e->props.getInt("width", 100);
        case HTP::EType::CANVAS: case HTP::EType::GL_CANVAS: return e->props.getInt("width", 400);
        default: return mw;
        }
    }
    void draw(HDC dc, std::shared_ptr<HTP::Elem> e, int x, int y, int mw) {
        if (!e) return;
        switch (e->type) {
        case HTP::EType::PAGE: case HTP::EType::UNKNOWN: { int cy = y; for (auto& c : e->children) { draw(dc, c, x, cy, mw); cy = curY; } break; }
        case HTP::EType::SCRIPT: curY = y; break;
        case HTP::EType::ROW: {
            int mar = e->props.getInt("margin", 5), pad = e->props.getInt("padding", 0), gap = e->props.getInt("gap", 10);
            int nc = 0, fw = 0, fc = 0;
            for (auto& c : e->children) { nc++; int w = c->props.getInt("width", 0); if (w > 0) fw += w; else fc++; }
            int avail = mw - mar * 2 - pad * 2 - gap * (std::max)(nc - 1, 0);
            int flexW = fc > 0 ? (avail - fw) / fc : 0; if (flexW < 50) flexW = 50;
            int cx = x + mar + pad, ry = y + mar, maxH = 0;
            for (auto& c : e->children) { int cw = c->props.getInt("width", flexW); int sv = curY; curY = ry; draw(dc, c, cx, ry, cw); int ch = curY - ry; if (ch > maxH) maxH = ch; cx += cw + gap; curY = sv; }
            curY = ry + maxH + mar; break;
        }
        case HTP::EType::COLUMN: {
            int pad = e->props.getInt("padding", 5), rad = e->props.getInt("border-radius", 4);
            std::string bg = e->props.get("background", "");
            int th = 0; for (auto& c : e->children) th += estH(c); th += pad * 2;
            if (!bg.empty()) fillRR(dc, x, y - scrollY, mw, th, HTP::Color::fromHex(bg), rad);
            curY = y + pad; for (auto& c : e->children) draw(dc, c, x + pad, curY, mw - pad * 2); curY += pad; break;
        }
        case HTP::EType::BLOCK: {
            int pad = e->props.getInt("padding", 10), mar = e->props.getInt("margin", 5), rad = e->props.getInt("border-radius", 6);
            std::string bg = e->props.get("background", ""), align = e->props.get("align", "left");
            int iy = y + mar + pad, ty = iy; for (auto& c : e->children) ty += estH(c); int bh = (ty - iy) + pad * 2;
            if (!bg.empty()) fillRR(dc, x + mar, y + mar - scrollY, mw - mar * 2, bh, HTP::Color::fromHex(bg), rad);
            curY = iy;
            for (auto& c : e->children) { int cx = x + mar + pad, cmw = mw - (mar + pad) * 2; if (align == "center") { int ew = estW(c, cmw); cx = x + (mw - ew) / 2; } draw(dc, c, cx, curY, cmw); }
            curY += pad + mar; break;
        }
        case HTP::EType::TEXT: {
            std::string id = e->props.get("id", ""), ct = e->props.get("content", "");
            if (!id.empty()) { auto i = Script::g_texts.find(id); if (i != Script::g_texts.end()) ct = i->second; else Script::g_texts[id] = ct; }
            int sz = e->props.getInt("size", 16); auto col = e->props.getColor("color", { 255,255,255 });
            HFONT f = FontCache::get(sz, e->props.getBool("bold"), e->props.getBool("italic"), e->props.get("font", "Segoe UI").c_str());
            auto of = SelectObject(dc, f); SetTextColor(dc, col.cr()); SetBkMode(dc, TRANSPARENT);
            RECT rc = { x,y - scrollY,x + mw,y - scrollY + 1000 }; DrawTextA(dc, ct.c_str(), -1, &rc, DT_WORDBREAK | DT_CALCRECT);
            int th = rc.bottom - rc.top; rc.bottom = rc.top + th; DrawTextA(dc, ct.c_str(), -1, &rc, DT_WORDBREAK);
            SelectObject(dc, of); curY = y + th + 4; break;
        }
        case HTP::EType::LINK: {
            std::string ct = e->props.get("content", "[link]"), url = e->props.get("url", "");
            int sz = e->props.getInt("size", 16); auto col = e->props.getColor("color", { 10,189,227 });
            HFONT f = FontCache::get(sz); auto of = SelectObject(dc, f);
            SetTextColor(dc, col.cr()); SetBkMode(dc, TRANSPARENT);
            SIZE ts; GetTextExtentPoint32A(dc, ct.c_str(), (int)ct.length(), &ts);
            RECT rc = { x,y - scrollY,x + ts.cx,y - scrollY + ts.cy }; DrawTextA(dc, ct.c_str(), -1, &rc, 0);
            HPEN p = CreatePen(PS_SOLID, 1, col.cr()); auto op = SelectObject(dc, p);
            MoveToEx(dc, x, y - scrollY + ts.cy - 1, 0); LineTo(dc, x + ts.cx, y - scrollY + ts.cy - 1);
            SelectObject(dc, op); DeleteObject(p); SelectObject(dc, of);
            if (pH) { Hit h; h.r = { x,y - scrollY,x + ts.cx,y - scrollY + ts.cy }; h.url = url; pH->push_back(h); }
            curY = y + ts.cy + 4; break;
        }
        case HTP::EType::BUTTON: {
            std::string id = e->props.get("id", ""), ct = e->props.get("content", "Button"), act = e->props.get("action", ""), url = e->props.get("url", act);
            int bw = e->props.getInt("width", 120), bh = e->props.getInt("height", 35), sz = e->props.getInt("size", 14), rad = e->props.getInt("border-radius", 6);
            auto bg = e->props.getColor("background", { 233,69,96 }); auto col = e->props.getColor("color", { 255,255,255 });
            int bx = x, by = y - scrollY; fillRR(dc, bx, by, bw, bh, bg, rad);
            HFONT f = FontCache::get(sz, true); auto of = SelectObject(dc, f);
            SetTextColor(dc, col.cr()); SetBkMode(dc, TRANSPARENT);
            RECT rc = { bx,by,bx + bw,by + bh }; DrawTextA(dc, ct.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, of);
            if (pH) { Hit h; h.r = { bx,by,bx + bw,by + bh }; h.url = url; h.action = act; h.elemId = id; h.isBtn = true; pH->push_back(h); }
            curY = y + bh + 8; break;
        }
        case HTP::EType::INPUT_FIELD: {
            std::string id = e->props.get("id", "inp_" + std::to_string(y)), ph = e->props.get("placeholder", "");
            int iw = e->props.getInt("width", 250), ih = e->props.getInt("height", 28);
            auto& inp = Script::g_inputs[id]; if (inp.placeholder.empty() && !ph.empty()) inp.placeholder = ph;
            inp.x = x; inp.y = y - scrollY; inp.w = iw; inp.h = ih;
            int ix = x, iy = y - scrollY; bool foc = (Script::g_focusId == id);
            HBRUSH br = CreateSolidBrush(foc ? RGB(50, 50, 80) : RGB(40, 40, 60));
            HPEN pn = CreatePen(PS_SOLID, foc ? 2 : 1, foc ? RGB(10, 189, 227) : RGB(100, 100, 140));
            auto ob = SelectObject(dc, br); auto op = SelectObject(dc, pn);
            RoundRect(dc, ix, iy, ix + iw, iy + ih, 4, 4);
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br); DeleteObject(pn);
            HFONT f = FontCache::get(14); auto of = SelectObject(dc, f); SetBkMode(dc, TRANSPARENT);
            if (!inp.text.empty()) {
                SetTextColor(dc, RGB(230, 230, 240)); RECT rc = { ix + 6,iy + 2,ix + iw - 4,iy + ih - 2 };
                DrawTextA(dc, inp.text.c_str(), -1, &rc, DT_VCENTER | DT_SINGLELINE);
                if (foc) { int cp = (std::min)(inp.cursor, (int)inp.text.length()); SIZE ts; GetTextExtentPoint32A(dc, inp.text.c_str(), cp, &ts); HPEN cpen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255)); auto ocp = SelectObject(dc, cpen); MoveToEx(dc, ix + 6 + ts.cx, iy + 4, 0); LineTo(dc, ix + 6 + ts.cx, iy + ih - 4); SelectObject(dc, ocp); DeleteObject(cpen); }
            } else if (!inp.placeholder.empty()) { SetTextColor(dc, RGB(120, 120, 140)); RECT rc = { ix + 6,iy + 2,ix + iw - 4,iy + ih - 2 }; DrawTextA(dc, inp.placeholder.c_str(), -1, &rc, DT_VCENTER | DT_SINGLELINE); }
            SelectObject(dc, of);
            if (pH) { Hit h; h.r = { ix,iy,ix + iw,iy + ih }; h.elemId = id; h.isInput = true; pH->push_back(h); }
            curY = y + ih + 8; break;
        }
        case HTP::EType::CANVAS: {
            std::string id = e->props.get("id", "cv_" + std::to_string(y));
            int cw = e->props.getInt("width", 400), ch = e->props.getInt("height", 200);
            auto bdr = e->props.getColor("border-color", { 80,80,100 });
            auto cb = Canvas::get(id, refDC, cw, ch); int cx = x, cy = y - scrollY;
            HPEN p = CreatePen(PS_SOLID, 1, bdr.cr()); auto op = SelectObject(dc, p);
            auto nb = (HBRUSH)GetStockObject(NULL_BRUSH); auto ob = SelectObject(dc, nb);
            Rectangle(dc, cx - 1, cy - 1, cx + cw + 1, cy + ch + 1);
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(p);
            if (cb->dc) BitBlt(dc, cx, cy, cw, ch, cb->dc, 0, 0, SRCCOPY);
            curY = y + ch + 8; break;
        }
        case HTP::EType::GL_CANVAS: {
            std::string id = e->props.get("id", "glcv_" + std::to_string(y));
            int cw = e->props.getInt("width", 400), ch = e->props.getInt("height", 200);
            auto bdr = e->props.getColor("border-color", { 80,80,100 });
            int cx = x, cy = y - scrollY;
            HPEN p = CreatePen(PS_SOLID, 1, bdr.cr()); auto op = SelectObject(dc, p);
            auto nb = (HBRUSH)GetStockObject(NULL_BRUSH); auto ob = SelectObject(dc, nb);
            Rectangle(dc, cx - 1, cy - 1, cx + cw + 1, cy + ch + 1);
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(p);
            if (GLLoader::available()) {
                auto glb = GLCanvas::get(id, cw, ch);
                if (glb && glb->valid) { glb->screenX = cx; glb->screenY = cy; glb->blitTo(dc, cx, cy); }
            } else {
                HBRUSH fb = CreateSolidBrush(RGB(40, 40, 60)); RECT fr = { cx,cy,cx + cw,cy + ch }; FillRect(dc, &fr, fb); DeleteObject(fb);
                HFONT f = FontCache::get(14); auto of = SelectObject(dc, f);
                SetTextColor(dc, RGB(150, 150, 170)); SetBkMode(dc, TRANSPARENT);
                RECT rc = { cx,cy,cx + cw,cy + ch }; DrawTextA(dc, "No OpenGL", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(dc, of);
            }
            curY = y + ch + 8; break;
        }
        case HTP::EType::DIVIDER: {
            auto col = e->props.getColor("color", { 60,60,80 }); int t = e->props.getInt("thickness", 1), m = e->props.getInt("margin", 10);
            HPEN p = CreatePen(PS_SOLID, t, col.cr()); auto op = SelectObject(dc, p);
            MoveToEx(dc, x, y + m - scrollY, 0); LineTo(dc, x + mw, y + m - scrollY);
            SelectObject(dc, op); DeleteObject(p); curY = y + m * 2 + t; break;
        }
        case HTP::EType::LIST: { int cy = y; for (auto& c : e->children) { draw(dc, c, x + 20, cy, mw - 20); cy = curY; } break; }
        case HTP::EType::ITEM: {
            std::string ct = e->props.get("content", ""); int sz = e->props.getInt("size", 14);
            auto col = e->props.getColor("color", { 200,200,200 }); int by = y - scrollY + sz / 2 - 2;
            HBRUSH b = CreateSolidBrush(col.cr()); HPEN pn = CreatePen(PS_SOLID, 1, col.cr());
            auto ob = SelectObject(dc, b); auto op = SelectObject(dc, pn);
            Ellipse(dc, x - 12, by, x - 4, by + 6);
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(pn);
            HFONT f = FontCache::get(sz); auto of = SelectObject(dc, f);
            SetTextColor(dc, col.cr()); SetBkMode(dc, TRANSPARENT);
            RECT rc = { x,y - scrollY,x + mw,y - scrollY + 200 }; DrawTextA(dc, ct.c_str(), -1, &rc, DT_WORDBREAK | DT_CALCRECT);
            int th = rc.bottom - rc.top; rc.bottom = rc.top + th; DrawTextA(dc, ct.c_str(), -1, &rc, DT_WORDBREAK);
            SelectObject(dc, of); curY = y + th + 4; break;
        }
        case HTP::EType::BR: curY = y + e->props.getInt("size", 16); break;
        case HTP::EType::IMAGE: {
            int iw = e->props.getInt("width", 100), ih = e->props.getInt("height", 100);
            std::string alt = e->props.get("alt", "[image]"); auto bdr = e->props.getColor("border-color", { 80,80,100 });
            HBRUSH b = CreateSolidBrush(RGB(50, 50, 70)); HPEN p = CreatePen(PS_SOLID, 1, bdr.cr());
            auto ob = SelectObject(dc, b); auto op = SelectObject(dc, p);
            Rectangle(dc, x, y - scrollY, x + iw, y - scrollY + ih);
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
            HFONT f = FontCache::get(12); auto of = SelectObject(dc, f);
            SetTextColor(dc, RGB(150, 150, 170)); SetBkMode(dc, TRANSPARENT);
            RECT rc = { x + 4,y - scrollY + 4,x + iw - 4,y - scrollY + ih - 4 }; DrawTextA(dc, alt.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, of); curY = y + ih + 8; break;
        }
        default: break;
        }
        if (curY > contentH + scrollY) contentH = curY - scrollY;
    }
public:
    int totalH() const { return contentH; }
    void setScroll(int y) { scrollY = (std::max)(0, y); }
    int getScroll() const { return scrollY; }
    std::vector<Hit> hits;
    void render(HDC tdc, int w, int h, HTP::Doc& doc) {
        contentH = 0; curY = 10; hits.clear(); pH = &hits; refDC = tdc;
        HBRUSH bg = CreateSolidBrush(doc.bg.cr()); RECT r = { 0,0,w,h }; FillRect(tdc, &r, bg); DeleteObject(bg);
        draw(tdc, doc.root, 10, 10, w - 20);
    }
};
}

namespace Pages {
std::string readF(const std::string& p) { std::ifstream f(p); if (!f) return ""; std::stringstream s; s << f.rdbuf(); return s.str(); }
std::string home() {
    auto c = readF("home.htp"); if (!c.empty()) return c;
    std::string s; s.reserve(1024);
    s += "@page{title:\"Lume\";background:#0f0f23;}\n";
    s += "@block{align:center;padding:40;background:#1a1a3e;border-radius:12;margin:20;\n";
    s += "  @text{content:\"Lume\";size:42;color:#e94560;bold:true;}\n";
    s += "  @br{size:10;}\n";
    s += "  @text{content:\"Lua + Canvas + OpenGL + Plugins\";size:18;color:#8888aa;italic:true;}\n";
    s += "}\n";
    s += "@block{padding:20;margin:20;background:#16213e;border-radius:8;\n";
    s += "  @text{content:\"Quick links:\";size:18;color:#ffffff;bold:true;}\n  @br{size:10;}\n";
    s += "  @link{content:\"Server localhost:8080\";url:\"htp://localhost:8080/index.htp\";color:#0abde3;size:16;}\n";
    s += "  @link{content:\"OpenGL Demo\";url:\"about:gldemo\";color:#0abde3;size:16;}\n";
    s += "  @link{content:\"Plugin Demo\";url:\"about:plugindemo\";color:#0abde3;size:16;}\n";
    s += "  @link{content:\"About\";url:\"about:info\";color:#0abde3;size:16;}\n}\n";
    return s;
}
std::string about() {
    auto c = readF("about_browser.htp"); if (!c.empty()) return c;
    return "@page{title:\"About\";background:#0f0f23;}\n@block{align:center;padding:30;background:#1a1a3e;margin:20;border-radius:10;\n  @text{content:\"Lume\";size:36;color:#e94560;bold:true;}\n}\n@block{padding:20;margin:20;background:#16213e;border-radius:8;\n  @list{\n    @item{content:\"Lua 5.4 + GDI + OpenGL\";size:15;color:#ccccee;}\n    @item{content:\"DLL Plugin system\";size:15;color:#ccccee;}\n  }\n  @br{size:15;}\n  @link{content:\"Home\";url:\"about:home\";color:#0abde3;size:16;}\n}\n";
}
std::string glDemo() {
    std::string s; s.reserve(2048);
    s += "@page{title:\"GL Demo\";background:#0f0f23;}\n@block{align:center;padding:20;background:#1a1a3e;margin:20;border-radius:10;\n";
    s += "  @text{content:\"OpenGL Canvas\";size:32;color:#e94560;bold:true;}\n  @br{size:10;}\n  @glcanvas{id:\"gl1\";width:400;height:300;border-color:#0abde3;}\n";
    s += "  @br{size:10;}\n  @button{id:\"btn_spin\";content:\"Toggle Spin\";width:150;height:35;background:#e94560;}\n";
    s += "  @text{id:\"gl_status\";content:\"Ready\";size:14;color:#aaaacc;}\n}\n";
    s += "@block{padding:10;margin:20;background:#16213e;border-radius:8;\n  @link{content:\"Home\";url:\"about:home\";color:#0abde3;size:16;}\n}\n";
    s += "@script{\nlocal spinning=false local angle=0 local tid=nil\n";
    s += "local function rgl()\n  if not gl_available() then return end\n";
    s += "  if not gl_begin_render('gl1',400,300) then return end\n";
    s += "  gl_viewport(0,0,400,300) gl_matrix_mode(GL_PROJECTION) gl_load_identity()\n";
    s += "  gl_ortho(-2,2,-1.5,1.5,-1,1) gl_matrix_mode(GL_MODELVIEW) gl_load_identity()\n";
    s += "  gl_clear(0.06,0.06,0.14,1) gl_push_matrix() gl_rotatef(angle,0,0,1)\n";
    s += "  gl_begin(GL_TRIANGLES) gl_color(1,0.27,0.38) gl_vertex2f(0,1)\n";
    s += "    gl_color(0.04,0.74,0.89) gl_vertex2f(-0.87,-0.5)\n";
    s += "    gl_color(0.28,0.78,0.45) gl_vertex2f(0.87,-0.5) gl_end() gl_pop_matrix()\n";
    s += "  gl_end_render('gl1') gl_refresh('gl1')\nend\nrgl()\n";
    s += "on_click('btn_spin',function()\n  spinning=not spinning\n";
    s += "  if spinning then set_text('gl_status','Spinning...')\n";
    s += "    tid=set_timer(33,function() angle=angle+2 if angle>=360 then angle=angle-360 end rgl() end)\n";
    s += "  else set_text('gl_status','Stopped') if tid then kill_timer(tid) tid=nil end end\nend)\n}\n";
    return s;
}
std::string pluginDemo() {
    std::string s;
    s += "@page{title:\"Plugin Demo\";background:#0f0f23;}\n@block{align:center;padding:30;background:#1a1a3e;margin:20;border-radius:10;\n  @text{content:\"Plugin Demo\";size:32;color:#e94560;bold:true;}\n}\n";
    s += "@block{padding:20;margin:20;background:#16213e;border-radius:8;\n  @button{id:\"btn_yesno\";content:\"Yes/No?\";width:200;height:40;background:#e94560;}\n  @text{id:\"result\";content:\"Click above\";size:15;color:#aaaacc;}\n}\n";
    s += "@block{padding:10;margin:20;background:#16213e;border-radius:8;\n  @link{content:\"Home\";url:\"about:home\";color:#0abde3;size:16;}\n}\n";
    s += "@script{\non_click('btn_yesno',function()\n  msgBox('Test','Delete?',{yes=function() set_text('result','Yes!') end,no=function() set_text('result','No!') end})\nend)\n}\n";
    return s;
}
std::string error(const std::string& e, const std::string& u) {
    return "@page{title:\"Error\";background:#1a0000;}\n@block{align:center;padding:40;margin:30;background:#2a1010;border-radius:10;\n  @text{content:\"Error\";size:32;color:#ff4444;bold:true;}\n  @br{size:15;}\n  @text{content:\"" + e + "\";size:16;color:#ff8888;}\n  @text{content:\"" + u + "\";size:14;color:#aa6666;}\n  @br{size:20;}\n  @link{content:\"Home\";url:\"about:home\";color:#0abde3;size:16;}\n}\n";
}
}

HTP::Doc g_doc; Render::Engine g_ren; std::string g_curUrl;
std::vector<std::string> g_hist; int g_histPos = -1;

void loadContent(const std::string& content, const std::string& url) {
    Script::reset(); HTP::Parser p; g_doc = p.parse(content);
    g_curUrl = url; g_ren.setScroll(0);
    std::function<void(std::shared_ptr<HTP::Elem>)> run;
    run = [&](std::shared_ptr<HTP::Elem> e) { if (!e) return; if (e->type == HTP::EType::SCRIPT && !e->scriptCode.empty()) Script::exec(e->scriptCode); for (auto& c : e->children) run(c); };
    run(g_doc.root);
    SetWindowTextA(g_mainWnd, (g_doc.title + " - Lume").c_str());
    SetWindowTextA(g_addressBar, url.c_str());
    invalidateContent();
}
void loadFile(const std::string& fp) {
    std::string path = fp; if (path.length() > 7 && path.substr(0, 7) == "file://") path = path.substr(7);
    std::ifstream f(path); if (!f) { loadContent(Pages::error("Not found", path), "file://" + path); return; }
    std::stringstream ss; ss << f.rdbuf(); f.close();
    loadContent(ss.str(), "file://" + path); setStatus("Loaded: " + path);
}
void navigateTo(const std::string& url) {
    std::string u = url;
    if (g_histPos >= 0 && g_histPos < (int)g_hist.size() - 1) g_hist.resize(g_histPos + 1);
    auto nav = [&](const std::string& c, const std::string& nu) { g_hist.push_back(nu); g_histPos = (int)g_hist.size() - 1; loadContent(c, nu); setStatus("Ready"); };
    if (u == "about:home" || u == "about:blank" || u.empty()) { nav(Pages::home(), "about:home"); return; }
    if (u == "about:info") { nav(Pages::about(), u); return; }
    if (u == "about:plugindemo") { nav(Pages::pluginDemo(), u); return; }
    if (u == "about:gldemo") { nav(Pages::glDemo(), u); return; }
    if (u.length() > 7 && u.substr(0, 7) == "file://") { g_hist.push_back(u); g_histPos = (int)g_hist.size() - 1; loadFile(u); return; }
    if (u.find("://") == std::string::npos) {
        if (u.length() > 4 && u.substr(u.length() - 4) == ".htp") { std::ifstream t(u); if (t) { t.close(); g_hist.push_back("file://" + u); g_histPos = (int)g_hist.size() - 1; loadFile(u); return; } }
        u = "htp://" + u;
    }
    if (u.length() > 6 && u.substr(0, 6) == "htp://") {
        setStatus("Loading..."); auto r = Net::fetch(Net::URL::parse(u));
        g_hist.push_back(u); g_histPos = (int)g_hist.size() - 1;
        if (r.ok && !r.body.empty()) { loadContent(r.body, u); setStatus("OK"); }
        else { loadContent(Pages::error(r.error, u), u); setStatus("Err"); }
        return;
    }
    loadContent(Pages::error("Unknown protocol", u), u);
}
void histNav(const std::string& u) {
    if (u == "about:home") loadContent(Pages::home(), u);
    else if (u == "about:info") loadContent(Pages::about(), u);
    else if (u == "about:plugindemo") loadContent(Pages::pluginDemo(), u);
    else if (u == "about:gldemo") loadContent(Pages::glDemo(), u);
    else if (u.length() > 7 && u.substr(0, 7) == "file://") loadFile(u);
    else { auto r = Net::fetch(Net::URL::parse(u)); if (r.ok) loadContent(r.body, u); else loadContent(Pages::error(r.error, u), u); }
}
void goBack() { if (g_histPos > 0) { g_histPos--; histNav(g_hist[g_histPos]); } }
void goFwd() { if (g_histPos < (int)g_hist.size() - 1) { g_histPos++; histNav(g_hist[g_histPos]); } }

WNDPROC g_origAddr = 0;
LRESULT CALLBACK AddrProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN) { char b[2048] = {}; GetWindowTextA(h, b, 2048); navigateTo(b); return 0; }
    if (m == WM_CHAR && w == VK_RETURN) return 0;
    return CallWindowProcA(g_origAddr, h, m, w, l);
}

LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        Script::g_hwnd = hw;
        CreateWindowA("BUTTON", "<", WS_CHILD | WS_VISIBLE, 4, 6, 30, 28, hw, (HMENU)ID_BACK, 0, 0);
        CreateWindowA("BUTTON", ">", WS_CHILD | WS_VISIBLE, 38, 6, 30, 28, hw, (HMENU)ID_FWD, 0, 0);
        CreateWindowA("BUTTON", "R", WS_CHILD | WS_VISIBLE, 72, 6, 30, 28, hw, (HMENU)ID_REF, 0, 0);
        g_addressBar = CreateWindowA("EDIT", "about:home", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 108, 8, 500, 24, hw, (HMENU)ID_ADDR, 0, 0);
        CreateWindowA("BUTTON", "Go", WS_CHILD | WS_VISIBLE, 614, 6, 40, 28, hw, (HMENU)ID_GO, 0, 0);
        g_statusBar = CreateWindowA("STATIC", "Ready", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 800, STATUS_H, hw, 0, 0, 0);
        { HFONT uf = CreateFontA(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
          SendMessage(g_addressBar, WM_SETFONT, (WPARAM)uf, 1); SendMessage(g_statusBar, WM_SETFONT, (WPARAM)uf, 1); }
        g_origAddr = (WNDPROC)SetWindowLongPtrA(g_addressBar, GWLP_WNDPROC, (LONG_PTR)AddrProc);
        navigateTo("about:home"); return 0;
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp); int aw = w - 108 - 50 - 10; if (aw < 100) aw = 100;
        MoveWindow(g_addressBar, 108, 8, aw, 24, TRUE);
        HWND gb = GetDlgItem(hw, ID_GO); if (gb) MoveWindow(gb, 108 + aw + 4, 6, 40, 28, TRUE);
        MoveWindow(g_statusBar, 0, h - STATUS_H, w, STATUS_H, TRUE);
        g_contentDirty = true; invalidateContent(); return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hw, &ps);
        RECT cr; GetClientRect(hw, &cr);
        int w = cr.right, h = cr.bottom - TOOLBAR_H - STATUS_H; if (h < 1) h = 1;
        if (ps.rcPaint.top < TOOLBAR_H) {
            HBRUSH tb = CreateSolidBrush(RGB(30, 30, 50));
            RECT tr = { ps.rcPaint.left,0,ps.rcPaint.right,TOOLBAR_H }; FillRect(hdc, &tr, tb); DeleteObject(tb);
        }
        if (ps.rcPaint.bottom > TOOLBAR_H) {
            ensureBackbuffer(hdc, w, h);
            if (g_contentDirty) { g_ren.render(g_backDC, w, h, g_doc); g_contentDirty = false; }
            int srcTop = (std::max)(0, (int)ps.rcPaint.top - TOOLBAR_H);
            int srcBot = (std::min)(h, (int)ps.rcPaint.bottom - TOOLBAR_H);
            int srcLeft = (std::max)(0, (int)ps.rcPaint.left);
            int srcRight = (std::min)(w, (int)ps.rcPaint.right);
            if (srcBot > srcTop && srcRight > srcLeft)
                BitBlt(hdc, srcLeft, TOOLBAR_H + srcTop, srcRight - srcLeft, srcBot - srcTop,
                       g_backDC, srcLeft, srcTop, SRCCOPY);
        }
        EndPaint(hw, &ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEWHEEL: {
        int d = GET_WHEEL_DELTA_WPARAM(wp); int s = g_ren.getScroll() - d / 2;
        if (s < 0) s = 0; RECT cr; GetClientRect(hw, &cr);
        int mx = g_ren.totalH() - (cr.bottom - TOOLBAR_H - STATUS_H); if (mx < 0) mx = 0; if (s > mx) s = mx;
        if (s != g_ren.getScroll()) { g_ren.setScroll(s); invalidateContent(); }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int mx = LOWORD(lp), my = HIWORD(lp) - TOOLBAR_H; bool ci = false;
        for (auto& h : g_ren.hits) {
            if (mx >= h.r.left && mx <= h.r.right && my >= h.r.top && my <= h.r.bottom) {
                if (h.isInput) { Script::g_focusId = h.elemId; Script::g_inputs[h.elemId].focused = true; Script::g_inputs[h.elemId].cursor = (int)Script::g_inputs[h.elemId].text.length(); ci = true; invalidateContent(); break; }
                if (h.isBtn && !h.elemId.empty()) { Script::fireClick(h.elemId); invalidateContent(); if (h.url.empty()) return 0; }
                if (!h.url.empty()) { navigateTo(h.url); return 0; }
            }
        }
        if (!ci) { Script::g_focusId.clear(); for (auto& kv : Script::g_inputs) kv.second.focused = false; invalidateContent(); }
        SetFocus(hw); return 0;
    }
    case WM_CHAR: {
        if (!Script::g_focusId.empty() && GetFocus() == hw) {
            auto it = Script::g_inputs.find(Script::g_focusId);
            if (it != Script::g_inputs.end()) {
                auto& inp = it->second; char c = (char)wp;
                if (c == '\b') { if (inp.cursor > 0 && !inp.text.empty()) { inp.text.erase(inp.cursor - 1, 1); inp.cursor--; } }
                else if (c == '\r' || c == '\n') { inp.focused = false; Script::g_focusId.clear(); }
                else if (c >= 32) { inp.text.insert(inp.cursor, 1, c); inp.cursor++; }
                invalidateContent();
            }
            return 0;
        }
        break;
    }
    case WM_KEYDOWN: {
        if (Script::g_keyDownRef != LUA_NOREF && Script::g_L && GetFocus() == hw && Script::g_focusId.empty()) {
            lua_rawgeti(Script::g_L, LUA_REGISTRYINDEX, Script::g_keyDownRef);
            lua_pushinteger(Script::g_L, (int)wp);
            if (lua_pcall(Script::g_L, 1, 0, 0) != 0) { OutputDebugStringA(lua_tostring(Script::g_L, -1)); lua_pop(Script::g_L, 1); }
        }
        if (!Script::g_focusId.empty() && GetFocus() == hw) {
            auto it = Script::g_inputs.find(Script::g_focusId);
            if (it != Script::g_inputs.end()) {
                auto& inp = it->second;
                if (wp == VK_LEFT) { if (inp.cursor > 0) inp.cursor--; invalidateContent(); return 0; }
                if (wp == VK_RIGHT) { if (inp.cursor < (int)inp.text.length()) inp.cursor++; invalidateContent(); return 0; }
                if (wp == VK_HOME) { inp.cursor = 0; invalidateContent(); return 0; }
                if (wp == VK_END) { inp.cursor = (int)inp.text.length(); invalidateContent(); return 0; }
                if (wp == VK_DELETE) { if (inp.cursor < (int)inp.text.length()) { inp.text.erase(inp.cursor, 1); invalidateContent(); } return 0; }
                if (wp == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    if (OpenClipboard(hw)) { HANDLE h = GetClipboardData(CF_TEXT); if (h) { const char* cl = (const char*)GlobalLock(h); if (cl) { std::string p = cl; p.erase(std::remove(p.begin(), p.end(), '\r'), p.end()); p.erase(std::remove(p.begin(), p.end(), '\n'), p.end()); inp.text.insert(inp.cursor, p); inp.cursor += (int)p.length(); GlobalUnlock(h); } } CloseClipboard(); }
                    invalidateContent(); return 0;
                }
            }
        }
        if (wp == VK_F5) navigateTo(g_curUrl);
        else if (wp == VK_BACK && Script::g_focusId.empty() && GetFocus() != g_addressBar) goBack();
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hw, &pt); pt.y -= TOOLBAR_H;
            bool ov = false;
            for (auto& h : g_ren.hits) { if (pt.x >= h.r.left && pt.x <= h.r.right && pt.y >= h.r.top && pt.y <= h.r.bottom) { if (!h.url.empty() || h.isBtn) { ov = true; break; } if (h.isInput) { SetCursor(LoadCursor(0, IDC_IBEAM)); return TRUE; } } }
            SetCursor(LoadCursor(0, ov ? IDC_HAND : IDC_ARROW)); return TRUE;
        }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_GO) { char b[2048] = {}; GetWindowTextA(g_addressBar, b, 2048); navigateTo(b); }
        else if (id == ID_BACK) goBack(); else if (id == ID_FWD) goFwd(); else if (id == ID_REF) navigateTo(g_curUrl);
        return 0;
    }
    case WM_DROPFILES: {
        HDROP hd = (HDROP)wp; char fp[MAX_PATH] = {}; DragQueryFileA(hd, 0, fp, MAX_PATH); DragFinish(hd);
        std::string p = fp; if (p.length() > 4 && p.substr(p.length() - 4) == ".htp") loadFile(p); return 0;
    }
    case WM_TIMER: return 0;
    case WM_DESTROY: Script::reset(); Plugins::unloadAll(); cleanupBackbuffer(); FontCache::clear(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hw, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmd, int show) {
    WSADATA wd; WSAStartup(MAKEWORD(2, 2), &wd);
    INITCOMMONCONTROLSEX ic = { sizeof(ic),ICC_STANDARD_CLASSES }; InitCommonControlsEx(&ic);
    GLLoader::load(); Plugins::initHostAPI(); Plugins::discoverPlugins();
    WNDCLASSEXA wc = {}; wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc; wc.hInstance = hI; wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = 0; wc.lpszClassName = "LumeClass"; wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    RegisterClassExA(&wc);
    g_mainWnd = CreateWindowExA(WS_EX_ACCEPTFILES, "LumeClass", "Lume", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, 0, 0, hI, 0);
    if (!g_mainWnd) return 1;
    ShowWindow(g_mainWnd, show); UpdateWindow(g_mainWnd);
    if (cmd && strlen(cmd) > 0) { std::string a = cmd; if (!a.empty() && a.front() == '"') a.erase(a.begin()); if (!a.empty() && a.back() == '"') a.pop_back(); if (!a.empty()) navigateTo(a); }
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    Plugins::unloadAll(); cleanupBackbuffer(); FontCache::clear(); GLLoader::unload(); WSACleanup();
    return (int)msg.wParam;
}
