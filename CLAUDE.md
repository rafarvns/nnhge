# CLAUDE.md

This file guides Claude Code (claude.ai/code) when working in this repository.

## What this is

**NNHGE — Native Nintendo Homebrew Game Engine.** An open-source (zlib) C++ game engine that produces
**native executables for homebrew-enabled Nintendo consoles**. First target: **Nintendo 3DS**. Later
targets (Switch, Wii U, …) must fit the same platform layer without rewriting games.

Three pillars:

1. **Engine** — C++ library with a small, well-documented, powerful public API, **event-driven**
   (event bus + scenes).
2. **PC simulator** — the same game code builds as a native desktop app (SDL3; Windows first) that mimics
   the console (two screens, buttons, 60 Hz, 3DS limits). A host build, not an emulator.
3. **NNH Link** (planned) — a console app that asks the PC to build, receives the result, runs it and
   streams logs back. Compilation always happens on the PC.

Artifacts: 3DS → `.3dsx` (Homebrew Launcher) and `.cia` (HOME Menu, planned).

Status and next steps: [docs/ROADMAP.md](docs/ROADMAP.md). Prior art: [docs/REFERENCES.md](docs/REFERENCES.md).

## Where the rules live

| Topic | Read |
|-------|------|
| Non-negotiable rules (checked in every plan) | [docs/process/constitution.md](docs/process/constitution.md) |
| Architecture: events, scenes, patterns, folders, decisions | [docs/architecture/](docs/architecture/) |
| How we work: SDD, TDD, SOLID | [docs/process/](docs/process/) |
| Technology: devkitPro/3DS, CMake, C++ subset, SDL3 | [docs/tech/](docs/tech/) |
| Public API reference | [docs/api/](docs/api/) |
| Feature specs | [specs/](specs/) |

## How we work

```
spec (approved by the maintainer) → plan (Constitution Check) → tasks → TDD per task → review → docs + example → ROADMAP
```

- **No feature code without an approved spec** in `specs/NNN-slug/`. Bug fixes start with a failing test.
- **Test first** for all logic in `engine/core/src/<module>/`; backends are verified by examples, simulator
  screenshots and the 3DS.
- **Small steps**: one roadmap item at a time, working on PC *and* 3DS, documented, with an example.
- **Organized files**: one concept per file, `.cpp` aim ≤ 200 / hard 400 lines, one public header per
  module, explicit CMake source lists ([project-structure.md](docs/architecture/project-structure.md)).
- **Query for state, events for changes**; scenes for game modes; no platform `#ifdef` in game code or
  public headers; no exceptions/RTTI/per-frame heap in the engine.
- **Docs with every change**: after any modification, run the `nnh-docs` skill — update every doc the
  change made false and check links. Repository docs are the source of truth; the
  [GitHub wiki](https://github.com/rafarvns/nnhge/wiki) is generated from them by CI on push to `main`
  (never edit it by hand). A Stop hook (`.claude/settings.json` → `scripts/hooks/docs_reminder.py`)
  sends you back if a turn ends with changed files but no doc change, or with broken links. See
  [docs/process/documentation.md](docs/process/documentation.md).

Project skills (`.claude/skills/`): `nnh-architecture`, `nnh-spec`, `nnh-tdd`, `nnh-new-module`,
`nnh-review`, `nnh-docs`, `nnh-devkitpro`, `nnh-cmake`, `nnh-cpp`, `nnh-sdl3`.
Project agents (`.claude/agents/`): `nnh-spec-writer` → `nnh-architect` → `nnh-tdd-developer` →
`nnh-reviewer`; specialists `nnh-3ds-specialist`, `nnh-build-engineer`. Mapping per step:
[docs/process/README.md](docs/process/README.md).

## Repository layout

Portable core + one library per system, plugged in at link time through the platform contract
([platforms.md](docs/architecture/platforms.md)). Foundation services (time, log, assert/Result,
memory, fs, debug overlay, system capabilities) are being added by spec 002
([foundation.md](docs/architecture/foundation.md)).

```
engine/core/include/nnh/<module>.hpp             public API for games (app, gfx, input, debug, ...)
engine/core/src/<module>/                        core implementation (app/, gfx/, input/, debug/, core/)
engine/core/contract/nnh/platform/<area>.hpp     platform contract (app, video, input, ...)
engine/platforms/<system>/src/<area>_<system>.cpp   platform libraries: pc (SDL3), 3ds (libctru+citro2d)
engine/platforms/fake/                           scriptable fake platform for tests (nnh/platform_fake.hpp)
tests/framework/  tests/<module>/                in-house test framework, unit tests mirroring core/src
tests/platform_conformance/                      contract tests run against every platform
examples/<name>/                                 one small program per feature
specs/NNN-slug/                                  spec.md, plan.md, tasks.md
docs/                                            architecture/, process/, tech/, api/, guides/
cmake/  scripts/                                 nnh_add_game(), engine options, SDL3 fetch, dkp.sh, docs tool
```

CMake targets: `nnh::core`, `nnh::core_contract`, `nnh::platform_{pc,3ds,fake}`, and `nnh::engine`
(core + the preset's platform), which games link.

## Build

Prerequisites and setup: [docs/guides/getting-started.md](docs/guides/getting-started.md).
Maintainer's machine: WinLibs MinGW-w64 GCC + CMake + Ninja on PATH (Git Bash); devkitPro at `C:\devkitPro`.

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug     # simulator (flavors: -debug, -dev, -release)
ctest --preset pc-debug                                        # unit tests
build/pc-debug/tests/nnh_tests.exe [name-filter]               # tests, one line each
scripts/dkp.sh cmake --preset 3ds-release                      # 3DS configure (once)
scripts/dkp.sh cmake --build --preset 3ds-release              # → build/3ds-release/**/<game>.3dsx
NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/shot.png build/pc-debug/examples/hello/hello.exe
python -I scripts/docs_tool.py check                           # docs: broken links (python3 on Linux)
```

- **3DS builds go through `scripts/dkp.sh` on Windows**: devkitPro's toolchain only runs from its MSYS2.
- **GitHub CI is manual** (saves Actions minutes): verify locally before pushing and use the homelab
  build VM for Linux checks; don't trigger or wait for CI runs unless asked. The wiki workflow still
  runs on push to `main` (a few seconds).
- Read the screenshot PNG to verify simulator output without a human.
- Run on a console: Homebrew Launcher → Y, then `/c/devkitPro/tools/bin/3dslink.exe <file>.3dsx -a <ip>`.
- Build budgets: edit one game `.cpp` → simulator < 2 s, `.3dsx` < 5 s; clean engine < 30 s. Measure
  build changes before/after ([docs/tech/cmake.md](docs/tech/cmake.md)).

## Language

Code, comments, docs, skills and agents in **English** (international contributors); talk to the
maintainer in Portuguese.

## Licensing

zlib (`LICENSE`); contributions under the same license. No Nintendo SDK, leaked code or copyrighted
assets. Don't copy code from incompatibly licensed projects; reference them in `docs/REFERENCES.md`.
