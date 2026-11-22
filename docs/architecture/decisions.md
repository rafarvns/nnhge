# Architecture decisions

Each decision has a short rationale. Change a decision by editing its row and adding a dated note below
the table; never silently. New decisions get the next number.

| # | Decision | Why |
|---|----------|-----|
| D1 | C++20 subset, no exceptions/RTTI in engine | Smaller binaries, predictable cost on the 268 MHz ARM11; still modern syntax. Same choice as Butano. See [../tech/cpp.md](../tech/cpp.md). |
| D2 | CMake + Ninja, presets, devkitPro's `3DS.cmake` toolchain | One build system for host and console, fast incremental builds, IDE support. See [../tech/cmake.md](../tech/cmake.md). |
| D3 | PC simulator is a host build on SDL3, not an emulator | Instant iteration and native debugging. Azahar and hardware stay as accuracy checks. |
| D4 | Simulator enforces 3DS limits (screen sizes, memory budget, texture rules) | Avoid "works on PC, breaks on console". |
| D5 | Rendering: 2D first on citro2d/citro3d; stereoscopic 3D later on citro3d | citro* are the maintained devkitPro GPU libraries; 2D gets games running sooner. |
| D6 | Assets compiled ahead of time into platform formats, packed in romfs (spec 007: per-file CMake build steps, incremental by timestamps; the C++ `assetc` with a content-hash cache waits for heavier conversions such as audio) | No decoding cost on the console; only changed assets rebuild. |
| D7 | Console builds always happen on the PC | The 3DS cannot compile in useful time. NNH Link *requests* builds and runs results. |
| D8 | NNH Link starts compatible with 3dslink, then adds its own protocol | Reuse a proven path (Homebrew Launcher netloader, stdio redirect on port 17491) before inventing. |
| D9 | `.3dsx` for the dev loop, `.cia` for installation | `.3dsx` is fastest to send and launch; `.cia` is the native installable artifact. |
| D10 | Tooling (`nnh` CLI, `assetc`) in C++ | One language for contributors, single static binary. Revisit if it slows us down. |
| D11 | **Event-driven: typed Event Bus (publish/subscribe) + Scenes (State pattern)** | Simple mental model, decoupled modules (OCP: add behaviour by subscribing), cheap on ARM11 when the bus is fixed-capacity and RTTI-free. ECS was considered and rejected for now: more abstract, harder for newcomers; can be added later as a module. See [events.md](events.md), [scenes.md](scenes.md). |
| D12 | **Spec-Driven Development + TDD** for every feature | Specs make intent reviewable before code and give Claude Code a precise contract; tests first keep core logic decoupled from hardware. See [../process/](../process/). |
| D13 | **Small files, one concept per file, module folders** | Easy to navigate for humans and AI tools, small diffs, fast incremental builds. See [project-structure.md](project-structure.md). |

| D14 | **Repository docs are the source of truth; the GitHub wiki is generated** (`scripts/docs_tool.py`, published by CI) | Docs are reviewed with the code in the same commit; the wiki gives readers navigation without a second copy that drifts. See [../process/documentation.md](../process/documentation.md). |
| D15 | **Portable core + one static library per system**, plugged in at link time through a documented platform contract, with conformance tests | Adding a system never touches core; no virtual cost on ARM11; consoles can't load libraries at runtime anyway. A runtime registry with virtual interfaces was rejected. See [platforms.md](platforms.md). |
| D16 | **System-only features via capability queries + extension headers** `<nnh/<system>/...>` | Games stay portable by default, unique hardware (stereo 3D, Joy-Con) stays reachable, and non-portable code is explicit at the include line. |
| D17 | **Core foundation before features**: time, logging, asserts/`Result`/crash report, memory accounting, debug overlay/profiler, confined `fs`; three build flavors (Debug, Development, Release) | Every later module needs them; a console without a debugger needs logs and crash reports from day one. See [foundation.md](foundation.md). |
| D18 | **Safe by default** (constitution X): confined file writes, atomic saves, fuzzed parsers, pinned supply chain, no telemetry | Homebrew has no sandbox; players' SD cards and saves must be safe. See [security.md](security.md). |
| D19 | **`IMAGES`: images embedded in the executable** (tex3ds `.t3x` on 3DS, PNG on PC), loaded by name — kept for small games; since spec 007 `ASSETS` (romfs, on demand) is the recommended way and `load_texture` looks there first | Games got graphics before the asset pipeline existed; the same `load_texture` call works for both. |
| D20 | **Rendering is recorded, then submitted per screen in passes; one texture type for 2D and 3D** (3DS: `C3D_Tex` drawn by citro2d; PC: SDL renderer on an `SDL_GPUDevice`, default renderer as headless fallback) | Layers, batching and stereo (one list, two eyes) come for free, and 3D (M8) slots in as a pass without changing 2D calls or textures. See [rendering.md](rendering.md). |
| D21 | **Audio: the platform plays voices, core owns the rules** (spec 008). The contract is samples + voices (gain per speaker, rate, loop, pause); priorities, groups, pan, pitch, fades, loop points and Ogg decoding (vendored stb_vorbis) are core logic. Without output, core runs the same rules silently, ending voices by the clock | The 3DS DSP mixes 24 channels in hardware for free; mixing in core would cost the Old 3DS CPU. Rules in core are the same everywhere and unit-tested on the fake platform. See [api/audio.md](../api/audio.md). |
| D22 | **Packaging is a build step around devkitPro's** (spec 009): one identity in `nnh_add_game()` (title, author, version, icon) feeds the `.3dsx` SMDH, the `.cia` and the PC window; the `.cia` is made by makerom from the same ELF, SMDH and assets, with our own RSF and a bannertool banner; the tools are pinned prebuilt releases fetched by the first `.cia` build | A contributor needs nothing new until a game asks for a `.cia`, and the game behaves the same in both formats. See [guides/packaging.md](../guides/packaging.md). |
| D23 | **`nnh` speaks the netloader and FBI protocols itself** (spec 010): its own small C++ code sends `.3dsx` files to the Homebrew Launcher (zlib stored blocks, chunks of at most 16 KB, argv[0] = `sdmc:/3ds/<name>`), receives the game's log on TCP 17491 and serves `.cia` files to FBI; discovery asks every local network's broadcast | No dependency on devkitPro's `3dslink` or Python, works on Linux and in CI, and NNH Link (spec 011) reuses the same code. D8 still holds: it is compatible with 3dslink. See [guides/nnh-tool.md](../guides/nnh-tool.md). |
| D24 | **One link library, mutual authentication, no encryption** (spec 011): `link/` (`nnh_link`) is built for PC and 3DS and holds sockets, SHA-256/HMAC, the message codec, pairing and the sessions, so `nnh serve` and NNH Link run the same code. A 6-digit code pairs a console with a PC once; every connection then proves both sides with HMAC (the server first) and every message carries a MAC and a sequence number; nothing is encrypted. Launching uses Luma3DS's `hb:ldr`, like the Homebrew Launcher | Our own small code (no third-party crypto), tested with published vectors and fuzzed. A console runs programs only from the PC it paired with, a PC builds only for paired consoles. Known limit: someone recording the network *while pairing* could brute-force the code offline: pair on a network you trust. See [guides/nnh-link.md](../guides/nnh-link.md). |
| D25 | **Game features are opt-in libraries over the core, code first** (constitution XI): tilemaps, collision, tags, data assets, prefabs, animation, UI and the rest of M7 are separate modules a game may use, skip or replace; none needs another the game didn't choose; every one works through the C++ API, and files (hand-written text, Tiled, Aseprite) are converted at build time | Godot/Unity productivity without their lock-in: NNHGE has no editor, and a programmer who prefers their own entities, maps or data loses nothing and pays nothing for the features they skip. |
| D26 | **tile layers are drawn as tile batches**: one draw-list command per (map layer, tileset), sorted with sprites by layer; at submit core expands only the visible cells into 12-byte quads and calls one contract function, `draw_tiles`, per batch (between `draw_2d` calls). Tile quads per frame are capped by `AppConfig::gfx.max_tiles` on every system | Tiles as sprites would cost ~12 µs each on an Old 3DS (a full 16×16 layer is up to 416 tiles); pre-rendered chunks need ~1.5 MB of VRAM per layer and stall when scrolling or editing tiles. Batches skip the per-tile record, sort and texture lookup, keep layers, cameras, parallax, flips and animated tiles exact, and need one small contract function. See [rendering.md](rendering.md), [plan 012](../../specs/012-tilemaps-and-collision/plan.md). |
| D27 | **build-time conversions that CMake scripts can't do run in `nnh-assetc`**, a host C++ tool (`tools/assetc/`), first for Tiled maps: a target in PC presets, built for the host with `ExternalProject` in cross presets (or `-DNNH_ASSETC=<path>`), only when a game needs it; one step per file with a Ninja depfile | Fulfils D6/D10's `assetc`. CMake's JSON access is quadratic and it can't write binary files; Python would be a new dependency for every game. Sharing the format and validator with core means the build checks maps with the console's own code. |
| D28 | **The PC drives a Development game over the game's own log connection**: `nnh` sends fixed 16-byte messages stamped with the frame they apply to (buttons, sticks, touch, screenshot, hello/bye); the game answers with in-band records (`0x1E 'N' 'R'` + type + length) in its log stream (ticks, screenshots, late/rejected). Core merges remote input into the frame's sample before input is read; platforms only move bytes and read their screens back. The simulator connects to `nnh` on the loopback interface (`NNH_REMOTE`). Debug/Development only; Release has no code for it | One connection the console opens to the PC that launched it: no new port, and log lines and records stay in order (frame-exact `expect`). Fixed-size messages are bounded and fuzzed without heap; `nnh_link`'s codec allocates per message, needs a pairing secret and isn't a game dependency. Frame stamps make scripts repeatable; the same session code serves console and simulator. See [plan 013](../../specs/013-console-remote/plan.md). |

## NNH Link (D7, D8, D24)

```
 console (NNH Link)                          PC (`nnh serve`)
 discover servers (UDP 17492 broadcast) ───>  answers: id + name
 pair once (6-digit code), then every
 connection: mutual HMAC proofs         <──>  (TCP 17492, every message MAC'd)
 list games                             ───>  the repository's games (nnh_add_game)
 build <game> [release]                 ───>  cmake --build (incremental)
                                        <───  build output lines, then the .3dsx (+ SHA-256)
 save to sdmc:/3ds/nnh/, start it through Luma3DS's hb:ldr with --nnh-return=<Link>
 game stdout/stderr                     ───>  shown in the terminal (TCP 17491, 3dslink-style)
 the game exits → NNH Link starts again
```

Built in [spec 011](../../specs/011-nnh-link/plan.md); guide: [guides/nnh-link.md](../guides/nnh-link.md).
Still open: installing `.cia` files from Link (needs AM service access) and Link itself as a `.cia`.

## Future platforms

Switch (libnx + deko3d), Wii U (wut). A platform is accepted when every example builds and runs on it.
Platform-specific features (stereo 3D, second screen, Joy-Con) are exposed through capability queries,
never through `#ifdef` in game code.

## Change log

- 2026-11-17: D11–D13 added (event-driven architecture, SDD + TDD, file structure rules).
- 2026-11-17: D14 added (docs as source of truth, generated wiki).
- 2026-11-17: D15–D18 added (core + platform libraries, capabilities/extensions, foundation first,
  safe by default). D3's "PC simulator" is now `nnh::platform_pc`.
- 2026-11-21: D25 (opt-in game features), D26 (tile batches) and D27 (`nnh-assetc` host tool), the last two from
  plan 012, approved by the maintainer. D27 replaces D6's "content-hash cache" with per-file steps and
  depfiles.
- 2026-11-21: D28 (console remote over the log connection), from plan 013, approved by the maintainer; implemented
  the same day (spec 013) and verified on an Old 3DS XL. Changed while building it, within D28: a `screenshot` and
  the end of a `drag` each take a frame of the script clock (plan 013, "Settled in stage 5").
