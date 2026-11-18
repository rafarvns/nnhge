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
| D6 | Assets compiled ahead of time (`assetc`) into platform formats, packed in romfs | No decoding cost on the console; content-hash cache keeps it incremental. |
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
| D19 | **Until M3, game images are embedded in the executable**, converted at build time (tex3ds `.t3x` on 3DS, PNG on PC) and loaded by name | Games get graphics now without the romfs/`assetc` pipeline; loading by name keeps the API when M3 moves images to romfs (D6). |
| D20 | **Rendering is recorded, then submitted per screen in passes; one texture type for 2D and 3D** (3DS: `C3D_Tex` drawn by citro2d; PC: SDL renderer on an `SDL_GPUDevice`, default renderer as headless fallback) | Layers, batching and stereo (one list, two eyes) come for free, and 3D (M8) slots in as a pass without changing 2D calls or textures. See [rendering.md](rendering.md). |

## NNH Link (planned flow, D7/D8)

```
 console (NNH Link)                      PC (`nnh serve`)
 discover server (UDP broadcast)  ───>   announce project(s)
 "build <project> <preset>"       ───>   cmake --build (incremental)
                                  <───   build log stream
                                  <───   .3dsx (+ romfs)
 save to sdmc:/3ds/nnh/, launch it (mechanism to be decided, see below)
 game stdout/stderr               ───>   shown in terminal (3dslink-style redirect)
```

Open questions: how NNH Link launches a `.3dsx` — libctru has no 3dsx-to-3dsx chainload API
(`aptSetChainloader` only targets installed titles), so options are Luma3DS's homebrew loader service,
returning to the Homebrew Launcher with a target, or shipping Link as a `.cia`; auto-return to NNH Link after the game exits; installing `.cia` from Link (needs AM
service access); LAN authentication (at least a pairing code).

## Future platforms

Switch (libnx + deko3d), Wii U (wut). A platform is accepted when every example builds and runs on it.
Platform-specific features (stereo 3D, second screen, Joy-Con) are exposed through capability queries,
never through `#ifdef` in game code.

## Change log

- 2026-11-17: D11–D13 added (event-driven architecture, SDD + TDD, file structure rules).
- 2026-11-17: D14 added (docs as source of truth, generated wiki).
- 2026-11-17: D15–D18 added (core + platform libraries, capabilities/extensions, foundation first,
  safe by default). D3's "PC simulator" is now `nnh::platform_pc`.
