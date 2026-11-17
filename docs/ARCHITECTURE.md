# Architecture and decisions

Living document. Each decision has a short rationale; change it with a note when we learn otherwise.

## Layers

```
 game code  ──>  nnh public API (engine/include/nnh)
                      │
                 core (engine/src/core): loop, math, assets, scene, audio mixer API
                      │
          platform backend (engine/src/platform/<target>)
          ├── 3ds : libctru, citro3d/citro2d, ndsp, romfs
          └── pc  : SDL3 window/input/audio + GPU rendering, simulates the 3DS
```

The platform backend implements a small internal interface (`nnh::platform`): video frame begin/end,
textures, draw submission, input polling, audio buffers, file access, time, memory stats. Core and games
never call a platform SDK directly.

## Decisions

| # | Decision | Why |
|---|----------|-----|
| D1 | C++20 subset, no exceptions/RTTI in engine | Smaller binaries, predictable cost on ARM11; still modern syntax. Same choice as Butano. |
| D2 | CMake + Ninja, presets, devkitPro's `3DS.cmake` toolchain | One build system for host and console, fast incremental builds, IDE support (VS, VS Code, CLion). |
| D3 | PC simulator is a host build on SDL3, not an emulator | Instant iteration and native debugging. Emulator (Azahar) and hardware stay as accuracy checks. |
| D4 | Simulator enforces 3DS limits (screen sizes 400×240 + 320×240, memory budget, texture rules) | Avoid "works on PC, breaks on console". |
| D5 | Rendering: 2D first on citro2d/citro3d; 3D (stereoscopic) later on citro3d | citro* are the maintained devkitPro GPU libs; 2D gets games running sooner. |
| D6 | Assets compiled ahead of time by `assetc` into platform formats (e.g. `.t3x` via tex3ds), packed in romfs | No decoding cost on the console; content-hash cache keeps it incremental. |
| D7 | Console builds always happen on the PC | The 3DS cannot compile in useful time. NNH Link *requests* builds and runs results. |
| D8 | NNH Link starts compatible with 3dslink, then adds its own protocol | Reuse a proven transfer path (Homebrew Launcher netloader, port 17491 stdio redirect) before inventing. |
| D9 | `.3dsx` for dev loop, `.cia` for installation | `.3dsx` is fastest to send/launch; `.cia` is the native installable artifact (makerom + bannertool). |
| D10 | Tooling (`nnh` CLI, `assetc`) in C++ | One language for contributors, single static binary, no runtime to install. Revisit if it slows us down. |

## NNH Link (planned flow)

```
 console (NNH Link)                      PC (`nnh serve`)
 ─────────────────                       ─────────────────
 discover server (UDP broadcast)  ───>   announce project(s)
 "build <project> <preset>"       ───>   cmake --build (incremental)
                                  <───   build log stream
                                  <───   .3dsx (+ romfs)
 save to sdmc:/3ds/nnh/, chainload  (libctru envSetNextLoad via Homebrew Launcher)
 game stdout/stderr               ───>   shown in terminal (3dslink-style redirect)
```

Open questions: auto-return to NNH Link after the game exits; installing `.cia` from Link (needs AM service
access); authentication on the LAN (at least a pairing code).

## Future platforms

Switch (libnx + deko3d), Wii U (wut). A platform is accepted when every example builds and runs on it.
Platform-specific features (stereo 3D, second screen, Joy-Con) are exposed through capability queries,
never through `#ifdef` in game code.
