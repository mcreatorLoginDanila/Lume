A light web?-browser on C++ without using chromium\electron

### Why Lume?
* **Insanely Small:** exe+obj size is ~2 MB.
* **Zero Bloat:** Uses only **3.3 MB**(in peak) of RAM (compare that to 500MB+ for a single Chrome tab).
* **Blazing Fast:** CPU usage is near **0%**(On Phenom II X6 1055T) in idle and ~0.3%(maybe) under load.
* **Programmable:** Built-in **Lua 5.4.7** engine for logic, animations, and interactivity.
* **Custom Graphics:** Built-in `@canvas` API for high-performance 2D drawing.

example code:
```htp
@page { 
    title: "Lume Site"; 
    background: #0a0a1a; 
}
@row {
    margin: 20;
    gap: 15;
    @column {
        padding: 20;
        background: #1a1a3e;
        border-radius: 10;
        @text { content: "Hello from Lume!"; size: 24; color: #e94560; bold: true; }
        @br { size: 10; }
        @text(id="status") { content: "System: Ready"; size: 16; color: #88ff88; }
        @br { size: 15; }
        @button(id="btn") { content: "Click Me"; width: 120; height: 35; background: #0abde3; color: #ffffff; }
    }
}
@script {
    on_click("btn", function()
        set_text("status", "Status: Lume is alive! :}")
        refresh()
    end)
}
```

this is how htp/showcase.htp file looks like:
<img width="1918" height="1038" alt="изображение" src="https://github.com/user-attachments/assets/0ffbb2be-b436-40cd-81c2-49773208c8c3" />


---


COMPILE WITH

cl /EHsc /std:c++17 /O2 /I. main.cpp lib/lapi.c lib/lcode.c lib/lctype.c lib/ldebug.c lib/ldo.c lib/ldump.c lib/lfunc.c lib/lgc.c lib/llex.c lib/lmem.c lib/lobject.c lib/lopcodes.c lib/lparser.c lib/lstate.c lib/lstring.c lib/lstrlib.c lib/ltable.c lib/ltablib.c lib/ltm.c lib/lundump.c lib/lutf8lib.c lib/lvm.c lib/lzio.c lib/lauxlib.c lib/lbaselib.c lib/lcorolib.c lib/ldblib.c lib/linit.c lib/liolib.c lib/lmathlib.c lib/loadlib.c lib/loslib.c /link ws2_32.lib gdi32.lib user32.lib comctl32.lib shell32.lib
