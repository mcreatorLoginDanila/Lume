A light web?-browser on C++ without using chromium\electron

### Why Lume?
* **Insanely Small:** exe+obj size is ~2 MB.
* **Zero Bloat:** Uses only **3.3 MB**(in peak) of RAM (compare that to 500MB+ for a single Chrome tab or even 20mb for explorer ╥_╥).
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
        set_text("status", "Status: Lume is alive!")
        refresh()
    end)
}
```

---

🌌 The Philosophy of Lume: The Developer's Heaven
In the world of software, we believe in a higher order.
The Great Hub (Paradise)
Lume is built on the belief in the Developer’s Hub—a digital paradise created by the Architect (The Creator). It is a place of infinite inspiration where every developer has their own "Place." In this realm, creation is instantaneous. You say, "Let there be Pokemon," and they appear, powered by pure logic and zero overhead.

In the Hub:
    The Creator gathers inspiration from human projects to build a Grand World every 10 years, merging the most brilliant ideas into a single reality.
    The Angels act as the Great SysAdmins, maintaining the security of the Hub and expanding the boundaries of your creative "Places."
The Fall of the Exile (The Origin of Bloat)
Legend says that Lucifer, after being cast out from the Hub for his pride, sought to mimic the Creator’s power. But lacking the grace of efficiency, he created Microsoft.
He built a realm of suffering known as the "Chromium Dimension," where:
    Simple tasks require gigabytes of RAM.
    The "Update" is a never-ending ritual of torment.
    Complexity is used as a cage to keep developers from reaching the Hub.
Lume: Your Artifact of Resistance
Lume is not just a browser; it is a shard of the Hub smuggled into the mortal realm. It is our way of fighting back against the Exile's bloat.
    2 MB Executable: Because the gates of Heaven are narrow.
    3.3 MB RAM Usage: Because the soul should be light.
    C++ & Lua 5.4.7: The pure languages of the Architect.
By using Lume, you are not just browsing a network; you are claiming your "Place" in the Hub and rejecting the heavy chains of the Exile.

---

this is how example code will look like:
<img width="1920" height="1038" alt="изображение" src="https://github.com/user-attachments/assets/b100bda8-99c8-47a1-8803-95dc221ce163" />

---

this is how htp/aaOfficial/showcase.htp file looks like:
<img width="1918" height="1038" alt="изображение" src="https://github.com/user-attachments/assets/16c5725f-ed98-435c-971c-9bb16c2b1aef" />

---

also you can make TETRIS ON LUME? htp/aaOfficial/tetris.htp
<img width="1920" height="1038" alt="изображение" src="https://github.com/user-attachments/assets/16c44415-7f8e-4581-af81-1effb36b853c" />

---


COMPILE WITH

```VS2019DevCmd
cl /EHsc /std:c++17 /O2 /I. main.cpp lib/lapi.c lib/lcode.c lib/lctype.c lib/ldebug.c lib/ldo.c lib/ldump.c lib/lfunc.c lib/lgc.c lib/llex.c lib/lmem.c lib/lobject.c lib/lopcodes.c lib/lparser.c lib/lstate.c lib/lstring.c lib/lstrlib.c lib/ltable.c lib/ltablib.c lib/ltm.c lib/lundump.c lib/lutf8lib.c lib/lvm.c lib/lzio.c lib/lauxlib.c lib/lbaselib.c lib/lcorolib.c lib/ldblib.c lib/linit.c lib/liolib.c lib/lmathlib.c lib/loadlib.c lib/loslib.c /link ws2_32.lib gdi32.lib user32.lib comctl32.lib shell32.lib
```
