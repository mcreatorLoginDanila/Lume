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
#include <regex>

extern "C" {
#include "lib/lua.h"
#include "lib/lauxlib.h"
#include "lib/lualib.h"
}

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

void navigateTo(const std::string& url);
void invalidateContent();

HWND g_mainWnd = nullptr;
HWND g_addressBar = nullptr;
HWND g_statusBar = nullptr;
const int TOOLBAR_H = 40;
const int STATUS_H = 24;

#define ID_ADDR  1001
#define ID_GO    1002
#define ID_BACK  1003
#define ID_FWD   1004
#define ID_REF   1005

void invalidateContent() {
    if (!g_mainWnd) return;
    RECT cr;
    GetClientRect(g_mainWnd, &cr);
    RECT contentRect = { 0, TOOLBAR_H, cr.right, cr.bottom - STATUS_H };
    InvalidateRect(g_mainWnd, &contentRect, FALSE);
}

void setStatus(const std::string& t) {
    if (g_statusBar) SetWindowTextA(g_statusBar, t.c_str());
}

// parser
namespace HTP {

struct Color {
    int r=255,g=255,b=255,a=255;
    static Color fromHex(const std::string& hex) {
        Color c; std::string h=hex;
        if(!h.empty()&&h[0]=='#') h=h.substr(1);
        try {
            if(h.length()>=6){c.r=std::stoi(h.substr(0,2),0,16);c.g=std::stoi(h.substr(2,2),0,16);c.b=std::stoi(h.substr(4,2),0,16);}
            if(h.length()>=8) c.a=std::stoi(h.substr(6,2),0,16);
        } catch(...){}
        return c;
    }
    COLORREF cr() const { return RGB(r,g,b); }
};

enum class EType {
    PAGE,BLOCK,TEXT,IMAGE,LINK,BUTTON,INPUT_FIELD,
    DIVIDER,LIST,ITEM,BR,SCRIPT,CANVAS,ROW,COLUMN,UNKNOWN
};

struct Props {
    std::map<std::string,std::string> d;
    std::string get(const std::string& k,const std::string& def="") const {
        auto i=d.find(k); return i!=d.end()?i->second:def;
    }
    int getInt(const std::string& k,int def=0) const {
        auto i=d.find(k); if(i!=d.end()){try{return std::stoi(i->second);}catch(...){}} return def;
    }
    bool getBool(const std::string& k,bool def=false) const {
        auto i=d.find(k); if(i!=d.end()) return i->second=="true"||i->second=="1"; return def;
    }
    Color getColor(const std::string& k,Color def={255,255,255}) const {
        auto i=d.find(k); if(i!=d.end()) return Color::fromHex(i->second); return def;
    }
};

struct Elem {
    EType type=EType::UNKNOWN;
    std::string tag;
    Props props;
    std::string scriptCode;
    std::vector<std::shared_ptr<Elem>> children;
};

struct Doc {
    std::shared_ptr<Elem> root;
    std::string title;
    Color bg={26,26,46};
};

// recursive parser that works on the source string
class Parser {
    std::string src;
    int pos=0;
    int len=0;

    void skipWS() {
        while(pos<len) {
            char c=src[pos];
            if(c==' '||c=='\t'||c=='\r'||c=='\n') { pos++; continue; }
            if(c=='/'&&pos+1<len&&src[pos+1]=='/') { while(pos<len&&src[pos]!='\n') pos++; continue; }
            if(c=='/'&&pos+1<len&&src[pos+1]=='*') {
                pos+=2; while(pos+1<len&&!(src[pos]=='*'&&src[pos+1]=='/')) pos++;
                if(pos+1<len) pos+=2; continue;
            }
            break;
        }
    }

    std::string readIdent() {
        std::string r;
        while(pos<len&&(std::isalnum((unsigned char)src[pos])||src[pos]=='_'||src[pos]=='-'))
            { r+=src[pos]; pos++; }
        return r;
    }

    std::string readString() {
        if(pos>=len||src[pos]!='"') return "";
        pos++; // skip "
        std::string r;
        while(pos<len&&src[pos]!='"') {
            if(src[pos]=='\\'&&pos+1<len) {
                pos++;
                if(src[pos]=='n') r+='\n';
                else if(src[pos]=='t') r+='\t';
                else if(src[pos]=='"') r+='"';
                else if(src[pos]=='\\') r+='\\';
                else r+=src[pos];
            } else r+=src[pos];
            pos++;
        }
        if(pos<len) pos++; // skip "
        return r;
    }

    std::string readValue() {
        skipWS();
        std::string val;
        if(pos<len&&src[pos]=='"') return readString();
        // read until ; or }
        while(pos<len&&src[pos]!=';'&&src[pos]!='}') {
            val+=src[pos]; pos++;
        }
        while(!val.empty()&&(val.back()==' '||val.back()=='\t'||val.back()=='\r'||val.back()=='\n'))
            val.pop_back();
        return val;
    }

    // (for @script thing)
    std::string readRawBlock() {
        int depth=1;
        int start=pos;
        bool inStr=false, inLC=false, inBC=false;
        while(pos<len&&depth>0) {
            char c=src[pos];
            if(inLC) { if(c=='\n') inLC=false; pos++; continue; }
            if(inBC) { if(c=='*'&&pos+1<len&&src[pos+1]=='/'){inBC=false;pos+=2;continue;} pos++; continue; }
            if(inStr) { if(c=='\\'){pos+=2;continue;} if(c=='"') inStr=false; pos++; continue; }
            if(c=='"'){inStr=true;pos++;continue;}
            if(c=='/'&&pos+1<len&&src[pos+1]=='/'){inLC=true;pos+=2;continue;}
            if(c=='/'&&pos+1<len&&src[pos+1]=='*'){inBC=true;pos+=2;continue;}
            if(c=='{') depth++;
            else if(c=='}') { depth--; if(depth==0) break; }
            pos++;
        }
        std::string code = src.substr(start, pos-start);
        if(pos<len&&src[pos]=='}') pos++; // skip closing }
        return code;
    }

    void parseInlineAttrs(std::shared_ptr<Elem>& e) {
        skipWS();
        if(pos>=len||src[pos]!='(') return;
        pos++; // skip (
        while(pos<len&&src[pos]!=')') {
            skipWS();
            std::string key = readIdent();
            skipWS();
            if(pos<len&&src[pos]=='=') {
                pos++; skipWS();
                std::string val = readString();
                e->props.d[key] = val;
            }
            skipWS();
            if(pos<len&&src[pos]==',') pos++;
        }
        if(pos<len) pos++; // skip )
    }

    EType tagToType(const std::string& t) {
        if(t=="page") return EType::PAGE;
        if(t=="block") return EType::BLOCK;
        if(t=="text") return EType::TEXT;
        if(t=="image") return EType::IMAGE;
        if(t=="link") return EType::LINK;
        if(t=="button") return EType::BUTTON;
        if(t=="input") return EType::INPUT_FIELD;
        if(t=="divider") return EType::DIVIDER;
        if(t=="list") return EType::LIST;
        if(t=="item") return EType::ITEM;
        if(t=="br") return EType::BR;
        if(t=="script") return EType::SCRIPT;
        if(t=="canvas") return EType::CANVAS;
        if(t=="row") return EType::ROW;
        if(t=="column") return EType::COLUMN;
        return EType::UNKNOWN;
    }

    std::shared_ptr<Elem> parseElement() {
        skipWS();
        if(pos>=len||src[pos]!='@') return nullptr;
        pos++;
        auto e = std::make_shared<Elem>();
        e->tag = readIdent();
        e->type = tagToType(e->tag);

        parseInlineAttrs(e);
        skipWS();

        if(pos>=len||src[pos]!='{') return e;
        pos++;

        // extract raw Lua code
        if(e->type == EType::SCRIPT) {
            e->scriptCode = readRawBlock();
            return e;
        }

        while(pos<len) {
            skipWS();
            if(pos>=len) break;
            if(src[pos]=='}') { pos++; break; }

            if(src[pos]=='@') {
                auto child = parseElement();
                if(child) e->children.push_back(child);
            } else if(std::isalpha((unsigned char)src[pos])||src[pos]=='_') {
                std::string key = readIdent();
                skipWS();
                if(pos<len&&src[pos]==':') {
                    pos++; // skip :
                    std::string val = readValue();
                    e->props.d[key] = val;
                    skipWS();
                    if(pos<len&&src[pos]==';') pos++;
                }
            } else {
                pos++;
            }
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

        while(pos<len) {
            skipWS();
            if(pos>=len) break;
            if(src[pos]=='@') {
                auto e = parseElement();
                if(e) {
                    if(e->type==EType::PAGE) {
                        doc.title = e->props.get("title","Lume");
                        doc.bg = e->props.getColor("background",{26,26,46});
                        for(auto& ch:e->children) root->children.push_back(ch);
                    } else {
                        root->children.push_back(e);
                    }
                }
            } else pos++;
        }
        doc.root = root;
        return doc;
    }
};

}

namespace Net {
struct URL {
    std::string proto,host,path; int port=8080;
    static URL parse(const std::string& s) {
        URL u; std::string r=s;
        auto pe=r.find("://");
        if(pe!=std::string::npos){u.proto=r.substr(0,pe);r=r.substr(pe+3);}else u.proto="htp";
        auto ps=r.find('/');
        if(ps!=std::string::npos){u.path=r.substr(ps);r=r.substr(0,ps);}else u.path="/";
        auto pp=r.find(':');
        if(pp!=std::string::npos){u.host=r.substr(0,pp);try{u.port=std::stoi(r.substr(pp+1));}catch(...){u.port=8080;}}
        else{u.host=r;u.port=8080;}
        if(u.path=="/"||u.path.empty()) u.path="/index.htp";
        return u;
    }
};
struct Resp { int code=0; std::string body,error; bool ok=false; };
Resp fetch(const URL& url) {
    Resp resp;
    addrinfo hints={},*res=0; hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
    if(getaddrinfo(url.host.c_str(),std::to_string(url.port).c_str(),&hints,&res)!=0)
        {resp.error="DNS fail: "+url.host;return resp;}
    SOCKET s=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(s==INVALID_SOCKET){freeaddrinfo(res);resp.error="Socket fail";return resp;}
    DWORD to=5000;
    setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char*)&to,sizeof(to));
    setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,(char*)&to,sizeof(to));
    if(connect(s,res->ai_addr,(int)res->ai_addrlen)==SOCKET_ERROR)
        {freeaddrinfo(res);closesocket(s);resp.error="Connect fail";return resp;}
    freeaddrinfo(res);
    std::string req="GET "+url.path+" HTTP/1.1\r\nHost: "+url.host+
        "\r\nUser-Agent: Lume\r\nConnection: close\r\n\r\n";
    send(s,req.c_str(),(int)req.length(),0);
    std::string raw;char buf[4096];int n;
    while((n=recv(s,buf,sizeof(buf)-1,0))>0) raw.append(buf,n);
    closesocket(s);
    if(raw.empty()){resp.error="Empty";return resp;}
    auto he=raw.find("\r\n\r\n");
    if(he==std::string::npos){resp.body=raw;resp.ok=true;return resp;}
    resp.body=raw.substr(he+4);
    auto fl=raw.find("\r\n"); auto sl=raw.substr(0,fl);
    auto sp=sl.find(' ');
    if(sp!=std::string::npos){try{resp.code=std::stoi(sl.substr(sp+1,3));}catch(...){}}
    resp.ok=(resp.code>=200&&resp.code<400);
    if(!resp.ok) resp.error="HTTP "+std::to_string(resp.code);
    return resp;
}
Resp fetchUrl(const std::string& u){return fetch(URL::parse(u));}
}

// CANVAS
namespace Canvas {
struct Buf {
    int w=0,h=0; HDC dc=0; HBITMAP bmp=0,old=0;
    void create(HDC ref,int ww,int hh){
        cleanup();w=ww;h=hh;
        dc=CreateCompatibleDC(ref);bmp=CreateCompatibleBitmap(ref,w,h);
        old=(HBITMAP)SelectObject(dc,bmp);
        HBRUSH b=CreateSolidBrush(0);RECT r={0,0,w,h};FillRect(dc,&r,b);DeleteObject(b);
    }
    void cleanup(){
        if(dc){SelectObject(dc,old);DeleteDC(dc);dc=0;}
        if(bmp){DeleteObject(bmp);bmp=0;}
    }
    ~Buf(){cleanup();}
};
std::map<std::string,std::shared_ptr<Buf>> g_bufs;
std::shared_ptr<Buf> get(const std::string& id,HDC ref,int w,int h){
    auto i=g_bufs.find(id);
    if(i!=g_bufs.end()&&i->second->w==w&&i->second->h==h) return i->second;
    auto b=std::make_shared<Buf>();b->create(ref,w,h);g_bufs[id]=b;return b;
}
} // namespace Canvas

// SCRIPTS
namespace Script {

struct InputState {
    std::string text,placeholder;
    int x=0,y=0,w=250,h=28;
    bool focused=false;
    int cursor=0;
};

std::map<std::string,InputState> g_inputs;
std::map<std::string,std::string> g_texts; // id->displayed text
std::map<std::string,std::function<void()>> g_clicks;
std::map<int,int> g_timerRefs;
int g_timerN=9000;
lua_State* g_L=nullptr;
HWND g_hwnd=nullptr;
std::string g_focusId;

static int l_set_text(lua_State*L){
    const char*id=luaL_checkstring(L,1);const char*t=luaL_checkstring(L,2);
    g_texts[id]=t;
    auto i=g_inputs.find(id);if(i!=g_inputs.end())i->second.text=t;
    invalidateContent();return 0;
}
static int l_get_text(lua_State*L){
    const char*id=luaL_checkstring(L,1);
    auto i=g_texts.find(id);
    if(i!=g_texts.end()) lua_pushstring(L,i->second.c_str());
    else{auto j=g_inputs.find(id);if(j!=g_inputs.end())lua_pushstring(L,j->second.text.c_str());
    else lua_pushstring(L,"");}
    return 1;
}
static int l_on_click(lua_State*L){
    const char*id=luaL_checkstring(L,1);
    luaL_checktype(L,2,LUA_TFUNCTION);
    lua_pushvalue(L,2);int ref=luaL_ref(L,LUA_REGISTRYINDEX);
    std::string sid=id;
    g_clicks[sid]=[sid,ref](){
        if(!g_L)return;
        lua_rawgeti(g_L,LUA_REGISTRYINDEX,ref);
        if(lua_pcall(g_L,0,0,0)!=0){
            const char*e=lua_tostring(g_L,-1);
            OutputDebugStringA(e?e:"lua err");OutputDebugStringA("\n");
            lua_pop(g_L,1);
        }
    };
    return 0;
}
static int l_get_mouse(lua_State*L){
    POINT p;GetCursorPos(&p);if(g_hwnd)ScreenToClient(g_hwnd,&p);
    lua_pushinteger(L,p.x);lua_pushinteger(L,p.y);return 2;
}
static int l_http(lua_State*L){
    const char*u=luaL_checkstring(L,1);
    auto r=Net::fetchUrl(u);
    lua_pushstring(L,r.body.c_str());lua_pushinteger(L,r.code);return 2;
}
static int l_navigate(lua_State*L){navigateTo(luaL_checkstring(L,1));return 0;}
static int l_alert(lua_State*L){MessageBoxA(g_hwnd,luaL_checkstring(L,1),"Lume",MB_OK);return 0;}
static int l_refresh(lua_State*L){invalidateContent();return 0;}
static int l_get_input(lua_State*L){
    auto i=g_inputs.find(luaL_checkstring(L,1));
    lua_pushstring(L,i!=g_inputs.end()?i->second.text.c_str():"");return 1;
}
static int l_set_input(lua_State*L){
    g_inputs[luaL_checkstring(L,1)].text=luaL_checkstring(L,2);
    invalidateContent();return 0;
}

static int l_cv_clear(lua_State*L){
    auto i=Canvas::g_bufs.find(luaL_checkstring(L,1));
    if(i!=Canvas::g_bufs.end()&&i->second->dc){
        auto c=HTP::Color::fromHex(luaL_optstring(L,2,"#000000"));
        HBRUSH b=CreateSolidBrush(c.cr());RECT r={0,0,i->second->w,i->second->h};
        FillRect(i->second->dc,&r,b);DeleteObject(b);
    }return 0;
}
static int l_cv_rect(lua_State*L){
    auto i=Canvas::g_bufs.find(luaL_checkstring(L,1));
    if(i!=Canvas::g_bufs.end()&&i->second->dc){
        int x=(int)luaL_checkinteger(L,2),y=(int)luaL_checkinteger(L,3),
            w=(int)luaL_checkinteger(L,4),h=(int)luaL_checkinteger(L,5);
        auto c=HTP::Color::fromHex(luaL_optstring(L,6,"#ffffff"));
        HBRUSH b=CreateSolidBrush(c.cr());RECT r={x,y,x+w,y+h};
        FillRect(i->second->dc,&r,b);DeleteObject(b);
    }return 0;
}
static int l_cv_circle(lua_State*L){
    auto i=Canvas::g_bufs.find(luaL_checkstring(L,1));
    if(i!=Canvas::g_bufs.end()&&i->second->dc){
        int cx=(int)luaL_checkinteger(L,2),cy=(int)luaL_checkinteger(L,3),
            r=(int)luaL_checkinteger(L,4);
        auto c=HTP::Color::fromHex(luaL_optstring(L,5,"#ffffff"));
        HBRUSH b=CreateSolidBrush(c.cr());HPEN p=CreatePen(PS_SOLID,1,c.cr());
        auto ob=SelectObject(i->second->dc,b);auto op=SelectObject(i->second->dc,p);
        Ellipse(i->second->dc,cx-r,cy-r,cx+r,cy+r);
        SelectObject(i->second->dc,ob);SelectObject(i->second->dc,op);
        DeleteObject(b);DeleteObject(p);
    }return 0;
}
static int l_cv_line(lua_State*L){
    auto i=Canvas::g_bufs.find(luaL_checkstring(L,1));
    if(i!=Canvas::g_bufs.end()&&i->second->dc){
        int x1=(int)luaL_checkinteger(L,2),y1=(int)luaL_checkinteger(L,3),
            x2=(int)luaL_checkinteger(L,4),y2=(int)luaL_checkinteger(L,5);
        auto c=HTP::Color::fromHex(luaL_optstring(L,6,"#ffffff"));
        int t=(int)luaL_optinteger(L,7,1);
        HPEN p=CreatePen(PS_SOLID,t,c.cr());
        auto op=SelectObject(i->second->dc,p);
        MoveToEx(i->second->dc,x1,y1,0);LineTo(i->second->dc,x2,y2);
        SelectObject(i->second->dc,op);DeleteObject(p);
    }return 0;
}
static int l_cv_text(lua_State*L){
    auto i=Canvas::g_bufs.find(luaL_checkstring(L,1));
    if(i!=Canvas::g_bufs.end()&&i->second->dc){
        int x=(int)luaL_checkinteger(L,2),y=(int)luaL_checkinteger(L,3);
        const char*txt=luaL_checkstring(L,4);
        int sz=(int)luaL_optinteger(L,5,16);
        auto c=HTP::Color::fromHex(luaL_optstring(L,6,"#ffffff"));
        HFONT f=CreateFontA(-sz,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
        auto of=SelectObject(i->second->dc,f);
        SetTextColor(i->second->dc,c.cr());SetBkMode(i->second->dc,TRANSPARENT);
        TextOutA(i->second->dc,x,y,txt,(int)strlen(txt));
        SelectObject(i->second->dc,of);DeleteObject(f);
    }return 0;
}

static int l_set_timer(lua_State*L){
    int ms=(int)luaL_checkinteger(L,1);
    luaL_checktype(L,2,LUA_TFUNCTION);
    lua_pushvalue(L,2);int ref=luaL_ref(L,LUA_REGISTRYINDEX);
    int tid=++g_timerN; g_timerRefs[tid]=ref;
    SetTimer(g_hwnd,tid,ms,[](HWND,UINT,UINT_PTR id,DWORD){
        auto i=g_timerRefs.find((int)id);
        if(i!=g_timerRefs.end()&&g_L){
            lua_rawgeti(g_L,LUA_REGISTRYINDEX,i->second);
            if(lua_pcall(g_L,0,0,0)!=0) lua_pop(g_L,1);
        }
    });
    lua_pushinteger(L,tid);return 1;
}
static int l_kill_timer(lua_State*L){
    int id=(int)luaL_checkinteger(L,1);
    KillTimer(g_hwnd,id);g_timerRefs.erase(id);return 0;
}

void init(){
    if(g_L) lua_close(g_L);
    g_L=luaL_newstate();luaL_openlibs(g_L);
    lua_register(g_L,"set_text",l_set_text);
    lua_register(g_L,"get_text",l_get_text);
    lua_register(g_L,"on_click",l_on_click);
    lua_register(g_L,"get_mouse_pos",l_get_mouse);
    lua_register(g_L,"http_request",l_http);
    lua_register(g_L,"navigate",l_navigate);
    lua_register(g_L,"alert",l_alert);
    lua_register(g_L,"refresh",l_refresh);
    lua_register(g_L,"get_input",l_get_input);
    lua_register(g_L,"set_input",l_set_input);
    lua_register(g_L,"canvas_clear",l_cv_clear);
    lua_register(g_L,"canvas_rect",l_cv_rect);
    lua_register(g_L,"canvas_circle",l_cv_circle);
    lua_register(g_L,"canvas_line",l_cv_line);
    lua_register(g_L,"canvas_text",l_cv_text);
    lua_register(g_L,"set_timer",l_set_timer);
    lua_register(g_L,"kill_timer",l_kill_timer);
}

void exec(const std::string& code){
    if(!g_L) init();
    if(luaL_dostring(g_L,code.c_str())!=0){
        const char*e=lua_tostring(g_L,-1);
        std::string em=e?e:"Unknown error";
        OutputDebugStringA(("LUA ERR: "+em+"\n").c_str());
        MessageBoxA(g_hwnd,em.c_str(),"Lua Error",MB_OK|MB_ICONERROR);
        lua_pop(g_L,1);
    }
}

void fireClick(const std::string& id){
    auto i=g_clicks.find(id);if(i!=g_clicks.end()) i->second();
}

void reset(){
    for(auto&kv:g_timerRefs) KillTimer(g_hwnd,kv.first);
    g_timerRefs.clear();g_timerN=9000;
    g_clicks.clear();g_texts.clear();g_inputs.clear();g_focusId.clear();
    Canvas::g_bufs.clear();
    if(g_L){lua_close(g_L);g_L=nullptr;}
}
}

// RENDER
namespace Render {

struct Hit {
    RECT r; std::string url,action,elemId;
    bool isInput=false,isBtn=false;
};

class Engine {
    int scrollY=0,contentH=0,curY=0;
    std::vector<Hit>*pH=nullptr;
    HDC refDC=nullptr;

    HFONT mkFont(int sz,bool b=false,bool i=false,const char*f="Segoe UI"){
        return CreateFontA(-sz,0,0,0,b?FW_BOLD:FW_NORMAL,i,0,0,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,f);
    }
    void fillRR(HDC dc,int x,int y,int w,int h,HTP::Color c,int rad=5){
        HBRUSH b=CreateSolidBrush(c.cr());HPEN p=CreatePen(PS_SOLID,1,c.cr());
        auto ob=SelectObject(dc,b);auto op=SelectObject(dc,p);
        RoundRect(dc,x,y,x+w,y+h,rad,rad);
        SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);
    }
    int estH(std::shared_ptr<HTP::Elem> e){
        if(!e) return 0;
        switch(e->type){
            case HTP::EType::TEXT:case HTP::EType::ITEM: return e->props.getInt("size",16)+8;
            case HTP::EType::LINK: return e->props.getInt("size",16)+8;
            case HTP::EType::BUTTON: return e->props.getInt("height",35)+8;
            case HTP::EType::INPUT_FIELD: return e->props.getInt("height",28)+8;
            case HTP::EType::DIVIDER: return e->props.getInt("margin",10)*2+e->props.getInt("thickness",1);
            case HTP::EType::IMAGE: return e->props.getInt("height",100)+8;
            case HTP::EType::BR: return e->props.getInt("size",16);
            case HTP::EType::CANVAS: return e->props.getInt("height",200)+8;
            case HTP::EType::SCRIPT: return 0;
            case HTP::EType::BLOCK:case HTP::EType::COLUMN:{
                int h=e->props.getInt("padding",10)*2+e->props.getInt("margin",5)*2;
                for(auto&c:e->children)h+=estH(c);return h;}
            case HTP::EType::ROW:{int m=0;for(auto&c:e->children)m=(std::max)(m,estH(c));
                return m+e->props.getInt("margin",5)*2;}
            case HTP::EType::LIST:{int h=0;for(auto&c:e->children)h+=estH(c);return h;}
            default: return 20;
        }
    }
    int estW(std::shared_ptr<HTP::Elem> e,int mw){
        if(!e) return mw;
        switch(e->type){
            case HTP::EType::BUTTON: return e->props.getInt("width",120);
            case HTP::EType::INPUT_FIELD: return e->props.getInt("width",250);
            case HTP::EType::IMAGE: return e->props.getInt("width",100);
            case HTP::EType::CANVAS: return e->props.getInt("width",400);
            default: return mw;
        }
    }

    void draw(HDC dc,std::shared_ptr<HTP::Elem> e,int x,int y,int mw){
        if(!e) return;
        switch(e->type){
        case HTP::EType::PAGE:case HTP::EType::UNKNOWN:{
            int cy=y;for(auto&c:e->children){draw(dc,c,x,cy,mw);cy=curY;}break;}
        case HTP::EType::SCRIPT: curY=y; break;
        case HTP::EType::ROW:{
            int mar=e->props.getInt("margin",5),pad=e->props.getInt("padding",0),
                gap=e->props.getInt("gap",10);
            int nc=0,fw=0,fc=0;
            for(auto&c:e->children){nc++;int w=c->props.getInt("width",0);if(w>0)fw+=w;else fc++;}
            int avail=mw-mar*2-pad*2-gap*((std::max)(nc-1,0));
            int flexW=fc>0?(avail-fw)/fc:0;if(flexW<50)flexW=50;
            int cx=x+mar+pad,ry=y+mar,maxH=0;
            for(auto&c:e->children){
                int cw=c->props.getInt("width",flexW);
                int sv=curY;curY=ry;draw(dc,c,cx,ry,cw);
                int ch=curY-ry;if(ch>maxH)maxH=ch;cx+=cw+gap;curY=sv;
            }
            curY=ry+maxH+mar;break;}
        case HTP::EType::COLUMN:{
            int pad=e->props.getInt("padding",5),rad=e->props.getInt("border-radius",4);
            std::string bg=e->props.get("background","");
            int th=0;for(auto&c:e->children)th+=estH(c);th+=pad*2;
            if(!bg.empty()) fillRR(dc,x,y-scrollY,mw,th,HTP::Color::fromHex(bg),rad);
            curY=y+pad;for(auto&c:e->children) draw(dc,c,x+pad,curY,mw-pad*2);
            curY+=pad;break;}
        case HTP::EType::BLOCK:{
            int pad=e->props.getInt("padding",10),mar=e->props.getInt("margin",5),
                rad=e->props.getInt("border-radius",6);
            std::string bg=e->props.get("background",""),align=e->props.get("align","left");
            int iy=y+mar+pad,ty=iy;for(auto&c:e->children)ty+=estH(c);int bh=(ty-iy)+pad*2;
            if(!bg.empty()) fillRR(dc,x+mar,y+mar-scrollY,mw-mar*2,bh,HTP::Color::fromHex(bg),rad);
            curY=iy;
            for(auto&c:e->children){
                int cx=x+mar+pad,cmw=mw-(mar+pad)*2;
                if(align=="center"){int ew=estW(c,cmw);cx=x+(mw-ew)/2;}
                draw(dc,c,cx,curY,cmw);
            }
            curY+=pad+mar;break;}
        case HTP::EType::TEXT:{
            std::string id=e->props.get("id",""),ct=e->props.get("content","");
            if(!id.empty()){auto i=Script::g_texts.find(id);
                if(i!=Script::g_texts.end())ct=i->second;else Script::g_texts[id]=ct;}
            int sz=e->props.getInt("size",16);auto col=e->props.getColor("color",{255,255,255});
            bool bold=e->props.getBool("bold"),ital=e->props.getBool("italic");
            std::string face=e->props.get("font","Segoe UI");
            HFONT f=mkFont(sz,bold,ital,face.c_str());auto of=SelectObject(dc,f);
            SetTextColor(dc,col.cr());SetBkMode(dc,TRANSPARENT);
            RECT rc={x,y-scrollY,x+mw,y-scrollY+1000};
            DrawTextA(dc,ct.c_str(),-1,&rc,DT_WORDBREAK|DT_CALCRECT);
            int th=rc.bottom-rc.top;rc.bottom=rc.top+th;
            DrawTextA(dc,ct.c_str(),-1,&rc,DT_WORDBREAK);
            SelectObject(dc,of);DeleteObject(f);curY=y+th+4;break;}
        case HTP::EType::LINK:{
            std::string ct=e->props.get("content","[link]"),url=e->props.get("url","");
            int sz=e->props.getInt("size",16);auto col=e->props.getColor("color",{10,189,227});
            HFONT f=mkFont(sz);auto of=SelectObject(dc,f);
            SetTextColor(dc,col.cr());SetBkMode(dc,TRANSPARENT);
            SIZE ts;GetTextExtentPoint32A(dc,ct.c_str(),(int)ct.length(),&ts);
            RECT rc={x,y-scrollY,x+ts.cx,y-scrollY+ts.cy};DrawTextA(dc,ct.c_str(),-1,&rc,0);
            HPEN p=CreatePen(PS_SOLID,1,col.cr());auto op=SelectObject(dc,p);
            MoveToEx(dc,x,y-scrollY+ts.cy-1,0);LineTo(dc,x+ts.cx,y-scrollY+ts.cy-1);
            SelectObject(dc,op);DeleteObject(p);SelectObject(dc,of);DeleteObject(f);
            if(pH){Hit h;h.r={x,y-scrollY,x+ts.cx,y-scrollY+ts.cy};h.url=url;pH->push_back(h);}
            curY=y+ts.cy+4;break;}
        case HTP::EType::BUTTON:{
            std::string id=e->props.get("id",""),ct=e->props.get("content","Button"),
                act=e->props.get("action",""),url=e->props.get("url",act);
            int bw=e->props.getInt("width",120),bh=e->props.getInt("height",35),
                sz=e->props.getInt("size",14),rad=e->props.getInt("border-radius",6);
            auto bg=e->props.getColor("background",{233,69,96});
            auto col=e->props.getColor("color",{255,255,255});
            int bx=x,by=y-scrollY;
            fillRR(dc,bx,by,bw,bh,bg,rad);
            HFONT f=mkFont(sz,true);auto of=SelectObject(dc,f);
            SetTextColor(dc,col.cr());SetBkMode(dc,TRANSPARENT);
            RECT rc={bx,by,bx+bw,by+bh};
            DrawTextA(dc,ct.c_str(),-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            SelectObject(dc,of);DeleteObject(f);
            if(pH){Hit h;h.r={bx,by,bx+bw,by+bh};h.url=url;h.action=act;
                h.elemId=id;h.isBtn=true;pH->push_back(h);}
            curY=y+bh+8;break;}
        case HTP::EType::INPUT_FIELD:{
            std::string id=e->props.get("id","inp_"+std::to_string(y)),
                ph=e->props.get("placeholder","");
            int iw=e->props.getInt("width",250),ih=e->props.getInt("height",28);
            auto&inp=Script::g_inputs[id];
            if(inp.placeholder.empty()&&!ph.empty()) inp.placeholder=ph;
            inp.x=x;inp.y=y-scrollY;inp.w=iw;inp.h=ih;
            int ix=x,iy=y-scrollY;
            bool foc=(Script::g_focusId==id);
            HBRUSH br=CreateSolidBrush(foc?RGB(50,50,80):RGB(40,40,60));
            HPEN pn=CreatePen(PS_SOLID,foc?2:1,foc?RGB(10,189,227):RGB(100,100,140));
            auto ob=SelectObject(dc,br);auto op=SelectObject(dc,pn);
            RoundRect(dc,ix,iy,ix+iw,iy+ih,4,4);
            SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(br);DeleteObject(pn);
            HFONT f=mkFont(14);auto of=SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);
            if(!inp.text.empty()){
                SetTextColor(dc,RGB(230,230,240));
                RECT rc={ix+6,iy+2,ix+iw-4,iy+ih-2};
                DrawTextA(dc,inp.text.c_str(),-1,&rc,DT_VCENTER|DT_SINGLELINE);
                if(foc){
                    SIZE ts;std::string bc=inp.text.substr(0,(std::min)(inp.cursor,(int)inp.text.length()));
                    GetTextExtentPoint32A(dc,bc.c_str(),(int)bc.length(),&ts);
                    HPEN cp=CreatePen(PS_SOLID,1,RGB(255,255,255));auto ocp=SelectObject(dc,cp);
                    MoveToEx(dc,ix+6+ts.cx,iy+4,0);LineTo(dc,ix+6+ts.cx,iy+ih-4);
                    SelectObject(dc,ocp);DeleteObject(cp);
                }
            } else if(!inp.placeholder.empty()){
                SetTextColor(dc,RGB(120,120,140));
                RECT rc={ix+6,iy+2,ix+iw-4,iy+ih-2};
                DrawTextA(dc,inp.placeholder.c_str(),-1,&rc,DT_VCENTER|DT_SINGLELINE);
            }
            SelectObject(dc,of);DeleteObject(f);
            if(pH){Hit h;h.r={ix,iy,ix+iw,iy+ih};h.elemId=id;h.isInput=true;pH->push_back(h);}
            curY=y+ih+8;break;}
        case HTP::EType::CANVAS:{
            std::string id=e->props.get("id","cv_"+std::to_string(y));
            int cw=e->props.getInt("width",400),ch=e->props.getInt("height",200);
            auto bdr=e->props.getColor("border-color",{80,80,100});
            auto cb=Canvas::get(id,refDC,cw,ch);
            int cx=x,cy=y-scrollY;
            HPEN p=CreatePen(PS_SOLID,1,bdr.cr());auto op=SelectObject(dc,p);
            auto nb=(HBRUSH)GetStockObject(NULL_BRUSH);auto ob=SelectObject(dc,nb);
            Rectangle(dc,cx-1,cy-1,cx+cw+1,cy+ch+1);
            SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(p);
            if(cb->dc) BitBlt(dc,cx,cy,cw,ch,cb->dc,0,0,SRCCOPY);
            curY=y+ch+8;break;}
        case HTP::EType::DIVIDER:{
            auto col=e->props.getColor("color",{60,60,80});
            int t=e->props.getInt("thickness",1),m=e->props.getInt("margin",10);
            HPEN p=CreatePen(PS_SOLID,t,col.cr());auto op=SelectObject(dc,p);
            int dy=y+m-scrollY;MoveToEx(dc,x,dy,0);LineTo(dc,x+mw,dy);
            SelectObject(dc,op);DeleteObject(p);curY=y+m*2+t;break;}
        case HTP::EType::LIST:{
            int cy=y;for(auto&c:e->children){draw(dc,c,x+20,cy,mw-20);cy=curY;}break;}
        case HTP::EType::ITEM:{
            std::string ct=e->props.get("content","");int sz=e->props.getInt("size",14);
            auto col=e->props.getColor("color",{200,200,200});
            int by=y-scrollY+sz/2-2;
            HBRUSH b=CreateSolidBrush(col.cr());HPEN pn=CreatePen(PS_SOLID,1,col.cr());
            auto ob=SelectObject(dc,b);auto op=SelectObject(dc,pn);
            Ellipse(dc,x-12,by,x-4,by+6);
            SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(pn);
            HFONT f=mkFont(sz);auto of=SelectObject(dc,f);
            SetTextColor(dc,col.cr());SetBkMode(dc,TRANSPARENT);
            RECT rc={x,y-scrollY,x+mw,y-scrollY+200};
            DrawTextA(dc,ct.c_str(),-1,&rc,DT_WORDBREAK|DT_CALCRECT);
            int th=rc.bottom-rc.top;rc.bottom=rc.top+th;
            DrawTextA(dc,ct.c_str(),-1,&rc,DT_WORDBREAK);
            SelectObject(dc,of);DeleteObject(f);curY=y+th+4;break;}
        case HTP::EType::BR: curY=y+e->props.getInt("size",16); break;
        case HTP::EType::IMAGE:{
            int iw=e->props.getInt("width",100),ih=e->props.getInt("height",100);
            std::string alt=e->props.get("alt","[image]");
            auto bdr=e->props.getColor("border-color",{80,80,100});
            int ix=x,iy=y-scrollY;
            HBRUSH b=CreateSolidBrush(RGB(50,50,70));HPEN p=CreatePen(PS_SOLID,1,bdr.cr());
            auto ob=SelectObject(dc,b);auto op=SelectObject(dc,p);
            Rectangle(dc,ix,iy,ix+iw,iy+ih);
            SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);
            HFONT f=mkFont(12);auto of=SelectObject(dc,f);
            SetTextColor(dc,RGB(150,150,170));SetBkMode(dc,TRANSPARENT);
            RECT rc={ix+4,iy+4,ix+iw-4,iy+ih-4};
            DrawTextA(dc,alt.c_str(),-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            SelectObject(dc,of);DeleteObject(f);curY=y+ih+8;break;}
        default:break;
        }
        if(curY>contentH+scrollY) contentH=curY-scrollY;
    }

public:
    int totalH()const{return contentH;}
    void setScroll(int y){scrollY=(std::max)(0,y);}
    int getScroll()const{return scrollY;}
    std::vector<Hit> hits;
    void render(HDC tdc,int w,int h,HTP::Doc&doc){
        contentH=0;curY=10;hits.clear();pH=&hits;refDC=tdc;
        HDC m=CreateCompatibleDC(tdc);HBITMAP bm=CreateCompatibleBitmap(tdc,w,h);
        auto ob=SelectObject(m,bm);
        HBRUSH bg=CreateSolidBrush(doc.bg.cr());
        RECT r={0,0,w,h};FillRect(m,&r,bg);DeleteObject(bg);
        draw(m,doc.root,10,10,w-20);
        BitBlt(tdc,0,0,w,h,m,0,0,SRCCOPY);
        SelectObject(m,ob);DeleteObject(bm);DeleteDC(m);
    }
};
}

// PAGES
namespace Pages {
std::string readF(const std::string&p){std::ifstream f(p);if(!f)return"";std::stringstream s;s<<f.rdbuf();return s.str();}
std::string home(){
    auto c=readF("home.htp");if(!c.empty())return c;
    std::string s;
    s+="@page{title:\" Lume\";background:#0f0f23;}\n";
    s+="@block{align:center;padding:40;background:#1a1a3e;border-radius:12;margin:20;\n";
    s+="  @text{content:\"Lume\";size:42;color:#e94560;bold:true;}\n";
    s+="  @br{size:10;}\n";
    s+="  @text{content:\"Lua + Canvas + Input + Layout\";size:18;color:#8888aa;italic:true;}\n";
    s+="  @br{size:20;}\n";
    s+="  @text{content:\"Enter htp:// or open .htp file\";size:16;color:#666688;}\n";
    s+="}\n";
    s+="@block{padding:20;margin:20;background:#16213e;border-radius:8;\n";
    s+="  @text{content:\"Quick links:\";size:18;color:#ffffff;bold:true;}\n";
    s+="  @br{size:10;}\n";
    s+="  @link{content:\"Server localhost:8080\";url:\"htp://localhost:8080/index.htp\";color:#0abde3;size:16;}\n";
    s+="  @link{content:\"About\";url:\"about:info\";color:#0abde3;size:16;}\n";
    s+="}\n";
    return s;
}
std::string about(){
    auto c=readF("about_browser.htp");if(!c.empty())return c;
    std::string s;
    s+="@page{title:\"About\";background:#0f0f23;}\n";
    s+="@block{align:center;padding:30;background:#1a1a3e;margin:20;border-radius:10;\n";
    s+="  @text{content:\"Lume\";size:36;color:#e94560;bold:true;}\n";
    s+="}\n";
    s+="@block{padding:20;margin:20;background:#16213e;border-radius:8;\n";
    s+="  @list{\n";
    s+="    @item{content:\"Lua 5.4 scripting\";size:15;color:#ccccee;}\n";
    s+="    @item{content:\"Canvas drawing API\";size:15;color:#ccccee;}\n";
    s+="    @item{content:\"Live text input\";size:15;color:#ccccee;}\n";
    s+="    @item{content:\"Row/Column layout\";size:15;color:#ccccee;}\n";
    s+="  }\n";
    s+="  @br{size:15;}\n";
    s+="  @link{content:\"Home\";url:\"about:home\";color:#0abde3;size:16;}\n";
    s+="}\n";
    return s;
}
std::string error(const std::string&e,const std::string&u){
    std::string s;
    s+="@page{title:\"Error\";background:#1a0000;}\n";
    s+="@block{align:center;padding:40;margin:30;background:#2a1010;border-radius:10;\n";
    s+="  @text{content:\"Error\";size:32;color:#ff4444;bold:true;}\n";
    s+="  @br{size:15;}\n";
    s+="  @text{content:\""+e+"\";size:16;color:#ff8888;}\n";
    s+="  @text{content:\""+u+"\";size:14;color:#aa6666;}\n";
    s+="  @br{size:20;}\n";
    s+="  @link{content:\"Home\";url:\"about:home\";color:#0abde3;size:16;}\n";
    s+="}\n";
    return s;
}
}

// BROWSER
HTP::Doc g_doc;
Render::Engine g_ren;
std::string g_curUrl;
std::vector<std::string> g_hist;
int g_histPos=-1;

void loadContent(const std::string&content,const std::string&url){
    Script::reset();
    HTP::Parser p; g_doc=p.parse(content);
    g_curUrl=url; g_ren.setScroll(0);

    std::function<void(std::shared_ptr<HTP::Elem>)> run;
    run=[&](std::shared_ptr<HTP::Elem>e){
        if(!e)return;
        if(e->type==HTP::EType::SCRIPT&&!e->scriptCode.empty()){
            Script::exec(e->scriptCode);
        }
        for(auto&c:e->children) run(c);
    };
    run(g_doc.root);

    SetWindowTextA(g_mainWnd,(g_doc.title+" - Lume").c_str());
    SetWindowTextA(g_addressBar,url.c_str());
    invalidateContent();
}

void loadFile(const std::string&fp){
    std::string path=fp;
    if(path.length()>7&&path.substr(0,7)=="file://") path=path.substr(7);
    std::ifstream f(path);
    if(!f){loadContent(Pages::error("Not found: "+path,"file://"+path),"file://"+path);return;}
    std::stringstream ss;ss<<f.rdbuf();f.close();
    loadContent(ss.str(),"file://"+path);
    setStatus("Loaded: "+path);
}

void navigateTo(const std::string&url){
    std::string u=url;
    if(u=="about:home"||u=="about:blank"||u.empty()){
        g_histPos++;if(g_histPos<(int)g_hist.size())g_hist.resize(g_histPos);
        g_hist.push_back("about:home");loadContent(Pages::home(),"about:home");setStatus("Ready");return;
    }
    if(u=="about:info"){
        g_histPos++;if(g_histPos<(int)g_hist.size())g_hist.resize(g_histPos);
        g_hist.push_back(u);loadContent(Pages::about(),"about:info");setStatus("Ready");return;
    }
    if(u.length()>7&&u.substr(0,7)=="file://"){
        g_histPos++;if(g_histPos<(int)g_hist.size())g_hist.resize(g_histPos);
        g_hist.push_back(u);loadFile(u);return;
    }
    if(u.find("://")==std::string::npos){
        if(u.length()>4&&u.substr(u.length()-4)==".htp"){
            std::ifstream t(u);if(t){t.close();
                g_histPos++;if(g_histPos<(int)g_hist.size())g_hist.resize(g_histPos);
                g_hist.push_back("file://"+u);loadFile(u);return;
            }
        }
        u="htp://"+u;
    }
    if(u.length()>6&&u.substr(0,6)=="htp://"){
        setStatus("Loading "+u+"...");
        auto pu=Net::URL::parse(u);auto r=Net::fetch(pu);
        g_histPos++;if(g_histPos<(int)g_hist.size())g_hist.resize(g_histPos);
        g_hist.push_back(u);
        if(r.ok&&!r.body.empty()){loadContent(r.body,u);setStatus("OK: "+pu.host+pu.path);}
        else{loadContent(Pages::error(r.error,u),u);setStatus("Err: "+r.error);}
        return;
    }
    loadContent(Pages::error("Unknown protocol",u),u);
}

void histNav(const std::string&u){
    if(u=="about:home")loadContent(Pages::home(),"about:home");
    else if(u=="about:info")loadContent(Pages::about(),"about:info");
    else if(u.length()>7&&u.substr(0,7)=="file://")loadFile(u);
    else{auto r=Net::fetch(Net::URL::parse(u));
        if(r.ok)loadContent(r.body,u);else loadContent(Pages::error(r.error,u),u);}
}
void goBack(){if(g_histPos>0){g_histPos--;histNav(g_hist[g_histPos]);}}
void goFwd(){if(g_histPos<(int)g_hist.size()-1){g_histPos++;histNav(g_hist[g_histPos]);}}

WNDPROC g_origAddr=0;
LRESULT CALLBACK AddrProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_KEYDOWN&&w==VK_RETURN){char b[2048]={};GetWindowTextA(h,b,2048);navigateTo(b);return 0;}
    if(m==WM_CHAR&&w==VK_RETURN) return 0;
    return CallWindowProcA(g_origAddr,h,m,w,l);
}

LRESULT CALLBACK WndProc(HWND hw,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        Script::g_hwnd=hw;
        CreateWindowA("BUTTON","<",WS_CHILD|WS_VISIBLE,4,6,30,28,hw,(HMENU)ID_BACK,0,0);
        CreateWindowA("BUTTON",">",WS_CHILD|WS_VISIBLE,38,6,30,28,hw,(HMENU)ID_FWD,0,0);
        CreateWindowA("BUTTON","R",WS_CHILD|WS_VISIBLE,72,6,30,28,hw,(HMENU)ID_REF,0,0);
        g_addressBar=CreateWindowA("EDIT","about:home",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            108,8,500,24,hw,(HMENU)ID_ADDR,0,0);
        CreateWindowA("BUTTON","Go",WS_CHILD|WS_VISIBLE,614,6,40,28,hw,(HMENU)ID_GO,0,0);
        g_statusBar=CreateWindowA("STATIC","Ready",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,800,STATUS_H,hw,0,0,0);
        HFONT uf=CreateFontA(-14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
        SendMessage(g_addressBar,WM_SETFONT,(WPARAM)uf,1);
        SendMessage(g_statusBar,WM_SETFONT,(WPARAM)uf,1);
        g_origAddr=(WNDPROC)SetWindowLongPtrA(g_addressBar,GWLP_WNDPROC,(LONG_PTR)AddrProc);
        navigateTo("about:home");
        return 0;}
    case WM_SIZE:{
        int w=LOWORD(lp),h=HIWORD(lp);
        int aw=w-108-50-10;if(aw<100)aw=100;
        MoveWindow(g_addressBar,108,8,aw,24,TRUE);
        HWND gb=GetDlgItem(hw,ID_GO);if(gb)MoveWindow(gb,108+aw+4,6,40,28,TRUE);
        MoveWindow(g_statusBar,0,h-STATUS_H,w,STATUS_H,TRUE);
        invalidateContent();
        return 0;}
    case WM_PAINT:{
        PAINTSTRUCT ps;HDC hdc=BeginPaint(hw,&ps);
        RECT cr;GetClientRect(hw,&cr);
        int w=cr.right,h=cr.bottom-TOOLBAR_H-STATUS_H;if(h<1)h=1;
        if(ps.rcPaint.top < TOOLBAR_H){
            HBRUSH tb=CreateSolidBrush(RGB(30,30,50));
            RECT tr={0,0,w,TOOLBAR_H};FillRect(hdc,&tr,tb);DeleteObject(tb);
        }
        HDC cd=CreateCompatibleDC(hdc);HBITMAP cb=CreateCompatibleBitmap(hdc,w,h);
        auto ob=SelectObject(cd,cb);
        g_ren.render(cd,w,h,g_doc);
        BitBlt(hdc,0,TOOLBAR_H,w,h,cd,0,0,SRCCOPY);
        SelectObject(cd,ob);DeleteObject(cb);DeleteDC(cd);
        EndPaint(hw,&ps);return 0;}
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEWHEEL:{
        int d=GET_WHEEL_DELTA_WPARAM(wp);int s=g_ren.getScroll()-d/2;
        if(s<0)s=0;int mx=g_ren.totalH()-400;if(mx<0)mx=0;if(s>mx)s=mx;
        g_ren.setScroll(s);invalidateContent();return 0;}
    case WM_LBUTTONDOWN:{
        int mx=LOWORD(lp),my=HIWORD(lp)-TOOLBAR_H;
        bool ci=false;
        for(auto&h:g_ren.hits){
            if(mx>=h.r.left&&mx<=h.r.right&&my>=h.r.top&&my<=h.r.bottom){
                if(h.isInput){
                    Script::g_focusId=h.elemId;
                    Script::g_inputs[h.elemId].focused=true;
                    Script::g_inputs[h.elemId].cursor=(int)Script::g_inputs[h.elemId].text.length();
                    ci=true;invalidateContent();break;
                }
                if(h.isBtn&&!h.elemId.empty()){
                    Script::fireClick(h.elemId);
                    invalidateContent();
                    if(h.url.empty()) return 0;
                }
                if(!h.url.empty()){navigateTo(h.url);return 0;}
            }
        }
        if(!ci){Script::g_focusId.clear();for(auto&kv:Script::g_inputs)kv.second.focused=false;invalidateContent();}
        SetFocus(hw);return 0;}
    case WM_CHAR:{
        if(!Script::g_focusId.empty()&&GetFocus()==hw){
            auto&inp=Script::g_inputs[Script::g_focusId];
            char c=(char)wp;
            if(c=='\b'){if(inp.cursor>0&&!inp.text.empty()){inp.text.erase(inp.cursor-1,1);inp.cursor--;}}
            else if(c=='\r'||c=='\n'){inp.focused=false;Script::g_focusId.clear();}
            else if(c>=32){inp.text.insert(inp.cursor,1,c);inp.cursor++;}
            invalidateContent();return 0;
        }break;}
    case WM_KEYDOWN:{
        if(!Script::g_focusId.empty()&&GetFocus()==hw){
            auto&inp=Script::g_inputs[Script::g_focusId];
            if(wp==VK_LEFT){if(inp.cursor>0)inp.cursor--;invalidateContent();return 0;}
            if(wp==VK_RIGHT){if(inp.cursor<(int)inp.text.length())inp.cursor++;invalidateContent();return 0;}
            if(wp==VK_HOME){inp.cursor=0;invalidateContent();return 0;}
            if(wp==VK_END){inp.cursor=(int)inp.text.length();invalidateContent();return 0;}
            if(wp==VK_DELETE){if(inp.cursor<(int)inp.text.length()){inp.text.erase(inp.cursor,1);invalidateContent();}return 0;}
            if(wp=='V'&&(GetKeyState(VK_CONTROL)&0x8000)){
                if(OpenClipboard(hw)){HANDLE h=GetClipboardData(CF_TEXT);if(h){
                    const char*cl=(const char*)GlobalLock(h);if(cl){
                        std::string p=cl;p.erase(std::remove(p.begin(),p.end(),'\r'),p.end());
                        p.erase(std::remove(p.begin(),p.end(),'\n'),p.end());
                        inp.text.insert(inp.cursor,p);inp.cursor+=(int)p.length();
                        GlobalUnlock(h);}
                }CloseClipboard();}invalidateContent();return 0;
            }
        }
        if(wp==VK_F5)navigateTo(g_curUrl);
        else if(wp==VK_BACK&&Script::g_focusId.empty()&&GetFocus()!=g_addressBar)goBack();
        return 0;}
    case WM_SETCURSOR:{
        if(LOWORD(lp)==HTCLIENT){
            POINT pt;GetCursorPos(&pt);ScreenToClient(hw,&pt);pt.y-=TOOLBAR_H;
            bool ov=false;
            for(auto&h:g_ren.hits){
                if(pt.x>=h.r.left&&pt.x<=h.r.right&&pt.y>=h.r.top&&pt.y<=h.r.bottom){
                    if(!h.url.empty()||h.isBtn){ov=true;break;}
                    if(h.isInput){SetCursor(LoadCursor(0,IDC_IBEAM));return TRUE;}
                }
            }
            SetCursor(LoadCursor(0,ov?IDC_HAND:IDC_ARROW));return TRUE;
        }break;}
    case WM_COMMAND:{
        int id=LOWORD(wp);
        if(id==ID_GO){char b[2048]={};GetWindowTextA(g_addressBar,b,2048);navigateTo(b);}
        else if(id==ID_BACK)goBack();else if(id==ID_FWD)goFwd();
        else if(id==ID_REF)navigateTo(g_curUrl);
        return 0;}
    case WM_DROPFILES:{
        HDROP hd=(HDROP)wp;char fp[MAX_PATH]={};DragQueryFileA(hd,0,fp,MAX_PATH);DragFinish(hd);
        std::string p=fp;if(p.length()>4&&p.substr(p.length()-4)==".htp")loadFile(p);return 0;}
    case WM_TIMER: return 0;
    case WM_DESTROY: Script::reset();PostQuitMessage(0);return 0;
    }
    return DefWindowProcA(hw,msg,wp,lp);
}

int WINAPI WinMain(HINSTANCE hI,HINSTANCE,LPSTR cmd,int show){
    WSADATA wd;WSAStartup(MAKEWORD(2,2),&wd);
    INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_STANDARD_CLASSES};InitCommonControlsEx(&ic);
    WNDCLASSEXA wc={};wc.cbSize=sizeof(wc);wc.style=CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc=WndProc;wc.hInstance=hI;wc.hCursor=LoadCursor(0,IDC_ARROW);
    wc.hbrBackground=0;
    wc.lpszClassName="LumeClass";wc.hIcon=LoadIcon(0,IDI_APPLICATION);
    RegisterClassExA(&wc);
    g_mainWnd=CreateWindowExA(WS_EX_ACCEPTFILES,"LumeClass","Lume",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1024,768,0,0,hI,0);
    if(!g_mainWnd)return 1;
    ShowWindow(g_mainWnd,show);UpdateWindow(g_mainWnd);
    if(cmd&&strlen(cmd)>0){
        std::string a=cmd;
        if(!a.empty()&&a.front()=='"')a.erase(a.begin());
        if(!a.empty()&&a.back()=='"')a.pop_back();
        if(!a.empty())navigateTo(a);
    }
    MSG msg;
    while(GetMessage(&msg,0,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
    WSACleanup();return(int)msg.wParam;
}
