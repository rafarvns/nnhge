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

After spec 003 (2026-11-18, same machine): edit one game `.cpp` → simulator 1.3 s, `.3dsx` 2.5 s; clean
3DS configure + build (engine + 4 examples) 17.7 s. Release `hello.3dsx`: 199,648 → 201,336 bytes (+1.7 KB:
sticks, touch, mirror); `input.3dsx` 213,912 bytes.

After spec 004 (2026-11-18, same machine): edit one game `.cpp` → simulator 1.2 s, `.3dsx` 2.3 s; edit
one image → `.3dsx` 2.7 s; clean 3DS configure + build (engine + 5 examples) 17.4 s. Release
`hello.3dsx`: 201,336 → 224,768 bytes (+23 KB: 2D drawing, textures, tex3ds import); `sprites.3dsx`
(2 embedded images) 243,208 bytes. Old 3DS XL: about **1300 sprites of 16×16 at 60 Hz** (one texture,
`examples/sprites` benchmark, Release); it starts dropping at 1400.

After spec 005 (2026-11-18, same machine): edit one game `.cpp` → simulator 1.2 s, `.3dsx` 2.4 s; clean
3DS configure + build (engine + 6 examples) 17.5 s. Release `hello.3dsx`: 224,768 → 237,380 bytes
(+12.6 KB: built-in font, UTF-8 and layout, debug pass on the font; citro2d's text code no longer
linked); `text.3dsx` 239,736 bytes.

Spec 012 stage 0 (2026-11-21, same machine; host tool `nnh-assetc`): 3DS no-op build 1.69 s with and without
it (the tool is built only on demand); edit one game `.cpp` → simulator 1.4 s, `.3dsx` 2.85 s; clean 3DS
configure + build (engine + 7 examples + NNH Link) 12.1 + 19.3 s; building `nnh-assetc` for the host in a 3DS
folder: 9.0 s the first time (Windows), 0.8 s (Linux VM).

Spec 012 stage 5 (2026-11-21, same machine; the Tiled converter and the `.tmj` asset step; three edits each): edit
`examples/hello/main.cpp` → simulator 1.51–1.60 s before, 1.48–1.52 s after; → `.3dsx` 2.64–2.79 s before,
2.68–2.73 s after (no change: games without maps don't touch the converter). 3DS no-op 1.81 s. Editing a `.tsj`
reconverts only the maps that use it, on PC and 3DS (`ninja -d explain`).

Spec 013 (2026-11-21, same machine; before = commit `82f6e91`, the approved plan without remote code, after = the
finished spec; separate build folders, 3DS without ccache, PC with ccache but every edit a miss; three edits each):
edit `examples/hello/main.cpp` → simulator 1.66–2.07 s before, 1.77–2.04 s after; → `.3dsx` 2.97–3.05 s before,
2.93–2.97 s after. Clean 3DS engine (`nnh_core` + `nnh_platform_3ds`) 10.2 → 11.7 s; the rest (examples, NNH Link,
host tool) 12.7 → 13.1 s; no-op build 2.0 → 1.9 s. Release `hello.3dsx` 261,112 → 261,260 bytes (+148: the remote
is compiled out). All within budget; the simulator edit is close to its 2 s limit both before and after.

Spec 012 finished (2026-11-22, same machine; before = the numbers above, after = collision, tilemaps, the converter
and `examples/platformer` with a Tiled level; three edits each):

| Measurement | Budget | Result |
|-------------|--------|--------|
| Edit `examples/hello/main.cpp` → simulator | < 2 s | 1.41–1.52 s (1.77–2.04 s after spec 013) |
| Edit `examples/hello/main.cpp` → `.3dsx` | < 5 s | 2.61–2.78 s (2.93–2.97 s) |
| Edit a `.cpp` of a game with maps (`platformer/hud.cpp`) → simulator | < 2 s | 1.73 s (4.7 s the first time) |
| Edit a `.cpp` of a game with maps → `.3dsx` | < 5 s | 3.92–4.07 s: ~1 s of it is the host converter's up-to-date check (`ExternalProject`) |
| Edit a map (`level1.tmj`, 128×20, 5 tile layers) → simulator's assets | — | 0.92–0.95 s; the conversion alone 0.21–0.23 s |
| Edit a map → `.3dsx` | < 5 s | 3.21–3.30 s |
| Clean 3DS engine (`nnh_core` + `nnh_platform_3ds`, no ccache) | < 30 s | 11.4 s (11.7 s); the rest (8 examples, NNH Link, host tool) 16.2 s (13.1 s: the platformer is new) |
| 3DS no-op build | — | 2.9 s (1.9 s): `hello` alone 1.8 s, `platformer` 2.8 s (the converter check) |

Release `hello.3dsx` 261,260 → 261,284 bytes (+24 since spec 013's measurement); `platformer.3dsx`
365,632 bytes (romfs with the level and art included). All within budget; a game with maps pays ~1 s on every 3DS
build for the converter's check, and its `.3dsx` edit (≈ 4 s) is the closest to a limit.

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
- [x] Development builds started by 3dslink **without `-s`** no longer hang: the log connection gives up after 1 s and the game runs without logs (fixed and verified on an Old 3DS XL, 2026-11-18)

## M1b — Events, scenes, input
- [x] Event bus + scenes ([spec 001](../specs/001-event-bus-and-scenes/spec.md)): `<nnh/events.hpp>`, `ButtonPressed`/`ButtonReleased`, `<nnh/scene.hpp>` + `nnh::run()`, `examples/scenes` (2026-11-17)
- [x] `examples/scenes` run on a real 3DS (2026-11-17)
- [x] Input ([spec 003](../specs/003-full-input/spec.md)): Circle Pad, C-Stick/ZL/ZR (New 3DS), touch, opt-in Circle Pad → D-Pad mirror, `TouchStarted`/`TouchEnded`; PC keyboard, mouse = touch, gamepad with hot-plug; `examples/input` — verified on PC and an Old 3DS XL (2026-11-18)
- [ ] C-Stick and ZL/ZR on a real New 3DS (and measure the C-Stick range) — maintainer, when one is available

## M2 — 2D graphics
- [x] Textures, sprites, sprite batching, both screens, camera — [spec 004](../specs/004-2d-graphics/spec.md): `<nnh/texture.hpp>` (`IMAGES` in `nnh_add_game`), `gfx::draw`/shapes/layers/`Camera2D`, draw list ready for 3D passes and stereo, `examples/sprites` — verified on PC and an Old 3DS XL (2026-11-18)
- [x] Text with the built-in NNHGE font ([spec 005](../specs/005-text-and-builtin-font/spec.md)): `gfx::draw_text`/`measure_text`, UTF-8 + Latin-1, scale, alignment, shadow; debug text and overlay on the same font — pixel-identical on PC and an Old 3DS XL (2026-11-18), `examples/text`
- [ ] Game fonts (the game's own bitmap fonts, like `IMAGES`) — follow-up spec
- [x] Simulator window ([spec 006](../specs/006-simulator-window/spec.md)): five layouts (F2), scale 1–6× (F3/F4), fit (F5), fullscreen (F9, Alt+Enter), remembered per game; each screen rendered at native resolution and composed — checked by the maintainer (2026-11-18). 3D slider control moves to M8 with stereo rendering

## M3 — Assets
- [x] Assets folder and romfs ([spec 007](../specs/007-assets-and-romfs/spec.md)): `nnh_add_game(... ASSETS <folder>)` checks and converts each file in the build (one step per file, rerun only when it changes), writes a sorted case-sensitive index and packs the folder into the romfs on 3DS / next to the program on PC; `<nnh/assets.hpp>` (`exists`/`size`/`read`/`load` by path); `load_texture("art/hero")` from assets, `IMAGES` kept for embedded images; `examples/sprites` loads its textures and a level file — verified on PC, Linux and an Old 3DS XL (2026-11-18). Cost: `hello.3dsx` +10.5 KB (romfs support)
- [ ] `assetc` tool (a C++ converter with an incremental cache) — only when conversion outgrows the CMake steps (texture formats, atlases, audio). Started by spec 012 (D27): `nnh-assetc`, built for the host in every preset (stage 0), converts Tiled maps (stage 5)

## M4 — Audio
- [x] Audio ([spec 008](../specs/008-audio/spec.md)): `<nnh/audio.hpp>` — `load_sound` (WAV/Ogg Vorbis from assets), voices with volume/pan/pitch/priority, groups + master, streamed `music::play`/`crossfade` with loop points (`LOOPSTART`) and fades, `SoundFinished`/`MusicFinished`, silent fallback without output; 3DS DSP (ndsp) and PC (SDL 3 + software mixer); vendored stb_vorbis; `examples/audio` — verified on PC and an Old 3DS XL (2026-11-18). Cost: every `.3dsx` +8 KB (ndsp), +60 KB with Ogg; builds within budget (clean engine 8.6 s)
- [ ] Later, if games need them: positional audio, effects (reverb, filters), more than one music track (layers), streaming on a thread for long loads

## M5 — Packaging
- [x] `.3dsx` with icon/metadata and an installable `.cia` with banner and jingle ([spec 009](../specs/009-packaging/spec.md)): one identity in `nnh_add_game()` (`TITLE`/`DESCRIPTION`/`AUTHOR`/`VERSION`/`ICON`, NNHGE icon by default, TITLE = PC window title), `CIA UNIQUE_ID ...` with `BANNER`/`BANNER_AUDIO` (NNHGE defaults), every field checked at configure; `<game>_cia`, `<game>_dist`, `dist`; makerom + bannertool fetched pinned on the first `.cia` build only; `nnh install` installs over the network (spec 010); guide [Distributing your game](guides/packaging.md) — `examples/audio` `.cia` installed with FBI and played on an Old 3DS XL (2026-11-18)
- [ ] Later: `SYSTEM_MODE` (more memory, New 3DS 804 MHz) for `.cia`; `nnh package` with the CLI (M6)

## M6 — NNH Link
- [x] The `nnh` tool ([spec 010](../specs/010-nnh-cli-run/spec.md)): `nnh build`, `nnh run` (3dslink-compatible send with its own code, console found on every local network and remembered, coloured log saved in `build/logs/`, `--pc` for the simulator), `nnh install` (`.cia` through FBI) — guide [The nnh tool](guides/nnh-tool.md); verified with `hello` and `audio` on an Old 3DS XL (2026-11-18)
- [x] NNH Link app: discover dev server, list projects, request build, receive and launch — [spec 011](../specs/011-nnh-link/spec.md) (built: the `link/` library, `nnh serve`, the 3DS network and launch extensions and the app `apps/nnh-link`, guide [NNH Link](guides/nnh-link.md); tested on the Old 3DS XL 2026-11-21)
- [x] Pairing code for LAN safety — spec 011

- [x] Console remote: input, screenshots and test scripts from the PC — [spec 013](../specs/013-console-remote/spec.md) (decision D28): `nnh run <game> [--pc] --script <file.nnhs>` presses buttons, moves the sticks, touches, takes PNG screenshots of both screens and checks the log of a Debug/Development game, then exits 0 PASS / 1 FAIL / 2 could not run; without a script, commands typed in `nnh`'s terminal; frame-stamped inputs over the game's own log connection (simulator: loopback, `NNH_REMOTE`), merged in core before input is read; Release has none of it (checked); the reader fuzzed, ASan/UBSan clean on Linux; `examples/input/tests/remote.nnhs` and the platformer's automatic gate (`examples/platformer/tests/gate.nnhs`) pass in the simulator and on an Old 3DS XL with nobody touching it after Y (2026-11-21); screenshot pair 0.6–0.8 s on the console — guides [Testing on the console](guides/testing-on-the-console.md), [the nnh tool](guides/nnh-tool.md#driving-the-game-from-the-pc)
- [ ] Later (spec 013 non-goals): drive games started by NNH Link; record and replay a player's session

## M7 — Game structure
- [x] Tilemaps and collision — [spec 012](../specs/012-tilemaps-and-collision/spec.md) (implemented 2026-11-22): pure `<nnh/collision.hpp>` (box queries, `collision::move` through any grid, one-way cells, no tunnelling; [api/collision.md](api/collision.md)); `<nnh/tilemap.hpp>` — maps built in code or drawn in Tiled (`.tmj` assets converted by `nnh-assetc` at build time, D27, and loaded with `load_tilemap`; bounded and fuzzed readers), objects, properties, animated tiles, a collision grid ([api/tilemap.md](api/tilemap.md), guide [Levels in Tiled](guides/levels.md)); `gfx::draw_tilemap` as tile batches (D26). **Old 3DS XL: 4 full 16×16 layers + 300 sprites at 60 Hz (avg 16.7 ms, worst 17.4 ms, 0 missed of 600); limits 6 layers / 600 sprites.** `examples/platformer`: level 1 from Tiled, the bonus level in code, a replay checked against `tests/level.nnhs`. Still with the maintainer: T7.4 (`tests/level.nnhs` on the Old 3DS); Linux, ASan/UBSan and the three libFuzzer targets ran clean on the build VM
- [ ] Entities/components, scene transitions (scenes, saves and tweens: done; tweens' console check pending)

### Features in the spirit of Godot and Unity

What makes those engines productive, adapted to NNHGE's rules: no RTTI or exceptions, no allocation while
playing, the same on PC and 3DS. **NNHGE has no editor or frontend**: every feature is a C++ API a game can use on
its own, and where data belongs in a file (data assets, maps, animations) the file is plain text written by hand
or comes from a common external tool (Tiled, Aseprite), checked and converted at build time. **Everything here is
optional** (constitution XI, decision D25): a game uses what it wants, mixes it with its own code or ignores it all
and writes plain C++ on the core APIs; no item requires another the game didn't choose. Each item becomes its own
spec; the order below is a proposal.

- [ ] **Data assets** (Unity's ScriptableObjects, Godot's Resources): typed, shared game data (an enemy's stats, an
  item, a level's settings) in their own files in the assets folder — a readable text format written by hand —
  whose shape is a C++ struct the game declares; the build checks every file against it, the game loads them by
  path or name, values change without recompiling, and they can reference each other (an enemy's drop is an item)
  and textures or sounds — [spec 014](../specs/014-data-assets/spec.md), **in progress, stages 0–5 done**
  (2026-11-22: `<nnh/data.hpp>` value types and the `NNH_DATA_*` schema macros, the schema check, fingerprint and
  description, the converted format's validator and apply; the data index `.nnh_data` and `data::load` / `count` /
  `name` / `ref` / `find` / `path` (allocation-free once the index is loaded); `nnh-assetc data`: the TOML-subset
  reader and the converter with `file:line` messages and "did you mean"; all fuzzed. The index step, the build
  integration (`DATA_SCHEMA`) and the example follow)
- [ ] **Tags and groups** (Unity tags, Godot groups): mark entities and objects with names (`"enemy"`, `"coin"`), find
  every one with a tag, cheap to compare (hashed at build time)
- [ ] **Collision layers and masks** (Godot physics layers, Unity layers): which things can touch which (bullets
  ignore their shooter, coins only the player), on top of spec 012's collision
- [ ] **Prefabs** (Unity prefabs, Godot packed scenes): entity templates registered in code by name (a function or a
  set of components and values), spawned many times and overridden per copy; a Tiled object can name one
- [ ] **Scene tree and transforms** (Godot nodes): parent and child entities, local positions, rotation and scale
  that follow the parent (a gun on a ship, a hat on a head)
- [x] **Sprite animation** (Godot AnimatedSprite2D, Unity Animator): named clips declared in code (frames of a
  sprite sheet, timings, loop), events on frames (a footstep sound), and a simple state machine (idle, run, jump);
  optionally read from Aseprite files — [spec 016](../specs/016-sprite-animation/spec.md) (implemented 2026-11-22,
  **console check pending**, T10.2): `<nnh/animation.hpp>` — sprite libraries in one block (grid or trimmed frames,
  clips in loop/once/repeat/ping-pong forward or reverse, frame markers, a state machine with now/at-end/never rules,
  keep-time and next-on-finish), a 64-byte `SpriteAnimator` on the frame clock with queue, speed, flips, notices by
  callback or returned view, no allocation while playing; Aseprite exports (`<name>.ase.json`, D31) converted by
  `nnh-assetc sprite` into `.nnhsprite` and loaded with `load_sprite_library` ([api/animation.md](api/animation.md)).
  `examples/animation`: a gallery of modes, an Aseprite knight on a state machine, a self-check with a golden checksum
  (`tests/check.nnhs`) and a 512-animator bench (PC: 0.04 ms per frame in pc-dev); the platformer's player now draws
  through an animator
- [x] **Tweens and timers** (Godot Tween and Timer, DOTween): animate any value with easing, sequences and
  delays; timers and "wait then do" without hand-written counters —
  [spec 015](../specs/015-tweens-and-timers/spec.md) (implemented 2026-11-22, **console check pending**, T7.2):
  `<nnh/tween.hpp>` — 19 libm-free easings with the same bits everywhere, `nnh::Timer`, tween sets (float, Vec2,
  Color by pointer or setter; delay, loops, yoyo, speed, handles, callbacks or your own finish events, `after`/
  `every`, sequences with joins, waits and calls) on the deterministic frame clock (`Frames`/`FrameTime` in
  `<nnh/time.hpp>`), no allocation while playing, sets bound to a scene updated and cleared by it (D30)
  ([api/tween.md](api/tween.md)). `examples/tweens`: easing gallery, an intro sequence, an overlay scene, a
  self-check with a golden checksum (`tests/check.nnhs`) and a 256-tween bench (PC: 0.06 ms per update)
- [ ] **Input actions** (Godot InputMap, Unity Input System): named actions (`jump`, `attack`) mapped to buttons,
  remappable by the player and saved, instead of fixed buttons in game code
- [ ] **Camera helpers** (Godot Camera2D, Cinemachine): follow with smoothing and a dead zone, map limits, shake,
  zoom, the two screens as one tall world or two views
- [ ] **UI widgets** (Godot Control nodes, Unity UI): labels, buttons, lists, 9-slice panels and layout boxes built
  in code, focus moved with the D-Pad and touch on the bottom screen; menus without drawing rectangles by hand
- [ ] **Particles** (Godot GPUParticles2D/CPUParticles2D, Unity Particle System): emitters configured by a C++
  struct (rate, life, speed, color over time), pooled, within the draw budget
- [ ] **Localization** (Godot `tr()`, Unity Localization): text by key per language, registered in code or read
  from a table in the assets, the system language picked by default; the built-in font covers Latin-1
- [ ] **Save data with versions** (Godot ConfigFile/Resource saving): structs saved and loaded through `nnh::save`
  with a version number and migration from older saves
- [ ] **Pathfinding on grids** (Godot AStarGrid2D): A* over a tilemap's walkable cells, bounded and without
  allocation while searching
- [ ] **Raycasts and shape queries** (Godot RayCast2D, Unity Physics2D.Raycast): line of sight, "what's under
  this point", against tiles and boxes, filtered by layers
- [ ] **Object pools** (Unity ObjectPool): fixed-size pools for bullets, particles and enemies, with handles that
  detect stale use
- [ ] **Live values in the simulator** (in place of Unity's Inspector, without an editor): an API to watch and tweak
  a game's values on the debug overlay (`watch`, sliders driven by buttons), pause and step one frame
- [ ] **Hot reload of assets in the simulator** (both engines): a changed image, map or data file reloads in the
  running game; with "hot reload of game code" below, the fastest edit loop

## M8 — 3D
- [ ] citro3d meshes, materials, stereoscopic rendering, model import in `assetc`

## Later
- [ ] Hot reload of game code in the simulator
- [ ] Profiler overlay, memory tracker
- [ ] Second platform (Switch)
