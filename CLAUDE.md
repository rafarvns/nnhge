# CLAUDE.md

This file guides Claude Code (claude.ai/code) when working in this repository.

## What this is

**NNHGE — Native Nimble Homebrew Game Engine.** An open-source (free, forkable) C/C++ game engine
that produces **native executables for homebrew-enabled Nintendo consoles**. First target: **Nintendo 3DS**.
Later targets (Switch, Wii U, …) must fit the same platform layer without rewriting games.

Three pillars:

1. **Engine** — C++ library with a small, well-documented, powerful public API.
2. **PC simulator** — the same game code builds as a native desktop app (Windows first, then Linux and macOS)
   that mimics the console (two screens, touch, buttons, 3D slider, memory limits). This is a *host build*
   of the engine, not an emulator. Hardware accuracy checks use the Azahar emulator and real consoles.
3. **NNH Link** — a homebrew app installed on the console that connects to the dev server on the PC,
   **triggers builds**, receives the result, launches it and streams logs back. Compilation always happens
   on the PC (the 3DS CPU cannot compile in reasonable time); the console *drives* the build.

Release artifacts per platform: 3DS → `.3dsx` (Homebrew Launcher) **and** `.cia` (installable on the HOME Menu).

Status and next steps: see [docs/ROADMAP.md](docs/ROADMAP.md). Design decisions: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
Prior art to learn from: [docs/REFERENCES.md](docs/REFERENCES.md).

## Working rules (non-negotiable)

- **Small steps.** Implement one roadmap item at a time, fully working on PC *and* 3DS, documented and with an
  example before moving on. Do not scaffold empty modules "for later". Update the ROADMAP checkbox when done.
- **Build speed is a feature.** Every change must respect the budgets in "Build speed" below. If a change
  makes builds slower (heavy template header, new big dependency, include of a fat header in a public header),
  justify it or don't do it.
- **Same API on every platform.** Games only include `<nnh/...>` headers. No `#ifdef __3DS__` in game code
  or in public headers; platform differences live in `engine/src/platform/<name>/`.
- **Docs ship with the code.** A public API change is not done until its header comments, its page in
  `docs/api/`, and at least one example are updated (see "Documentation" below).
- **The 3DS is the constraint.** Design for 128 MB RAM (≈64 MB usable on Old 3DS apps), a 268 MHz ARM11,
  PICA200 GPU, linear-memory rules for GPU buffers. The PC build must not hide problems the 3DS will have
  (enforce the same memory budgets and texture size rules in the simulator).

## Repository layout (target — create directories only when the roadmap item needs them)

```
engine/include/nnh/     public API, one header per module (nnh/gfx.hpp, nnh/input.hpp, ...)
engine/src/core/        platform-independent code
engine/src/platform/3ds/  libctru + citro2d/citro3d + ndsp backend
engine/src/platform/pc/   SDL3 backend (simulator)
tools/nnh/              `nnh` CLI: new, build, run, package, serve (dev server for NNH Link)
tools/assetc/           asset compiler (PNG/WAV/fonts → platform formats), incremental with a cache
link/                   NNH Link console app (itself built with the engine)
examples/               one small, compiled example per feature
tests/                  unit tests for core (run on PC)
docs/                   guides, api/ reference pages, ROADMAP, ARCHITECTURE, REFERENCES
cmake/                  toolchain glue and the `nnh_add_game()` function
```

## Build

Prerequisites: CMake ≥ 3.25, Ninja, a host C++20 compiler, and for 3DS devkitPro (`3ds-dev`) with Ninja
installed in devkitPro's MSYS2. Full setup: [docs/guides/getting-started.md](docs/guides/getting-started.md).
Maintainer's machine: WinLibs MinGW-w64 GCC + CMake + Ninja on PATH (Git Bash); devkitPro at `C:\devkitPro`.

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug     # simulator
ctest --preset pc-debug                                        # unit tests (tests/, plain asserts)
build/pc-debug/tests/nnh_tests.exe                             # same, shows each test
scripts/dkp.sh cmake --preset 3ds-release                      # 3DS configure
scripts/dkp.sh cmake --build --preset 3ds-release              # → build/3ds-release/**/<game>.3dsx
NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/shot.png build/pc-debug/examples/hello/hello.exe
```

- **3DS builds must go through `scripts/dkp.sh` on Windows**: devkitPro's toolchain file refuses a
  Windows-native CMake and only runs from `C:\devkitPro\msys2`. The script handles that.
- The screenshot env vars let you verify simulator output visually (read the PNG) without a human.
- Run on a console: Homebrew Launcher → Y, then `/c/devkitPro/tools/bin/3dslink.exe <file>.3dsx -a <ip>`.
- `.cia` packaging (makerom + bannertool) is not implemented yet (M5).
- Unit tests cover only platform-independent code in `engine/src/core/`; keep logic there testable.
- New games/examples use `nnh_add_game()` (`cmake/nnh_game.cmake`); add each example to `examples/CMakeLists.txt`.

## Build speed

Budgets (measure on the dev machine, report regressions in the PR):

- Edit one game `.cpp` → simulator relinked and running: **< 2 s**.
- Edit one game `.cpp` → `.3dsx` ready: **< 5 s**.
- Clean build of the engine for one platform: **< 30 s**.

How we keep them:

- CMake + **Ninja** only; presets in `CMakePresets.json`. `ccache`/`sccache` used automatically when found.
- The engine is a static library built once; a game build compiles only the game's sources.
- Public headers are light: forward declarations, no `<iostream>`, no `<regex>`, no heavy templates,
  no platform SDK headers. Implementation details stay in `.cpp` (pimpl or opaque handles).
- Assets are compiled incrementally (content hash cache); unchanged assets are never reprocessed.
- No large third-party dependencies in the engine core. Vendored single-file libs are fine if small.
- Planned: game code as a hot-reloadable shared library in the PC simulator.

## Code conventions

- C++20 (devkitARM's GCC supports it), compiled with `-fno-exceptions -fno-rtti` in the engine. Errors are
  returned (`nnh::Result`/`std::optional`-style), and fatal misuse goes through `NNH_ASSERT`.
- No hidden heap allocation in per-frame paths. Prefer fixed-capacity containers and explicit allocators.
- Namespaces: everything public in `nnh::`, internals in `nnh::detail`/`nnh::platform`.
- Naming: `PascalCase` types, `snake_case` functions and variables, `kPascalCase` constants,
  `NNH_UPPER_CASE` macros. Files `snake_case.hpp/.cpp`.
- Formatting by `.clang-format` (add with the first code). Code, comments and docs in **English**
  (international contributors); talk to the maintainer in Portuguese.

## Documentation (written for humans and for Claude Code)

- Every public symbol in `engine/include/nnh/` gets a Doxygen comment: what it does, ownership/lifetime,
  cost (allocates? per-frame safe?), platform notes, and a 2–5 line usage snippet.
- `docs/api/<module>.md`: concepts, common patterns, pitfalls, and limits per platform (e.g. 3DS texture
  sizes must be powers of two, max 1024).
- `docs/guides/`: task-oriented ("draw a sprite", "play a sound", "ship a CIA").
- Examples are part of the build and CI, so docs never show code that doesn't compile.

## Licensing

zlib (see `LICENSE`). Contributions are accepted under the same license.
Do not copy code from projects with incompatible licenses; reference them in `docs/REFERENCES.md` instead.
No Nintendo SDK, leaked code or copyrighted assets, ever. Homebrew toolchain only.
