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
