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
- [x] CI (GitHub Actions): Linux PC build + tests + headless `hello` smoke test; 3DS build with `.3dsx` artifact
- [x] Build-time baseline (2026-11-17, see below)

Build-time baseline (2026-11-17, maintainer's PC, MinGW GCC 16 + ccache / devkitARM):

| Measurement | Budget | Result |
|-------------|--------|--------|
| Edit one game `.cpp` → simulator relinked | < 2 s | 1.3 s |
| Edit one game `.cpp` → `.3dsx` | < 5 s | 1.4 s |
| Clean build, 3DS (engine + hello) | < 30 s | 3.3 s |

After spec 002 (2026-11-17, same machine): edit one game `.cpp` → simulator 1.4 s, `.3dsx` 2.1 s; clean
3DS configure + build (engine + 2 examples) 14.3 s. Release `hello.3dsx`: 155,776 → 193,016 bytes
(+37 KB for the whole foundation). Linux (homelab build VM, GCC 13): cold `pc-debug` incl. SDL3 from
source ~60 s.

After spec 001 (2026-11-17, same machine): edit one game `.cpp` → simulator 1.2 s, `.3dsx` 2.5 s; clean
3DS configure + build (engine + 3 examples) 13.7 s. Release `hello.3dsx`: 193,016 → 199,648 bytes (+6.5 KB:
event bus and scene stack, linked by `nnh::frame()`); `scenes.3dsx` 214,924 bytes.

## M0.5 — Way of working
- [x] Architecture (event bus + scenes), process (SDD, TDD, SOLID, constitution) and tech docs
- [x] Project skills and agents in `.claude/`
- [x] Code reorganized into module folders, one concept per file

## M1 — Core foundation ([spec 002](../specs/002-core-foundation/spec.md)) — implemented
- [x] Core + platform libraries split (`nnh::core`, `nnh::platform_{pc,3ds,fake}`), platform contract, conformance tests (stage 1)
- [x] Capabilities and system info; extension headers mechanism (stage 8)
- [x] Build flavors (Debug, Development, Release) (stage 2)
- [x] Time: monotonic clock, frame delta, fixed timestep (stage 3)
- [x] Logging: levels, channels, sinks (console, ring buffer, 3dslink, debugger) (stage 4; file sink in stage 7)
- [x] Errors: `NNH_ASSERT`/`VERIFY`/`PANIC`, `Result<T>`, crash screen, 3DS exception handler (stage 5; report file in stage 7)
- [x] Memory: tagged allocators, budgets, leak report (stage 6)
- [x] Debug overlay, profiler, simulator pause/step (stage 8)
- [x] GDB-on-3DS guide ([guides/debugging-on-3ds.md](guides/debugging-on-3ds.md)), `examples/foundation` (stage 9)
- [x] File safety: confined `nnh::fs`, atomic saves, fuzzed parsers (stage 7)
- [x] CI hardening: least-privilege permissions, actions pinned by SHA, release-strings check, fuzz job (stage 9)
- [x] Foundation verified on a real 3DS (3dslink logs, overlay, save counter, crash screen, crash.txt) — 2026-11-17
- [ ] CPU-fault crash screen on 3DS with Luma3DS "Set developer UNITINFO" — maintainer, later

## M1b — Events, scenes, input
- [x] Event bus + scenes ([spec 001](../specs/001-event-bus-and-scenes/spec.md)): `<nnh/events.hpp>`, `ButtonPressed`/`ButtonReleased`, `<nnh/scene.hpp>` + `nnh::run()`, `examples/scenes` (2026-11-17)
- [x] `examples/scenes` run on a real 3DS (2026-11-17)
- [ ] Input: Circle Pad, C-Stick/ZL/ZR (New 3DS), touch; PC mapping (gamepad, mouse = touch)

## M2 — 2D graphics
- [ ] Textures, sprites, sprite batching, both screens, camera
- [ ] Text rendering (bitmap fonts first)
- [ ] Simulator window: two screens layout, scale, 3D slider control

## M3 — Assets
- [ ] `assetc`: PNG → platform textures, incremental cache
- [ ] romfs packaging (read side of `nnh::fs`; the confined write side comes with spec 002)

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
- [ ] Entities/components, tilemaps, collision, tweens, scene transitions (scenes and saves: done)

## M8 — 3D
- [ ] citro3d meshes, materials, stereoscopic rendering, model import in `assetc`

## Later
- [ ] Hot reload of game code in the simulator
- [ ] Profiler overlay, memory tracker
- [ ] Second platform (Switch)
