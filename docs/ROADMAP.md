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

Spec 013 (2026-11-21, same machine; before = commit `2ec6270`, the approved plan without remote code, after = the
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

Spec 014 finished (2026-11-22, same machine; data assets, `DATA_SCHEMA`, `examples/data`; another agent was building
in parallel, so 3DS times are noisy — the best of each set is given, slow outliers up to 8 s were machine load):

| Measurement | Budget | Result |
|-------------|--------|--------|
| Edit `examples/hello/main.cpp` → simulator | < 2 s | 1.63–1.77 s (hello's own steps unchanged since T0.1) |
| Edit `examples/hello/main.cpp` → `.3dsx` | < 5 s | 2.86–3.12 s |
| Edit a data file's value (`goblin.enemy` hp) → simulator | < 2 s | 0.82 s: its data step and the asset index, no C++ |
| Edit a data file → `.3dsx` | < 5 s | 3.68 s: the data step, the asset index, the repack (and `nnh-assetc`'s up-to-date check) |
| Edit a data file's comment | — | 0.65 s (PC): its step only, the output is unchanged |
| Edit the schema source, schema unchanged / changed → simulator | — | 2.7 s / 3.3–3.4 s (one compile, two links; changed: every data file reconverted) |
| Same → `.3dsx` | — | 6.6 s / 7.7 s: the console object and the host program both rebuild |
| Clean 3DS engine (`nnh_core` + `nnh_platform_3ds`, no ccache) | < 30 s | 13.9 s (145 steps: specs 014 and 015 add sources) |
| 3DS no-op build | — | 1.7 s for `hello`; 3.0 s for `data` (`nnh-assetc`'s check; the schema program doesn't run) |

Release `hello.exe` 277,503 bytes and `hello.3dsx` 261,464, unchanged by spec 014's stages 6–9 (the `hello.elf`
sections equal a build of the previous commit; the growth since T0.1 came with spec 015), with no data symbol;
`data.3dsx` 502,680 bytes.

Spec 018 (camera helpers, zoom in `Camera2D`), 2026-11-22, same machine: Release `hello.elf` text 216,484 →
216,748 (+264: the zoom in the draw list and the tile path's chunk loop; budget 320), data +24 (the two cameras);
`hello.exe` text 130,024 → 130,732 (+708, over the PC budget of 512: x86-64 code and 16-byte function alignment; accepted by the maintainer);
no camera symbol in either. Edit `hello` → simulator 1.5–1.6 s, → `.3dsx` 2.9 s; `<nnh/camera.hpp>` 956
preprocessed lines and 0.30–0.33 s alone (`<nnh/gfx.hpp>` 779 lines, 0.31–0.33 s). Details: plan 018, "Stage 0
results" and "Stage 10 results".

Spec 019 (collision layers and queries), 2026-11-23, same machine (devkitARM updated since spec 018: Release
`hello.elf` text 201,188 at its stage 0): `hello` unchanged in every preset (no query symbol); edit `hello` →
simulator 1.5–1.6 s, → `.3dsx` 2.8 s; clean `nnh_core` 5.3 → 5.7 s (21 new files); `<nnh/collision.hpp>` 151 → 691
preprocessed lines (`<cstdint>`), 0.28–0.34 s alone. Details: plan 019, "Stage 0 results" and "Stage 11 results".

Spec 023 (scene transitions), 2026-11-24, same machine: Release `hello.elf` text 201,188 → 201,276 (+88 of the 96-byte
budget: the input filter's check and raw engine buttons, 56; the scene stack's transition hook in `scenes_shutdown`,
32), its `nm` list differing only by the input path's accessors and the hook pointer; `scenes` +144 bytes; no
transition or input-block symbol in either. `<nnh/transition.hpp>` 8,476 preprocessed lines, 0.43 s alone;
`<nnh/ease.hpp>` 461 lines. Details: plan 023, "Stage 0 results" to "Stage 8 results".

Spec 032 (game projects outside the repository), 2026-11-27, same machine, three edits each: in the repository, edit
`hello` → simulator 1.91–2.22 s before, 2.02–2.09 s after (pc-debug); → `.3dsx` 3.57–3.81 s before, 3.53–3.62 s after
(3ds-dev); clean engine (`nnh_core` + platform) pc-release 19.4 → 19.1 s (ccache), 3ds-release 48.1 → 47.2 s; fresh
configure pc-debug 25.6 → 19.9 s, 3ds-dev 24.4 → 23.1 s; `hello.3dsx` byte-identical. In a game project outside the
repository (78-character path, the engine as a pinned archive or a local checkout): edit a hello-sized game → simulator
1.94–2.94 s (the repository's `hello` 1.86–2.69 s in the same series), → `.3dsx` 3.57–3.62 s; configure fresh 17.5 s
(local engine) / 22.0 s (archive), again 1.8 / 2.8 s; the consumer ctest 76 s cold, 23 s warm. The simulator's < 2 s is
borderline on this machine for the repository's own `hello` too (it was 1.6–1.8 s in the spike, 2026-11-27 morning).
Details: plan 032, "Stage 0 results" to "Stage 6 results".

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
- [x] CPU-fault crash screen on 3DS with Luma3DS developer UNITINFO — passed on the Old 3DS XL (2026-11-22,
  `examples/foundation/tests/cpu_fault.nnhs`; it also fixed a black crash screen under `nnh run`)
- [x] Development builds started by 3dslink **without `-s`** no longer hang: the log connection gives up after 1 s and the game runs without logs (fixed and verified on an Old 3DS XL, 2026-11-18)

## M1b — Events, scenes, input
- [x] Event bus + scenes ([spec 001](../specs/001-event-bus-and-scenes/spec.md)): `<nnh/events.hpp>`, `ButtonPressed`/`ButtonReleased`, `<nnh/scene.hpp>` + `nnh::run()`, `examples/scenes` (2026-11-17)
- [x] `examples/scenes` run on a real 3DS (2026-11-17)
- [x] Input ([spec 003](../specs/003-full-input/spec.md)): Circle Pad, C-Stick/ZL/ZR (New 3DS), touch, opt-in Circle Pad → D-Pad mirror, `TouchStarted`/`TouchEnded`; PC keyboard, mouse = touch, gamepad with hot-plug; `examples/input` — verified on PC and an Old 3DS XL (2026-11-18)
- [ ] C-Stick and ZL/ZR on a real New 3DS (and measure the C-Stick range) — **open for anyone with a New 3DS / New 2DS**
  (the maintainer has only an Old 3DS XL; 2026-11-22). `examples/input` shows both; a `.nnhs` script can drive them
  (`nnh` refuses C-Stick/ZL/ZR commands on an Old 3DS, so the same script needs the New model).

## M2 — 2D graphics
- [x] Textures, sprites, sprite batching, both screens, camera — [spec 004](../specs/004-2d-graphics/spec.md): `<nnh/texture.hpp>` (`IMAGES` in `nnh_add_game`), `gfx::draw`/shapes/layers/`Camera2D`, draw list ready for 3D passes and stereo, `examples/sprites` — verified on PC and an Old 3DS XL (2026-11-18)
- [x] Text with the built-in NNHGE font ([spec 005](../specs/005-text-and-builtin-font/spec.md)): `gfx::draw_text`/`measure_text`, UTF-8 + Latin-1, scale, alignment, shadow; debug text and overlay on the same font — pixel-identical on PC and an Old 3DS XL (2026-11-18), `examples/text`
- [x] Game fonts — [spec 029](../specs/029-game-fonts-and-localization/spec.md) (see "Game fonts and localization" below)
- [x] Simulator window ([spec 006](../specs/006-simulator-window/spec.md)): five layouts (F2), scale 1–6× (F3/F4), fit (F5), fullscreen (F9, Alt+Enter), remembered per game; each screen rendered at native resolution and composed — checked by the maintainer (2026-11-18). The 3D slider (F6/F7) and eye view (F8) came with spec 026b

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

- [x] Console remote: input, screenshots and test scripts from the PC — [spec 013](../specs/013-console-remote/spec.md) (decision D28): `nnh run <game> [--pc] --script <file.nnhs>` presses buttons, moves the sticks, touches, takes PNG screenshots of both screens and checks the log of a Debug/Development game, then exits 0 PASS / 1 FAIL / 2 could not run; without a script, commands typed in `nnh`'s terminal; frame-stamped inputs over the game's own log connection (simulator: loopback, `NNH_REMOTE`), merged in core before input is read (a script's input is exclusive since the 2026-11-24 amendment: the console's own controls are ignored, a control not at rest is reported); Release has none of it (checked); the reader fuzzed, ASan/UBSan clean on Linux; `examples/input/tests/remote.nnhs` and the platformer's automatic gate (`examples/platformer/tests/gate.nnhs`) pass in the simulator and on an Old 3DS XL with nobody touching it after Y (2026-11-21); screenshot pair 0.6–0.8 s on the console — guides [Testing on the console](guides/testing-on-the-console.md), [the nnh tool](guides/nnh-tool.md#driving-the-game-from-the-pc)
- [ ] Later (spec 013 non-goals): drive games started by NNH Link; record and replay a player's session

## M7 — Game structure
- [x] Tilemaps and collision — [spec 012](../specs/012-tilemaps-and-collision/spec.md) (implemented 2026-11-22): pure `<nnh/collision.hpp>` (box queries, `collision::move` through any grid, one-way cells, no tunnelling; [api/collision.md](api/collision.md)); `<nnh/tilemap.hpp>` — maps built in code or drawn in Tiled (`.tmj` assets converted by `nnh-assetc` at build time, D27, and loaded with `load_tilemap`; bounded and fuzzed readers), objects, properties, animated tiles, a collision grid ([api/tilemap.md](api/tilemap.md), guide [Levels in Tiled](guides/levels.md)); `gfx::draw_tilemap` as tile batches (D26). **Old 3DS XL: 4 full 16×16 layers + 300 sprites at 60 Hz (avg 16.7 ms, worst 17.4 ms, 0 missed of 600); limits 6 layers / 600 sprites.** `examples/platformer`: level 1 from Tiled, the bonus level in code, a replay checked against `tests/level.nnhs`. Still with the maintainer: T7.4 (`tests/level.nnhs` on the Old 3DS); Linux, ASan/UBSan and the three libFuzzer targets ran clean on the build VM
- [x] Scene transitions (spec 023, implemented; Old 3DS runs 1 and 2: logic and doors PASS; after the T10.3
  optimisation round the bench budgets were set from run 2 on 2026-11-25: fade/wipe 0.11, split 0.12, blinds 0.19,
  iris 1.05, loading 0.11 ms): `<nnh/transition.hpp>` —
  replace/push/pop/cover through a fade, wipe, split, blinds, iris or a custom effect, on one or both screens or one
  tall area, a loading step while covered, input blocked (`input::set_blocked` too), whole frames, self-check
  `BFE1D810`; the platformer's doors close an iris on the player ([api/transition.md](api/transition.md))
- [x] Entities/components — [spec 020](../specs/020-entities/spec.md), **implemented 2026-11-23**, passed on the Old 3DS and
  on Linux (GCC 13/14, ASan, 10 min fuzz): `<nnh/entities.hpp>` — a `World` in one block, the game's plain structs as components in
  stable-slot arrays per kind, 32-bit generational handles that detect stale use, `each<A, B...>` in the first kind's
  slot order, destroy anywhere with remove hooks (D35); storage views read in place by spec 019's box views
  ([api/entities.md](api/entities.md)); `examples/entities` (showcase, a 100 000-operation self-check, the AC-7
  bench); the platformer's coins, turrets and sparkles are entities

### Features in the spirit of Godot and Unity

What makes those engines productive, adapted to NNHGE's rules: no RTTI or exceptions, no allocation while
playing, the same on PC and 3DS. **NNHGE has no editor or frontend**: every feature is a C++ API a game can use on
its own, and where data belongs in a file (data assets, maps, animations) the file is plain text written by hand
or comes from a common external tool (Tiled, Aseprite), checked and converted at build time. **Everything here is
optional** (constitution XI, decision D25): a game uses what it wants, mixes it with its own code or ignores it all
and writes plain C++ on the core APIs; no item requires another the game didn't choose. Each item becomes its own
spec; the order below is a proposal.

- [x] **Data assets** (Unity's ScriptableObjects, Godot's Resources): typed, shared game data (an enemy's stats, an
  item, a level's settings) in their own files in the assets folder — a readable text format written by hand —
  whose shape is a C++ struct the game declares; the build checks every file against it, the game loads them by
  path or name, values change without recompiling, and they can reference each other (an enemy's drop is an item)
  and textures or sounds — [spec 014](../specs/014-data-assets/spec.md), **implemented 2026-11-22**; the Old 3DS
  check (same checksum as the simulator) and the Linux sanitizer/fuzz runs passed: `<nnh/data.hpp>` (`NNH_DATA_TYPE`/`STRUCT`/`ENUM`/`FIELD` over plain
  structs; `data::load` / `count` / `name` / `ref` / `find` / `path`, allocation-free once the index is loaded),
  `nnh_add_game(... DATA_SCHEMA ...)` (a per-game schema program, decision D29), a TOML-subset text format checked
  by `nnh-assetc data` and `data-index` with `file:line: reason` and "did you mean" (one fixture per rule), a
  layout-independent binary validated on load; all parsers fuzzed ([api/data.md](api/data.md)). `examples/data`: a
  bestiary and shop with a self-check whose checksum the console must match (`examples/data/tests/check.nnhs`,
  `nnh: PASS` in the simulator). Editing a data file runs no C++ compile: 0.8 s to the simulator, 3.7 s to the
  `.3dsx` (table above)
- [x] **Tags and groups** (Unity tags, Godot groups): mark entities and objects with names (`"enemy"`, `"coin"`), find
  every one with a tag, cheap to compare (hashed at build time) — [spec 020](../specs/020-entities/spec.md),
  **implemented 2026-11-23**, passed on the Old 3DS: `nnh::tag("enemy")` (FNV-1a, `constexpr`, a `case` label),
  `tags_distinct`, up to 8 tags per entity, `find`/`first`/`each_tagged` (Godot's groups)
  ([api/entities.md](api/entities.md#tags-and-groups))
- [x] **Collision layers and masks** (Godot physics layers, Unity layers): which things can touch which (bullets
  ignore their shooter, coins only the player), on top of spec 012's collision —
  [spec 019](../specs/019-collision-layers-and-queries/spec.md), **implemented 2026-11-23**; Old 3DS check passed:
  `collision::Layers` (32 bits, the game's own enum), `detects(mask, layers)`, box views over the game's own arrays
  (structs by member pointer, no copies), `QueryFilter` (mask, ignore index); tilemap layer sets on `collision` layers
  and solid/one-way tiles, from code or a Tiled `collision_layers` flag enum, and `Tilemap::collision_grid(mask)` (D34:
  a runtime area after the map's bytes; `collision_grid()` and the `.nnhmap` format unchanged)
  ([api/collision.md](api/collision.md#layers-and-masks), [api/tilemap.md](api/tilemap.md#collision-layers)). The
  platformer's coins are taken by a layered `boxes_overlapping` query and its player walks a filtered grid (replay
  lines unchanged by the swap)
- [x] **Prefabs** (Unity prefabs, Godot packed scenes): entity templates registered in code by name (a function or a
  set of components and values), spawned many times and overridden per copy; a Tiled object can name one —
  [spec 021](../specs/021-prefabs/spec.md), **implemented 2026-11-23**, passed on the Old 3DS (spawn 1.63 ms for 256, map 3.56 ms for 512 objects): a `PrefabLibrary` the
  game owns (one block), prefabs as component values + tags + a placement member + a spawn hook, variants flattened at
  registration, `spawn`/`spawn_with` bit for bit what hand-written `spawn`/`add`/`add_tag` make (all or nothing
  through one entities-internal reservation, D36), per-copy overrides and positions, Tiled objects naming prefabs by
  type or a `prefab` property (unknown explicit names a loud level error), one prefab per data asset by pattern
  ([api/prefabs.md](api/prefabs.md), [levels guide](guides/levels.md#placing-prefabs-in-tiled), demo
  [`examples/prefabs`](../examples/prefabs/main.cpp)). The platformer's coins, turrets and sparkles are prefabs (replay
  lines unchanged)
- [x] **Scene tree and transforms** (Godot nodes): parent and child entities, local positions, rotation and scale
  that follow the parent (a gun on a ship, a hat on a head) — [spec 022](../specs/022-scene-tree-and-transforms/spec.md)
  (implemented 2026-11-24; console and Linux checks passed, budgets set). Named hierarchy, nodes and transforms in code (a "scene" is a
  game mode): `<nnh/transform.hpp>` (deterministic values: composition, points, inverses, bounds, an exact-quarter-turn
  sine), `<nnh/hierarchy.hpp>` (the engine's `Node` component kind, attach/detach keeping world or local, one pass per
  frame, `world_now`, cascading destroys), prefabs with children, `gfx::draw` of a world transform; demo
  `examples/hierarchy`; the platformer's turrets have barrel children ([hierarchy.md](api/hierarchy.md),
  [transform.md](api/transform.md))
- [x] **Sprite animation** (Godot AnimatedSprite2D, Unity Animator): named clips declared in code (frames of a
  sprite sheet, timings, loop), events on frames (a footstep sound), and a simple state machine (idle, run, jump);
  optionally read from Aseprite files — [spec 016](../specs/016-sprite-animation/spec.md) (implemented 2026-11-22;
  Old 3DS check passed, 512 animators 0.626 ms): `<nnh/animation.hpp>` — sprite libraries in one block (grid or trimmed frames,
  clips in loop/once/repeat/ping-pong forward or reverse, frame markers, a state machine with now/at-end/never rules,
  keep-time and next-on-finish), a 64-byte `SpriteAnimator` on the frame clock with queue, speed, flips, notices by
  callback or returned view, no allocation while playing; Aseprite exports (`<name>.ase.json`, D31) converted by
  `nnh-assetc sprite` into `.nnhsprite` and loaded with `load_sprite_library` ([api/animation.md](api/animation.md)).
  `examples/animation`: a gallery of modes, an Aseprite knight on a state machine, a self-check with a golden checksum
  (`tests/check.nnhs`) and a 512-animator bench (PC: 0.04 ms per frame in pc-dev); the platformer's player now draws
  through an animator
- [x] **Tweens and timers** (Godot Tween and Timer, DOTween): animate any value with easing, sequences and
  delays; timers and "wait then do" without hand-written counters —
  [spec 015](../specs/015-tweens-and-timers/spec.md) (implemented 2026-11-22; Old 3DS check passed: same checksum as the PC, 256 tweens 0.71 ms):
  `<nnh/tween.hpp>` — 19 libm-free easings with the same bits everywhere, `nnh::Timer`, tween sets (float, Vec2,
  Color by pointer or setter; delay, loops, yoyo, speed, handles, callbacks or your own finish events, `after`/
  `every`, sequences with joins, waits and calls) on the deterministic frame clock (`Frames`/`FrameTime` in
  `<nnh/time.hpp>`), no allocation while playing, sets bound to a scene updated and cleared by it (D30)
  ([api/tween.md](api/tween.md)). `examples/tweens`: easing gallery, an intro sequence, an overlay scene, a
  self-check with a golden checksum (`tests/check.nnhs`) and a 256-tween bench (PC: 0.06 ms per update)
- [x] **Input actions** (Godot InputMap, Unity Input System) — spec 017, implemented (Old 3DS check passed, 64 actions × 4 slots 0.142 ms):
  `<nnh/actions.hpp>`, button/axis/vector actions with several bindings (buttons, stick directions, sticks,
  composites, touch areas), contexts bound to scenes with no leaking presses, queries and `ActionPressed` events in the
  same frame (D32 after-input hook: 16 bytes in `hello` on the 3DS), listen/conflicts/reset for a controls screen,
  overrides saved with `nnh::save` (`NNHB`, fuzzed), Old 3DS fallbacks ([api/actions.md](api/actions.md)).
  `examples/actions`: playfield, controls screen, bench (PC: 64 actions × 4 slots in 0.026 ms) and a self-check
  script (`tests/check.nnhs`)
- [x] **Camera helpers** (Godot Camera2D, Cinemachine) — [spec 018](../specs/018-camera-helpers/spec.md), implemented;
  Old 3DS check passed (2 camera pairs well under 0.05 ms; zoom 0.5: 2 full layers at 60 Hz, zoom 2: 12): `<nnh/camera.hpp>`, a `CameraRig` value per view that follows a point or a box with an
  offset, a dead zone in screen pixels and per-frame smoothing, stays inside limits (`Tilemap::bounds()`), shakes from
  seeded trauma, zooms about its centre, drives one screen or both as one tall world (two rigs = two views), converts
  screen ↔ world points and draws its dead zone and limits; frame-deterministic, 128 bytes, no allocation
  ([api/camera.md](api/camera.md)). `Camera2D::zoom` in the renderer (D33: sprites, text, shapes, seamless tile layers
  with snapped edges, row chunks below zoom 1; zoom 1 byte-identical) ([api/gfx.md](api/gfx.md#zoom)).
  `examples/camera`: playground, self-check (`tests/check.nnhs`), bench (PC: 0.0013 ms per pair of rigs); the
  platformer follows its player with the rig (dead zone, smoothing on y, look-ahead, landing shake) and its bench
  zooms (B; `tests/zoom_gate.nnhs`)
- [x] **UI widgets** (Godot Control nodes, Unity UI) — [spec 028](../specs/028-ui-widgets/spec.md), **implemented
  2026-11-25, console and Linux checks passed, budgets set** (Old 3DS: relayout < 0.93 ms, list 0.051 ms, draw 1.13×): `<nnh/ui.hpp>` — a retained widget
  tree built in code in one block the game owns (88 B per widget on the 3DS): labels (wrap, `...`, typewriter reveal),
  images, progress bars, solid or 9-slice panels, buttons, toggles, sliders, choices, virtualised lists and grids
  (a row pool filled by the game only for rows coming into view; 65 535 items), VBox/HBox/grid box/stack/scroll box with
  stretch ratios and nine-point anchors on both screens, integer layout only when dirty; focus by D-Pad and Circle Pad
  with repeat, wrap on a fresh press, neighbours, a geometric fallback, disabled-skipped or inert entries, default and
  remembered focus, modals and a prompt helper; touch on the bottom screen; results by query, callbacks or opt-in bus
  events; text sources (copy, borrowed, keys through a resolver: the localization seam); themes and per-state styles;
  drawn through public gfx calls on layer 14; scene binding (D30); an optional `ActionSet` adapter
  ([api/ui.md](api/ui.md), guide [menus](guides/menus.md)). `examples/ui`: title, options slid in by a tween, a 300-item
  inventory, a gallery, a dialog, the quit prompt, a HUD, two themes, the self-check (`ui: PASS 30400DA6`) and the bench
  (PC: idle 0.002 ms, relayout 0.048 ms, draw ratio 0.91); `tests/check.nnhs` passes with `--pc`; `hello` unchanged
- [x] **Particles** (Godot GPUParticles2D/CPUParticles2D, Unity Particle System) — [spec 024](../specs/024-particles/spec.md),
  **implemented 2026-11-24**, Old 3DS runs 1 and 2 done (self-check `C5EF1A84` PASS; after the T10.3 round the budgets
  were set from run 2 on 2026-11-25: update 1024 0.8 ms, emit 512 2.35 ms, off-screen 0.06 ms, draw ratio ≤ 0.75): `<nnh/particles.hpp>` — game-owned emitters in one block
  (48 B per particle) configured by one plain `ParticleDesc`: rate, bursts, one-shots, `emit`/`emit_at`; point, box,
  circle and line shapes, cone or outward directions; gravity, damping, spin; colour and size keys over life; sprite
  frames; world or local space through spec 022's transforms with sub-frame spreading; a seeded counter-based
  generator, the same bits on PC and 3DS (golden `C5EF1A84`); scene binding (D30); one particle batch per draw in the
  draw list, expanded at submit under `AppConfig::gfx.max_particles` (D39) ([api/particles.md](api/particles.md)).
  `examples/particles`: gallery, touch bursts, a text-file preset, self-check and bench; the platformer gets landing
  dust, bullet puffs and muzzle flashes with its replay lines unchanged
- [x] **Game fonts and localization** (Godot `tr()` and FontFile, Unity Localization and TextMeshPro) —
  [spec 029](../specs/029-game-fonts-and-localization/spec.md), **implemented 2026-11-26, console and Linux checks
  passed, budgets set** (decisions D49, D50): `<nnh/font.hpp>` — fonts baked by the build from TrueType/OpenType descriptions (one
  size, named character sets, the characters of chosen languages' strings, outline, fallbacks, a packed licence) or
  BMFonts into 4-bit coverage pages ≤ 1024, drawn by `gfx::draw_text` through `TextOptions::font` with kerning,
  wrapping (CJK and kinsoku), line limits with `…`, outlines, `measure_text`/`measure_chars`, the same layout on PC and
  3DS ([api/fonts.md](api/fonts.md)); `<nnh/localization.hpp>` — per-language string files checked by the build
  (keys, placeholders, CLDR plural forms, glyph coverage, the `qps` pseudo-language), `tr`/`format` without allocation,
  the system language followed or the player's choice, switching at the next frame with `LanguageChanged`, role fonts
  per language, spec 028's widgets through `loc::apply` ([api/localization.md](api/localization.md), guide
  [localizing a game](guides/localizing-a-game.md)). `examples/localization`: en, pt-BR, de, ru, ja (a Noto Sans JP OFL
  subset), self-check `21809981` (pc-debug = pc-release), bench, `tests/check.nnhs` passes with `--pc`; `hello` unchanged
- [x] **Save data with versions** (Godot ConfigFile/Resource saving): structs saved and loaded through `nnh::save`
  with a version number and migration from older saves — [spec 027](../specs/027-save-versions/spec.md),
  **implemented 2026-11-24**, Linux checks passed; Old 3DS run 1 (2026-11-25): every golden equal to the PC's, the card
  workloads within target, encode 0.52 ms and decode 1.44 ms over target; after the T10.3 optimisation round (65–77 %
  fewer instructions) run 2 measured encode 0.20 ms and decode 0.38 ms, and the budgets were set: `<nnh/saves.hpp>` — `NNH_SAVE_TYPE`/`STRUCT`/`SUMMARY`
  with `NNH_SAVE_FIELD(member, id)`, retired ids, `NNH_SAVE_VERSION` and compile-time-complete `NNH_SAVE_STEP` chains
  (D45, D46); a self-describing little-endian format (tagged fields, packed sequences, one template per list of
  structs: 5 % framing) inside spec 002's `NNHS` container, the same bytes on PC and 3DS (golden v1–v3 files); the
  whole file validated before the struct changes, exact conversions (added, removed, renamed, widened fields load
  without code), a report of unknown/dropped/adjusted values, the backup on any damage, power cuts tested after every
  file operation; `OldFields` for migration steps, summaries for "Load game" screens, a Debug text dump and
  `NNH_SAVE_DUMP`; no allocation; fuzzed. [saves.md](api/saves.md); `examples/saves` (tiny game, slots, settings,
  check and bench modes, `tests/check.nnhs`)
- [x] **Pathfinding on grids** (Godot AStarGrid2D): A* over a tilemap's walkable cells, bounded and without
  allocation while searching — [spec 025](../specs/025-grid-pathfinding/spec.md), **implemented 2026-11-24**, Linux checks passed, passed on
  the Old 3DS after one optimisation round (A* 4.05 µs per expanded cell, `next` for 256 agents 0.17 ms): `<nnh/pathfinding.hpp>` — path maps baked from any `collision::Grid` (blocked cells and weights
  1–255 from a tile property, agent sizes 1–4, region syncs for doors, a version counter), integer A* searchers with a
  fixed tie order (the same cells on PC and 3DS: golden `1C0B5992`), expansion limits, partial paths, time slicing and
  stale flags, smoothing into waypoints (spec 019's exact walk in integers), double-buffered distance fields for crowds
  with `next`, a squared-distance path follower; nothing allocates after creation; optional (D40). Demo
  `examples/pathfinding` (a keep made in Tiled, a hero, 32 chasers, doors, the island, a 2 × 2 hero, debug views,
  self-check and bench) ([api/pathfinding.md](api/pathfinding.md))
- [x] **Raycasts and shape queries** (Godot RayCast2D, Unity Physics2D.Raycast): line of sight, "what's under
  this point", against tiles and boxes, filtered by layers — [spec 019](../specs/019-collision-layers-and-queries/spec.md),
  **implemented 2026-11-23**; Old 3DS check passed: rays from → to against grids (an exact Amanatides & Woo walk:
  no tunnelling at any length, no leak between diagonal walls, one-way cells from above) and box views (slab test),
  first hit / all hits / `cells_crossed`, `line_of_sight`, box casts (two movers by relative motion), `boxes_at`,
  `boxes_overlapping`, `cell_at`; the same bits on every system (exact `double` decisions, results rounded once;
  checked against an integer reference on 10 000 rays), no allocation ([api/collision.md](api/collision.md#rays)).
  `examples/queries`: a room with water and planks, rays in three modes, mask toggles, a touch asking what is under it,
  a self-check (`tests/check.nnhs`) and a bench (PC Debug: 128 rays + 64 boxes ≈ 1 ms per frame, point queries
  0.05 µs per box). The platformer's level 1 got two turrets with line of sight whose bullets move by rays (in the
  replay's checksum)
- [x] **Object pools** (Unity ObjectPool): fixed-size pools for bullets, particles and enemies, with handles that
  detect stale use — [spec 020](../specs/020-entities/spec.md), **implemented 2026-11-23**, passed on the Old 3DS:
  `<nnh/pool.hpp>` — `Pool<T, N>` (no allocation) and `Pool<T>::create(n)` (one block), lowest free slot, released
  slots quarantined until a flush, bound to a scene through D30 ([api/pool.md](api/pool.md)); the platformer's
  bullets and the example's ship use them
- [x] **Live values in the simulator** (in place of Unity's Inspector, without an editor; spec 030, implemented,
  console and Linux checks passed, budgets set): `<nnh/live.hpp>` — tunables registered by pointer (float, int, bool, colour,
  enums, actions) and watches (registered or per frame), the overlay's Tunables and Watches pages edited by buttons and
  touch on the PC and the 3DS, pause and step one frame in core, `nnh` commands and `.nnhs` script commands (`set`,
  `expect-value`, `pause`, `step`), dumps and `--tunes`; nothing in Release ([api/live.md](api/live.md),
  [guide](guides/tuning-your-game.md), `examples/live`, the platformer's movement numbers)
- [x] **Hot reload of assets in the simulator and on the console** (both engines) — [spec 031](../specs/031-hot-reload-assets/spec.md): `nnh run <game> [--pc]` watches the assets folder, rebuilds the asset target and sends the changed outputs as one batch, applied whole at one point of a frame; textures, maps, data, strings, fonts, sounds, music, meshes and sprite libraries (animators rebind) follow behind their handles, data and maps are announced (`assets::reloaded`, `AssetsReloaded`), tunables on reloaded data follow the file; script lines `swap-asset`/`restore-asset`/`expect-reload`; demo `examples/reload`, guide [editing assets live](guides/editing-assets-live.md). Implemented 2026-11-27; Linux checks and console runs 1 and 2 passed; save → frame on the Old 3DS 3.0–3.3 s in run 1, the 3DS asset build then cut from ≈ 2.9 s to ≈ 1.6 s.

## M8 — 3D
- [x] citro3d meshes, materials, stereoscopic rendering, model import in `assetc` — [spec 026](../specs/026-3d-foundation/spec.md)
  (3D foundation): **part 026a (mono) built 2026-11-24**, console run 1 done (math bit-identical; ≈ 100 µs of frame per
  small draw, so a per-draw optimisation round followed: T10.5), console run 2 done 2026-11-25 (256 small draws 28.3 ms:
  CPU ≈ 12 ms then GPU ≈ 16 ms, one after the other; overlapping them was tried and dropped, D47 frees released
  textures and meshes only after the GPU wait), budgets set (256 draws < 35.5 ms a frame), console pixel check PASS after two backend fixes (read-back
  timing, 3D textures' v), Linux checks (T9.3) passed 2026-11-25; **part 026b (stereo) implemented
  2026-11-25**, Old 3DS XL run PASS (stereo golden and both eyes' pixels; judged comfortable; 256 draws 51 ms a frame in
  stereo, budgets set) after an exit-freeze fix, Linux checks passed: `system::stereo_slider()`, the top screen drawn once per eye (off-axis, focal plane
  at screen depth, culling union, 2D and debug pass in both eyes), simulator F6/F7/F8 (left, right, anaglyph), the
  `.nnhs` `slider` command and `screenshot NAME eyes`, stereo golden `B3550D40`. `<nnh/math3d.hpp>` (same bits on PC and 3DS, golden `A3BF4EDF`),
  `<nnh/mesh.hpp>` (code-built meshes, primitives, glTF 2.0 → `.nnhmesh` by `nnh-assetc mesh`), `<nnh/gfx3d.hpp>`
  (camera, ambient + 4 lights, draws between the 2D layers, culling, order, `to_screen`/`screen_ray`, probe pixels);
  the simulator's CPU rasteriser (D41) and the citro3d backend; `examples/models` (showcase, check, bench, smoke).
  A 2D game links no 3D code (`hello` 3DS `.text` +272 B: the hook and the shared 2D slice function).

## After 1.0 (planned 2026-11-26)

1. **First release and a small sample game.** Cut a release right after 1.0, then build one simple, complete game with
   it from scratch (`nnh new`, the docs, the release download), as a test of the whole path a newcomer takes; what
   it finds becomes fixes and docs.
2. **M9 — 3DS hardware features.** Interfaces that cover every 3DS feature a homebrew game can use. Each one follows
   the usual path (spec → plan → tests → PC simulator stand-in → console check) and is exposed through
   `nnh::system::has(Capability::...)` so games can test for it:
   - [ ] Cameras: the two outer cameras (stereo pair) and the inner one, frames as textures, photo capture
   - [ ] Motion sensors: accelerometer and gyroscope (tilt, shake, rotation), with calibration
   - [ ] Microphone: input level and recording (blowing, voice); PC stand-in from the computer's microphone
   - [ ] Networking: internet sockets and HTTP(S) requests (`nnh_link` already has the socket layer), local wireless
     multiplayer between consoles (UDS) and Wi-Fi state
   - [ ] NFC (amiibo) on New 3DS, and the NFC reader accessory on Old 3DS
   - [ ] Circle Pad Pro (IR) on Old 3DS, read through the same input API as the New 3DS's C-Stick and ZL/ZR
   - [ ] System state: battery level and charging, Wi-Fi switch, lid closed/sleep and HOME menu events, date and time
     (RTC), step counter, play coins
   - [ ] System applets: software keyboard, Mii selector, error and message dialogs
   - [ ] StreetPass and notifications (only if homebrew support turns out reliable enough)

## Later
- [ ] Hot reload of game code in the simulator
- [ ] Profiler overlay, memory tracker
- [ ] Second platform (Switch)
