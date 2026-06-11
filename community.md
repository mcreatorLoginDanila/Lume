https://github.com/Lume-corp/LumeSources

### PLUGINS:
- **Verified Community Plugins:** Located in `plugins/Community/`. These dynamic libraries (DLLs) have been manually audited and approved by the Lume author to ensure engine stability and absence of malicious payloads.
- **Unsafe & Unverified Plugins:** Located in `plugins/CommunityUnsafe/`. Contains unreviewed experimental plugins or extensions that explicitly require elevated OS privileges and perform unsafe operations (e.g., direct local filesystem access, arbitrary memory manipulation).
- **Directory Structure Standard:** All community plugin submissions must strictly adhere to the following pathing format: `[Category]/[Author]/[LumeVersion]/` (e.g., `CommunityUnsafe/NotAndrey/V0.6.1/`). Submissions ignoring this structure will be rejected.

### CORE CONTENT:
- **Main Lume Hub (Syte):** Located in `syte/`. This directory serves as the primary entry point and central content hub for the Lume engine. It is strictly reserved for hosting major materials, including links to executable games, alternative browser builds, native Lua-based games, and technical articles.

### SCRIPTS & MARKUP:
- **Lua Libraries & Micro-Engines:** Located in `lua/`. Dedicated to standalone Lua scripts, reusable libraries, and custom logic engines (e.g., metadata parsers designed for procedural level generation). 
- **HTP Examples & Tests:** Located in `htp/`. A repository for third-party HyperText Page (`.htp`) examples, custom UI layout tests, and standalone rendering demonstrations intended for community learning and debugging.
