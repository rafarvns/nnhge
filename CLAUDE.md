# CLAUDE.md

This file guides Claude Code (claude.ai/code) when working in this repository.

## What this is

**NNHGE — Native Nimble Homebrew Game Engine.** An open-source (zlib) C++ game engine that produces
**native executables for homebrew-enabled Nintendo consoles**. First target: **Nintendo 3DS**. Later
targets (Switch, Wii U, …) must fit the same platform layer without rewriting games.

Three pillars:

1. **Engine** — C++ library with a small, well-documented, powerful public API, **event-driven**
   (event bus + scenes).
2. **PC simulator** — the same game code builds as a native desktop app (SDL3; Windows first) that mimics
   the console (two screens, buttons, 60 Hz, 3DS limits). A host build, not an emulator.
3. **NNH Link** (spec 011, `apps/nnh-link` + `nnh serve`) — a console app that asks the PC to build,
   receives the result, runs it and streams logs back. Compilation always happens on the PC
   ([guide](docs/guides/nnh-link.md)).

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
([platforms.md](docs/architecture/platforms.md)). Foundation services (spec 002, implemented):
`<nnh/time.hpp>`, `<nnh/log.hpp>`, `<nnh/assert.hpp>`, `<nnh/result.hpp>`, `<nnh/memory.hpp>`,
`<nnh/fs.hpp>` (+ `nnh::save`), `<nnh/system.hpp>`, `<nnh/profile.hpp>`, debug overlay — use them instead
of ad-hoc code ([foundation.md](docs/architecture/foundation.md), demo `examples/foundation`). Event bus
and scenes (spec 001, implemented; demo `examples/scenes`): `<nnh/events.hpp>` — `subscribe`/`publish`/`Subscription`, delivered in
`nnh::frame()` ([api/events.md](docs/api/events.md)); `<nnh/scene.hpp>` — `Scene`, `scenes::push/pop/replace`,
`nnh::run()` ([api/scene.md](docs/api/scene.md)). Full input (spec 003, implemented; demo `examples/input`):
`<nnh/input.hpp>` — buttons, `circle_pad()`/`c_stick()`, touch queries and events; PC keyboard, mouse,
gamepad ([api/input.md](docs/api/input.md)). 2D graphics (spec 004, implemented; demo `examples/sprites`):
`<nnh/texture.hpp>` — `load_texture()` from `nnh_add_game(... IMAGES ...)`; `<nnh/gfx.hpp>` — `gfx::draw`,
shapes, layers, `Camera2D`; recorded and submitted per screen so 3D can be added
([architecture/rendering.md](docs/architecture/rendering.md)). Text (spec 005, implemented; demo
`examples/text`): `gfx::draw_text`/`measure_text` with the built-in NNHGE font, identical on every system;
debug text uses it too ([api/text.md](docs/api/text.md)). Assets (spec 007; demo `examples/sprites`):
`nnh_add_game(... ASSETS assets)` packs a folder (romfs on 3DS, a folder next to the PC program);
`<nnh/assets.hpp>` reads files by case-sensitive path, `load_texture("art/hero")` loads images
([api/assets.md](docs/api/assets.md)). Audio (spec 008, implemented): `<nnh/audio.hpp>` — `load_sound("sfx/jump")` (WAV/Ogg from
assets), `audio::play` voices with volume/pan/pitch/priority, groups and master, `SoundFinished`; streamed
`music::play`/`crossfade` with loop points and fades, `MusicFinished`; Ogg via
vendored `third_party/stb_vorbis`; PC output via SDL 3 + a software mixer, 3DS via ndsp; demo
`examples/audio` ([api/audio.md](docs/api/audio.md)). Collision and tilemaps (spec 012, implemented; demo
`examples/platformer`): `<nnh/collision.hpp>` (pure, only `<nnh/math.hpp>`) — box queries, `collision::move`
through a `Grid` the game describes (its own `Cell` array or a callback; solid and one-way cells, no tunnelling),
`contacts`, `cells_touching` ([api/collision.md](docs/api/collision.md)); `<nnh/tilemap.hpp>` — `Tilemap::create`
(one block, fixed capacities), tilesets, layers, `set_tile`/`fill`, objects, typed properties, animated tiles,
`collision_grid()`; `load_tilemap("levels/level1")` loads a map the build converted from a Tiled `.tmj` in `ASSETS`
(`nnh-assetc`, D27; refusals print `file: reason`; fixtures in `tests/data/maps/`); `gfx::draw_tilemap` draws tile
batches (D26, needs `AppConfig::gfx.max_tiles`, 0 by default; Old 3DS: 4 full layers + 300 sprites at 60 Hz)
([api/tilemap.md](docs/api/tilemap.md), guide [levels.md](docs/guides/levels.md)).
Collision layers and queries (spec 019, implemented; D34; demo `examples/queries`): in
`<nnh/collision.hpp>` — `Layers` (32 bits, the game's own enum), `detects(mask, layers)`, `box_view(...)` over the
game's own arrays (structs by member pointer), `QueryFilter`; `raycast`/`raycast_all` against grids (exact walk: no
tunnelling, no corner leaks) and boxes, `cells_crossed`, `line_of_sight`, `box_cast`, `boxes_at`,
`boxes_overlapping`, `cell_at`; no allocation, the same bits everywhere ([api/collision.md](docs/api/collision.md)).
Tilemaps: layer sets per `collision` layer and solid/one-way tile (Tiled `collision_layers`), `collision_grid(mask)`
([api/tilemap.md](docs/api/tilemap.md#collision-layers)). The platformer uses layered coins and turrets.
Entities, tags and object pools (spec 020, implemented, console check pending; D35; demo `examples/entities`):
`<nnh/pool.hpp>` — `Pool<T, N>` (inline, no allocation) and `Pool<T>::create(n)` (one block), `Handle<T>` (16-bit slot
+ 16-bit generation: stale use detected), lowest free slot, released slots quarantined until `flush()`, `items()`/`each`
in slot order ([api/pool.md](docs/api/pool.md)); `<nnh/entities.hpp>` — `World::create(desc, kinds)` (one block),
`component<T>(cap[, remove hook])` over the game's plain structs in stable-slot arrays, `spawn`/`destroy` (hooks first,
destroys from hooks queued), `add/get/has/remove<T>`, `each<A, B...>` (first kind's slot order, safe destroy/spawn
inside), `storage<T>()` views (box views read bodies in place, `owner(slot)`), tags `nnh::tag("enemy")` with
`find`/`first`/`each_tagged`; both bind to a scene through D30 (flush before update, clear after `on_exit`)
([api/entities.md](docs/api/entities.md)). The platformer's coins, turrets and sparkles are entities, its bullets a pool.
Tweens and timers (spec 015, implemented; demo `examples/tweens`): `<nnh/tween.hpp>` — 19
libm-free easings (same bits everywhere), `nnh::Timer`, `TweenSet` (float/Vec2/Color by pointer or setter, loops,
yoyo, sequences, `after`/`every`, callbacks or your own finish events) on the frame clock (`Frames`/`FrameTime` in
`<nnh/time.hpp>`); a set built with `{*this, ...}` in a scene is updated and cleared by it (D30, scene attachments)
([api/tween.md](docs/api/tween.md)).
Data assets (spec 014, implemented; D29; demo `examples/data`): `<nnh/data.hpp>` — plain
structs + `NNH_DATA_TYPE(Enemy, "enemy", NNH_DATA_FIELD(hp, .min = 1), ...)` in a schema source named in
`nnh_add_game(... DATA_SCHEMA data_types.cpp ASSETS assets)`; hand-written TOML-subset files `assets/enemies/goblin.enemy`
checked by the build (`nnh-assetc data`/`data-index`, `file:line: reason`, fixtures in `tests/data/data_assets/`) and
converted; `data::load("enemies/goblin", e)`, `count<T>`/`name<T>`, `Ref<T>`/`TextureRef` + `data::path(ref)`;
allocation-free once the index `.nnh_data` is loaded. Schema sources compile for the host too: declarations only
([api/data.md](docs/api/data.md)).
Sprite animation (spec 016, implemented; demo `examples/animation`): `<nnh/animation.hpp>` —
`SpriteLibrary` (one block, tag `animation`: frames, clips, markers, a state machine), a 64-byte `SpriteAnimator` per
sprite (`update()` on the frame clock, notices by callback or returned view, `draw_options()`); `<name>.ase.json`
Aseprite exports → `nnh-assetc sprite` → `load_sprite_library("<name>")` (D31; fixtures in `tests/data/aseprite/`)
([api/animation.md](docs/api/animation.md)).
Input actions (spec 017, implemented; demo `examples/actions`): `<nnh/actions.hpp>` —
`ActionSet` of button/axis/vector actions with up to 4 bindings each (buttons, stick directions, sticks, composites,
touch areas), contexts bound to scenes, `pressed`/`vector`/`held_frames`/repeat and `ActionPressed` events in the same
frame (D32 after-input hook), listen/conflicts/reset for a controls screen, `NNHB` overrides saved with `nnh::save`
([api/actions.md](docs/api/actions.md)).
Camera helpers (spec 018, implemented; D33; demo `examples/camera`): `<nnh/camera.hpp>` —
`nnh::CameraRig`, a 128-byte value per view: `update(target)` once per frame after moving the target (one 60 Hz
frame, bit-identical on PC and 3DS), `apply()` writes `Camera2D`; dead zone in screen pixels, per-frame smoothing,
limits (`set_limits(map.bounds())`), seeded trauma shake, zoom about the centre, `CameraLayout::TallWorld` (both
screens as one world) or two rigs for two views, `to_world`/`to_screen`, `debug_draw()`. `Camera2D::zoom` (0.5–4)
is applied by core to sprites, text, shapes and tile layers (snapped tile edges, row chunks below zoom 1; zoom 1
byte-identical) ([api/camera.md](docs/api/camera.md), [gfx.md#zoom](docs/api/gfx.md#zoom)).
Console remote (spec 013, implemented, D28; no game API): `nnh run <game> [--pc] --script <file.nnhs>` drives a
Debug/Development game over its log connection — buttons, sticks, touch, screenshots, log checks — and exits 0/1/2;
core side `engine/core/src/remote/`, contract `nnh/platform/remote.hpp`, PC side `tools/nnh/src/remote/`; scripts in
`examples/<name>/tests/*.nnhs` ([guides/testing-on-the-console.md](docs/guides/testing-on-the-console.md)).

```
engine/core/include/nnh/<module>.hpp             public API for games (app, gfx, input, debug, ...)
engine/core/src/<module>/                        core implementation (app/, gfx/, input/, debug/, core/)
engine/core/contract/nnh/platform/<area>.hpp     platform contract (app, video, input, ...)
engine/platforms/<system>/src/<area>_<system>.cpp   platform libraries: pc (SDL3), 3ds (libctru+citro2d)
engine/platforms/fake/                           scriptable fake platform for tests (nnh/platform_fake.hpp)
engine/platforms/shared/                         code shared by platforms (stdio_fs.cpp)
engine/platforms/3ds/include/nnh/3ds/            3DS extension headers (system-only features)
tests/framework/  tests/<module>/                in-house test framework, unit tests mirroring core/src
tests/platform_conformance/                      contract tests run against every platform
tests/fuzz/                                      fuzz targets (mini-fuzz in nnh_tests; libFuzzer with NNH_BUILD_FUZZERS)
link/                                            nnh_link (spec 011): sockets, crypto, messages, pairing, sessions; PC + 3DS
apps/nnh-link/                                   NNH Link app (spec 011): pick a PC, pair, build and run from the console
tools/nnh/                                       the nnh command (spec 010): build, run on the console, install; test scripts (013)
tools/assetc/                                    nnh-assetc (specs 012, 014): Tiled .tmj → .nnhmap, data files → NNHD, .nnh_data; host-built in 3DS presets (NNH_HOST_CXX)
tools/data_schema/                               main() of each game's data schema program (spec 014, cmake/nnh_data.cmake)
examples/<name>/                                 one small program per feature (tests/*.nnhs: its test scripts)
specs/NNN-slug/                                  spec.md, plan.md, tasks.md
docs/                                            architecture/, process/, tech/, api/, guides/
cmake/  scripts/                                 nnh_add_game(), engine options, SDL3 fetch, dkp.sh, docs tool
third_party/<library>/                           vendored, unmodified, pinned code (stb_vorbis)
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
scripts/dkp.sh cmake --build --preset 3ds-release --target dist   # → build/3ds-release/dist/<game>/ (.3dsx, .cia)
build/pc-debug/tools/nnh/nnh run hello                         # build, send to the console (HBL → Y), show its log
build/pc-debug/tools/nnh/nnh install audio                      # .cia over the network (FBI → Receive URLs)
NNH_PRESET=pc-debug build/pc-debug/tools/nnh/nnh run input --pc --no-build --script examples/input/tests/remote.nnhs   # test script in the simulator: nnh: PASS, exit 0
build/pc-debug/tools/nnh/nnh run input --script examples/input/tests/remote.nnhs   # the same on the console (HBL → Y); PNGs in build/logs/
build/pc-debug/tools/nnh/nnh serve                              # build server for NNH Link (spec 011)
NNH_LINK_DEMO=games NNH_EXIT_AFTER_FRAMES=20 NNH_SCREENSHOT=build/link.png build/pc-debug/apps/nnh-link/nnh-link.exe   # NNH Link screens
NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/shot.png build/pc-debug/examples/hello/hello.exe
ANIMATION_DEMO=check NNH_EXIT_AFTER_FRAMES=605 build/pc-debug/examples/animation/animation.exe   # showcase (default)|check|bench
PLATFORMER_DEMO=bench NNH_EXIT_AFTER_FRAMES=90 NNH_SCREENSHOT=build/bench.png build/pc-debug/examples/platformer/platformer.exe   # level1 (default)|bonus|bench|gate|flips|replay
build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs   # tile gate on the console (Development, HBL → Y): bench: PASS/FAIL
build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs  # levels + replay on the console: checksums = PC's, 60 Hz PASS/FAIL
build/pc-debug/tools/nnh/nnh run data [--pc] --script examples/data/tests/check.nnhs        # data assets: self-check checksum = PC's
QUERIES_DEMO=all NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/queries.png build/pc-debug/examples/queries/queries.exe   # first|all|cast|water|touch|check|bench
build/pc-debug/tools/nnh/nnh run queries [--pc] --script examples/queries/tests/check.nnhs  # rays, touches, self-check 4ACAA177, bench
python -I scripts/docs_tool.py check                           # docs: broken links (python3 on Linux)
scripts/check_release_strings.sh build/3ds-release/examples/hello/hello.elf   # Release has no debug strings
```

Linux checks and libFuzzer run on the homelab build VM `nnhge-build` (VM 120, off by default; the
maintainer allows any session to start/stop it and run builds; `apt`/system changes need an OK):

```sh
ssh homelab 'sudo qm status 101 | grep -q stopped && free -m && sudo qm start 120'   # never with Bazzite (101)
git ls-files -z -co --exclude-standard | tar --null -T - -cf - | ssh nnhge-build 'rm -rf ~/wt/nnhge && mkdir -p ~/wt/nnhge && tar -xf - -C ~/wt/nnhge'
ssh nnhge-build 'cd ~/wt/nnhge && cmake --preset pc-debug && cmake --build --preset pc-debug && ctest --preset pc-debug'
ssh homelab 'sudo qm shutdown 120'                                                  # when done (it also idles off)
```

- **3DS builds go through `scripts/dkp.sh` on Windows**: devkitPro's toolchain only runs from its MSYS2.
- **GitHub CI is manual** (saves Actions minutes): verify locally before pushing and use the homelab
  build VM for Linux checks; don't trigger or wait for CI runs unless asked. The wiki workflow still
  runs on push to `main` (a few seconds).
- Read the screenshot PNG to verify simulator output without a human.
- Run on a console: Homebrew Launcher → Y, then `nnh run <game>` (builds, finds the console, shows the coloured
  log, saves it in `build/logs/`; [guide](docs/guides/nnh-tool.md)). devkitPro's 3dslink still works:
  `/c/devkitPro/tools/bin/3dslink.exe -s <file>.3dsx -a <ip>`
  (`-s` keeps the PC listening, so logs of Debug/Development builds appear in the terminal).
- Console checks: write a `.nnhs` script and run it with `nnh run <game> --script ...`; the maintainer only presses Y,
  and the verdict, log and screenshots come back to the PC (keep console tests yes/no or fully scripted).
- Platform libraries never call core (no logging there): return errors / set `platform::last_error()`.
- Build budgets: edit one game `.cpp` → simulator < 2 s, `.3dsx` < 5 s; clean engine < 30 s. Measure
  build changes before/after ([docs/tech/cmake.md](docs/tech/cmake.md)).

## Language

Code, comments, docs, skills and agents in **English** (international contributors); talk to the
maintainer in Portuguese.

## Licensing

zlib (`LICENSE`); contributions under the same license. No Nintendo SDK, leaked code or copyrighted
assets. Don't copy code from incompatibly licensed projects; reference them in `docs/REFERENCES.md`.
