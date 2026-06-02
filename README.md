A light ~~web~~-browser on C++ ~~without using chromium\electron~~

### Why Lume?
* **Insanely Small:** exe size is less than 1 MB.
* **Zero Bloat:** Uses only **3 MB**(Idle) of RAM (compare that to 500MB+ for a single Chrome tab or even 20mb for explorer ╥_╥).
* **Blazing Fast:** CPU usage is near **0%**(On Phenom II X6 1055T) in idle and ~10% under load(like in "doom" on 0.3).
* **Programmable:** Built-in **Lua 5.4.7** engine for logic, animations, and interactivity.
* **Custom Graphics:** Built-in `@canvas` API for high-performance 2D drawing. (also starting from beta 0.3 added OpenGL(opengl32.dll))

---

All pages\plugins has been moved to https://github.com/Lume-corp/LumeSources

content of /lib/ folder: https://github.com/Lume-corp/LumeLibs

---

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

this is how example code will look like:
<img width="1920" height="1038" alt="изображение" src="https://github.com/user-attachments/assets/b100bda8-99c8-47a1-8803-95dc221ce163" />

---

<details>
<summary>The Philosophy of Lume</summary>
will be remaked
</details>

---

this is how htp/aaOfficial/showcase.htp file looks like:
<img width="1918" height="1038" alt="изображение" src="https://github.com/user-attachments/assets/16c5725f-ed98-435c-971c-9bb16c2b1aef" />

---

also you can make TETRIS ON LUME? htp/aaOfficial/tetris.htp
<img width="1920" height="1038" alt="изображение" src="https://github.com/user-attachments/assets/16c44415-7f8e-4581-af81-1effb36b853c" />

---


COMPILE WITH
```VS2019DevCmd
cl /EHsc /std:c++17 /O2 /utf-8 /I. main.cpp lib/*.c lib/wasm3/*.c /link gdi32.lib user32.lib comctl32.lib shell32.lib winhttp.lib ole32.lib
```

added gradient text on v3.1
<img width="347" height="67" alt="изображение" src="https://github.com/user-attachments/assets/31412967-4053-419b-a09f-669e76c4c921" />


now supports https (0.3.4-0.3.4.1) and opengl (0.3-0.3.3)
<img width="1918" height="1040" alt="image" src="https://github.com/user-attachments/assets/b5448290-b705-48e1-8e3e-38bf872b584e" />
