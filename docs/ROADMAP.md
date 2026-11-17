# Roadmap

Slow and steady: each item is done only when it works on **PC and 3DS**, has docs and an example.
Order may change; keep this file honest.

## M0 — Foundation
- [x] License (zlib), `LICENSE`, `README.md`, `CONTRIBUTING.md`
- [x] Install devkitPro `3ds-dev` and document setup on Windows — Linux/macOS steps written, untested
- [x] `CMakePresets.json` with `pc-debug`, `pc-release`, `3ds-debug`, `3ds-release`; ccache detection
- [x] `nnh_add_game()` CMake function
- [x] Example `hello`: clear both screens, print text, exit on START — verified on PC (screenshot)
- [x] `hello` verified on a real 3DS (2026-11-17, sent with 3dslink to the Homebrew Launcher)
- [ ] CI (GitHub Actions): workflow written, not yet run (no remote repository yet)
- [x] Build-time baseline (2026-11-17, see below)

Build-time baseline (2026-11-17, maintainer's PC, MinGW GCC 16 + ccache / devkitARM):

| Measurement | Budget | Result |
|-------------|--------|--------|
| Edit one game `.cpp` → simulator relinked | < 2 s | 1.3 s |
| Edit one game `.cpp` → `.3dsx` | < 5 s | 1.4 s |
| Clean build, 3DS (engine + hello) | < 30 s | 3.3 s |

## M1 — Loop, input, logging
- [ ] App lifecycle, fixed-timestep loop, frame timing
- [ ] Input: buttons, Circle Pad, C-Stick/ZL/ZR (New 3DS), touch; PC mapping (keyboard, gamepad, mouse = touch)
- [ ] Logging with levels; stdout redirect to PC via 3dslink on console
- [ ] `NNH_ASSERT` and on-screen crash report

## M2 — 2D graphics
- [ ] Textures, sprites, sprite batching, both screens, camera
- [ ] Text rendering (bitmap fonts first)
- [ ] Simulator window: two screens layout, scale, 3D slider control

## M3 — Assets
- [ ] `assetc`: PNG → platform textures, incremental cache
- [ ] romfs packaging and `nnh::fs` API

## M4 — Audio
- [ ] Sound effects (WAV), streaming music (OGG), mixer channels — ndsp / SDL3

## M5 — Packaging
- [ ] `.3dsx` with icon/metadata (SMDH)
- [ ] `.cia` with banner (bannertool + makerom), `nnh package`

## M6 — NNH Link
- [ ] `nnh run --console <ip>` (3dslink-compatible send + log stream)
- [ ] NNH Link app: discover dev server, list projects, request build, receive and launch
- [ ] Pairing code for LAN safety

## M7 — Game structure
- [ ] Scenes, entities/components, tilemaps, collision, tweens, save data

## M8 — 3D
- [ ] citro3d meshes, materials, stereoscopic rendering, model import in `assetc`

## Later
- [ ] Hot reload of game code in the simulator
- [ ] Profiler overlay, memory tracker
- [ ] Second platform (Switch)
