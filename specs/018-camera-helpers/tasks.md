# Tasks 018: Camera helpers

Status: approved (2026-11-22, maintainer pre-approved)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The renderer is changed only after the zoom-1 regression test is pinned (T0.2), and the platformer adopts
the rig (stage 3) before the renderer changes, so its unchanged golden lines prove the swap. The only console stage
is stage 9: three scripted runs the maintainer only starts (Homebrew Launcher → Y). Deviations found while building
go into plan.md as "Stage N refinements", as in plans 012–017.

## Stage 0: baseline and the zoom-1 regression

- [x] **T0.1** Baseline numbers (Windows, 3 runs each): `hello.exe` (pc-release, pc-debug) and `hello.elf`/`.3dsx`
  (3ds-release, 3ds-dev) sizes (`size` text/data/bss); edit `examples/hello/main.cpp` → simulator and `.3dsx` build
  times; clean engine build; preprocessed line count and compile time of a one-line TU including `<nnh/gfx.hpp>` (the
  future `camera.hpp` comparison). Files: plan.md ("Stage 0 results"). Covers III, FR-16, AC-8.
- [x] **T0.2** Zoom-1 regression test, written and pinned on today's code: on the fake platform, a frame with
  sprites (sub-pixel and negative positions, scale, negative scale, flips, pivot, rotation, tint/opacity), fill and
  outline rectangles, lines, text with shadow and scale 2, `ignore_camera` draws, and a code-built tilemap (8×8 and
  16×16 tilesets, 3 layers: parallax 0.5, an offset, flips incl. diagonal, an animated tile), at fractional cameras on
  both screens over 3 frames; a hash of every recorded `DrawItem2D` field and every `TileBatch2D`/`TileQuad2D` field
  (named fields, never raw bytes: padding) pinned as golden values. Files: `tests/gfx/test_zoom1_regression.cpp`,
  `tests/CMakeLists.txt`. Test: passes on `main` before any change. Covers FR-9, AC-3.
- [x] **T0.3** [P] Reference screenshots for AC-8 (kept in `build/ref-018/`, not committed): `sprites`, `scenes`,
  `tweens`, `text`, `hello`, platformer `PLATFORMER_DEMO=bench` and level 1 (`NNH_EXIT_AFTER_FRAMES` +
  `NNH_SCREENSHOT`), plus a script `build/ref-018/compare.sh` that rebuilds them and `cmp`s each PNG. Files: plan.md
  (list and frame numbers). Covers AC-8.

## Stage 1: the rig, following and limits (pure)

- [x] **T1.1** Public header (settings, enums, `CameraRig` declarations), the rig's storage and size, construction,
  `set`/`settings`, copy; `static_assert(sizeof(CameraRig) <= 128)`. Files: `engine/core/include/nnh/camera.hpp`,
  `engine/core/src/camera/camera_internal.hpp`, `camera_rig.cpp`, `engine/core/CMakeLists.txt` (sources,
  `-ffp-contract=off` on `src/camera/*`). Test: `tests/camera/test_camera_value.cpp` (size, trivially copyable, copies
  independent, no `init()`). Covers FR-1, FR-15.
- [x] **T1.2** Target and desired position: point and box centre (`x + w / 2`), offset, per-axis follow off, the
  point zone (`c − half − dz.x / zoom`), dead zone edges against the previous desired position, overshoot, asymmetric
  zone, zone larger than the view clamped + logged once. Files: `camera/camera_follow.cpp`. Test:
  `tests/camera/test_camera_follow.cpp`, `test_camera_dead_zone.cpp`. Covers FR-2, FR-3.
- [x] **T1.3** Limits: clamp with `lo`/`hi`, Start/Center/End on small worlds, `set_limits`/`clear_limits`, the final
  clamp rule (followed's own range, not on small-world axes). Files: `camera/camera_limits.cpp`. Test:
  `tests/camera/test_camera_limits.cpp` (part 1: no smoothing). Covers FR-6.
- [x] **T1.4** Smoothing and the update order (steps 1–4, 7 of the plan; no shake yet), fraction 1 as an assignment,
  the 1/64 snap, still target settles bit-exactly, limits changed at run time (glide / cut), `snap()` (centred,
  limits kept), not calling `update` freezes. Files: `camera/camera_update.cpp`, `camera_follow.cpp`. Test:
  `tests/camera/test_camera_smoothing.cpp`, `test_camera_snap.cpp`, `test_camera_limits.cpp` (part 2),
  `test_camera_update.cpp`. Covers FR-4, FR-5, FR-6, FR-12.
- [x] **T1.5** [P] Zoom in the rig: view size, `set_zoom`/`set` keep the centre, dead zone in screen pixels at
  zooms 0.5–4, limits with the zoomed view. Files: `camera_rig.cpp`, `camera_internal.hpp`. Test:
  `tests/camera/test_camera_zoom.cpp`. Covers FR-3, FR-9.
- [x] **T1.6** Stage end: `ctest --preset pc-debug`. Covers VI.

## Stage 2: shake, layouts, conversions, misuse (pure)

- [x] **T2.1** Noise: the integer hash, lattice values in [-1, 1], smoothstep interpolation, 16.16 noise clock;
  golden values for a few (seed, axis, time) triples; continuity bound per tick. Files: `camera/camera_shake.cpp`.
  Test: `tests/camera/test_camera_noise.cpp`. Covers FR-8, FR-12.
- [x] **T2.2** Trauma: `add_trauma` (clamp, NaN/negative misuse), decay to exactly 0, shake from the decayed trauma
  (`trauma() == 0` ⇒ `position() == followed()`), `max_shake / zoom` bound, same/different seeds, `stop_shake`,
  `snap` keeps the shake, shake inside limits / clamp off / small-world axis. Files: `camera_shake.cpp`,
  `camera_update.cpp`. Test: `tests/camera/test_camera_shake.cpp`. Covers FR-5, FR-8.
- [x] **T2.3** Layouts and conversions: Single top/bottom, TallWorld (combined view, bottom at `+ (40 / zoom,
  240 / zoom + gap)`, combined limits and dead zone, a point crossing the screens continuously), `drives`,
  `camera(s)`, `view(s)`, `to_world`/`to_screen` round trips (both screens × both layouts × zooms 0.5, 1, 2, 3),
  pixel snap (applied camera only), layout change keeps the top-left, two rigs as two views. Files:
  `camera/camera_layout.cpp`. Test: `tests/camera/test_camera_layouts.cpp`, `test_camera_convert.cpp`. Covers FR-10,
  FR-11, FR-13.
- [x] **T2.4** [P] Misuse: every case of FR-15 asserts (assert hook); the Release path clamps or ignores and logs one
  error per rig; profile scope `camera` in `update`. Files: `camera_rig.cpp`, `camera_update.cpp`. Test:
  `tests/camera/test_camera_misuse.cpp`. Covers FR-15.
- [ ] **T2.5** Stage end: `ctest --preset pc-debug`; the same on VM 120 (GCC 13/14, clang 18): every camera test,
  noise goldens identical. Covers VI, FR-12.

## Stage 3: applying, debug drawing, the map's bounds, the platformer's swap (zoom 1)

- [x] **T3.1** `apply()`: `gfx::set_camera` on the driven screen(s) with position and zoom (until stage 4 the zoom
  field doesn't exist: `apply` writes the position; T4.1 adds the zoom), other screen untouched, pixel snap. Files:
  `camera/camera_apply.cpp`. Test: `tests/camera/test_camera_apply.cpp` (fake platform). Covers FR-13.
- [x] **T3.2** [P] `debug_draw()`: the inline header function, `detail::camera_debug_draw` (dead zone yellow, limits
  red where visible, target green cross; layer 15, `ignore_camera`; both screens in TallWorld; nothing outside a
  frame). Files: `camera/camera_debug.cpp`, `camera.hpp`. Test: `tests/camera/test_camera_debug.cpp`. Covers FR-14.
- [x] **T3.3** [P] `Tilemap::bounds()`. Files: `engine/core/include/nnh/tilemap.hpp`, `src/tilemap/tilemap.cpp`. Test:
  `tests/tilemap/test_map_bounds.cpp` (code-built, loaded, empty). Covers FR-7.
- [x] **T3.4** Opt-in check: `hello` Release sizes against T0.1 (unchanged: the rig isn't linked) and `nm` (no
  `CameraRig`/`camera_` symbol) on PC and 3DS; a test object using only the camera has no tilemap symbol and the
  tilemap module has no camera symbol. Files: plan.md ("Stage 3 results"). Covers FR-7, FR-16, AC-8.
- [x] **T3.5** Platformer step 1 (identical configuration): `level_camera.{hpp,cpp}` (target the player's box, no
  dead zone, no smoothing, limits `map.bounds()`, Start alignment); `test_level_camera.cpp` compares a frozen copy of
  `follow_camera()` with the rig over a dense sweep of boxes and worlds (incl. a world smaller than the view) bit for
  bit, through `snap` and `update`; `level_play` uses the rig (`snap` in `restart()`, `update` + `apply` in
  `update()`, the sample's camera = `position()`); `camera_follow.{hpp,cpp}` and its old test removed. Files:
  `examples/platformer/level_camera.{hpp,cpp}`, `level_play.{hpp,cpp}`, `CMakeLists.txt`, `tests/platformer/*`,
  `tests/CMakeLists.txt`. Test: `tests/platformer/test_level_camera.cpp`; ctest `platformer_replay_matches_script`
  passes with **unchanged** lines; `nnh run platformer --pc --no-build --script examples/platformer/tests/level.nnhs`
  → PASS. Covers AC-4 step 1.

## Stage 4: zoom in `Camera2D` (sprites, shapes, text)

- [x] **T4.1** `Camera2D::zoom`, `kMin/kMaxCameraZoom`, `set_camera` clamp (assert in Debug/Development, NaN → 1,
  Release silent); `apply()` writes the zoom. Files: `engine/core/include/nnh/gfx.hpp`, `src/gfx/draw2d.cpp`,
  `camera_apply.cpp`. Test: `tests/gfx/test_camera_zoom_clamp.cpp`, `test_camera_apply.cpp` (zoom). Covers FR-9, FR-13.
- [x] **T4.2** `DrawList::build(screen, Camera2D, …)`: positions, sizes and pivots × zoom for sprites and glyphs,
  rectangle corners and line ends; `ignore_camera` at zoom 1; `-ffp-contract=off` on `draw_list.cpp`. Files:
  `src/gfx/draw_list.{hpp,cpp}`, `gfx_frame.cpp`, `engine/core/CMakeLists.txt`. Test:
  `tests/gfx/test_draw_list_zoom.cpp` (zooms 2 and 0.5: expected items; integer zoom = exact multiples; debug pass
  unchanged); `test_zoom1_regression.cpp` and `test_draw_list.cpp` unchanged and passing. Covers FR-9, AC-3.
- [x] **T4.3** Stage end: ctest; AC-8 comparison (`build/ref-018/compare.sh`) for the scenes without tilemaps
  (`sprites`, `scenes`, `tweens`, `text`, `hello`): identical. Covers AC-3, AC-8.

## Stage 5: zoomed tile batches (D33)

- [ ] **T5.1** Contract: `TileBatch2D::dst_w/dst_h`, `kTileOrientWiden/Taller`, comments; `detail::zoomed_tile_size`;
  `gfx_frame.cpp` fills `dst_w/dst_h` (= tile size at zoom 1). Files: `contract/nnh/platform/video.hpp`,
  `src/gfx/tile_batches.{hpp,cpp}`, `gfx_frame.cpp`. Test: `tests/gfx/test_tile_batches.cpp` (dst at zoom 1, 2, 0.5,
  1.5); `test_zoom1_regression.cpp` unchanged except for the new fields (equal to the tile size). Covers FR-9.
- [ ] **T5.2** Edges and visible cells: `zoom_edges` (edge(i) = O + round(i × tile × z), tables ≤ 128 per axis), the
  zoomed parallax centre and origin, visible = cells whose span meets the screen; zoom 1 takes today's code. Files:
  `src/tilemap/zoom_edges.{hpp,cpp}`, `visible_range.{hpp,cpp}`, CMake (+ `-ffp-contract=off`). Test:
  `tests/tilemap/test_zoom_edges.cpp`, `test_visible_range_zoom.cpp` (counts at 0.5/2, parallax 0 and 0.5 layers,
  huge/NaN cameras still empty); `test_visible_range.cpp` unchanged. Covers FR-9, AC-3.
- [ ] **T5.3** Expansion: the zoomed loop (quad `x/y` from the edge tables, widen/taller bits from `edge(i+1) −
  edge(i) − dst`), row resume (`first_row` → `next_row` when the scratch fills at a row boundary; budget drops as
  today); `map_draw.cpp` forwards zoom and rows; `gfx_frame.cpp` loops `expand → draw_tiles` per chunk. Files:
  `src/tilemap/tile_expand.{hpp,cpp}`, `map_draw.cpp`, `src/gfx/gfx_frame.cpp`, `tile_batches.cpp`. Test:
  `tests/tilemap/test_tile_expand_zoom.cpp`, `tests/gfx/test_tile_chunks.cpp` (8×8 tiles at 0.5: several calls,
  rows in order, totals right, no drop with a big budget; a small budget drops as before); `test_tile_expand.cpp`
  and `test_tile_submit.cpp` unchanged. Covers FR-9, II.
- [ ] **T5.4** End to end on the fake platform: full layers at zooms 0.5, 0.75, 1.3, 1.5, 2, 3 have no gap and no
  overlap (each row's quads abut: `x + dst_w + widen = next x`, first ≤ 0, last ≥ screen width; same for columns);
  integer zooms set no widen/taller bit; a sprite at a tile corner of a parallax-1 layer lands on the quad's corner
  (exact at integer zooms, within 1 px otherwise); quads at zoom 0.5 ≈ 4× zoom 1. Files: tests only. Test:
  `tests/tilemap/test_map_draw_zoom.cpp`. Covers FR-9, AC-3.
- [ ] **T5.5** Backends: PC (`dest` from `dst_w/dst_h` + bits, fast path on `(orient & 7) == 0`, pre-swapped
  rectangle for diagonal quads with `w ≠ h`) and 3DS (`pos.w/h` from `dst_w/dst_h` + bits, texcoords unchanged). Files:
  `engine/platforms/pc/src/draw_tiles_pc.cpp`, `engine/platforms/3ds/src/draw_tiles_3ds.cpp`. Test:
  `tests/platform_conformance/test_draw_tiles_pc.cpp` (pixels: 16→32 exact, a widened 17-pixel tile, diagonal with a
  widen bit covers exactly its rectangle); `scripts/dkp.sh cmake --build --preset 3ds-release` builds. Covers FR-9, I.
- [ ] **T5.6** Stage end: ctest; `test_zoom1_regression.cpp` passes; AC-8 comparison for the platformer bench and
  level 1: identical; a quick PC screenshot of the platformer bench at zoom 0.5 and 2 (B) read back: no seams. Covers
  AC-3, AC-8.

## Stage 6: `examples/camera` (PC)

- [ ] **T6.1** Example core: original tileset made by `art/make_art.py` (contrasting tile borders that make any seam
  visible), a code-built map larger than four screens with rooms, the player square (Circle Pad / D-Pad) and an
  automatic second target, `CameraRig` with dead zone, smoothing on y, limits from `bounds()`, `debug_draw`. Controls
  (every 3DS has them): A trauma 0.4, X cycles zoom 0.5 → 1 → 2 → 3, Y toggles smoothing, R cycles the screen mode
  (single + HUD, tall world, two views), touch on the bottom screen places a marker at `to_world`, SELECT runs the
  self-check, START exits. Files: `examples/camera/*`, `examples/CMakeLists.txt`. Test: builds; PC screenshots
  (`CAMERA_DEMO=zoom1|zoom2|zoom05|tall|two`, `NNH_EXIT_AFTER_FRAMES`, `NNH_SCREENSHOT`) read back: no seams between
  tiles, the square continuous across the screens in tall-world mode, the marker under the touch. Covers AC-5, FR-10,
  FR-11, FR-14.
- [ ] **T6.2** Self-check (AC-2): `self_check.{hpp,cpp}` (pure: a 600-update script — target path with jumps and
  stops, dead zone, smoothing, limits changed mid-way, trauma, zoom changes, tall-world mode — FNV checksum of the
  bits of `followed()` and `position()` per update, checkpoints every 60), golden values from the simulator;
  `self_check_scene` logs `camera: <frame> sum <checksum>` every 60 frames and `camera: PASS <checksum>` or
  `camera: FAIL …`. Files: `examples/camera/self_check.{hpp,cpp}`, `self_check_scene.{hpp,cpp}`,
  `tests/camera_example/test_self_check.cpp`, `tests/CMakeLists.txt`. Test: the unit test against the goldens.
  Covers AC-2, AC-6, FR-12.
- [ ] **T6.3** [P] Bench (AC-7): `bench_scene` updates and applies a tall-world pair (dead zone, smoothing, trauma)
  16 times per frame for 600 frames, profile scope `camera`, logs `camera: bench avg X ms worst Y ms per pair
  (target < 0.05 ms) PASS|FAIL`. Files: `examples/camera/bench_scene.{hpp,cpp}`. Test: PC run logs the line. Covers
  AC-7, FR-15.
- [ ] **T6.4** The script `examples/camera/tests/check.nnhs`: SELECT → expect every `camera: <frame> sum` line and
  `camera: PASS`; then X/R presses with screenshots `zoom2`, `zoom05`, `zoom15` (set by the script's presses),
  `tall`, `two`; a touch on the bottom screen and its marker line; the bench line; `fail-on "camera: FAIL"`,
  `fail-on "] E "`. Test: `nnh run camera --pc --no-build --script examples/camera/tests/check.nnhs` → PASS, twice.
  Covers AC-5, AC-6, AC-7.

## Stage 7: the platformer's new feel, the zoomed gate, opt-in checks

- [ ] **T7.1** Platformer step 2: dead zone, smoothing on y, a look-ahead offset in the facing direction, a small
  trauma when the player lands from a high fall (landing speed threshold); `test_level_camera.cpp` covers the new
  settings and the landing rule. Regenerate the replay lines with `PLATFORMER_DEMO=replay NNH_EXIT_AFTER_FRAMES=920
  build/pc-debug/examples/platformer/platformer.exe` and copy them into `examples/platformer/tests/level.nnhs` **in
  the same commit**, whose message says why they changed. Files: `examples/platformer/level_camera.cpp`,
  `level_play.cpp`, `tests/level.nnhs`, `tests/platformer/test_level_camera.cpp`. Test: ctest
  `platformer_replay_matches_script`; `level.nnhs --pc` → PASS. Covers AC-4 step 2.
- [ ] **T7.2** [P] Bench zoom: B cycles 1 → 0.5 → 2 (`gfx::set_camera` by hand: the bench keeps its own camera, XI);
  map 64 × 40 cells with the zoom-1 camera range unchanged; a measure-only gate at zoom ≠ 1 logging
  `bench: zoom 0.50 limits L layers (300 sprites), S sprites (4 layers)` and `bench: zoom 0.50 done`. Files:
  `examples/platformer/bench.{hpp,cpp}`, `gate.{hpp,cpp}`, `tests/platformer/test_gate_run.cpp`. Test: unit test of
  the measure-only gate; AC-8 bench screenshot identical at zoom 1. Covers AC-7, AC-8.
- [ ] **T7.3** `examples/platformer/tests/zoom_gate.nnhs`: the zoom-1 gate (`bench: PASS` required), then B + A at
  zoom 0.5 and at zoom 2 (`expect "bench: zoom 0.50 done"`, `"bench: zoom 2.00 done"`), screenshots of each. Test:
  `nnh run platformer --pc --no-build --script examples/platformer/tests/zoom_gate.nnhs` → PASS. Covers AC-7.
- [ ] **T7.4** Opt-in and regression: `hello` sizes against T0.1 (budget ≤ 320 B text 3ds-release, ≤ 512 B
  pc-release; no camera symbol), AC-8 comparison of every reference screenshot, `gate.nnhs --pc` and the other
  examples' scripts unchanged and passing. Files: plan.md ("Stage 7 results"). Covers FR-16, AC-8.

## Stage 8: Linux checks (VM 120)

- [ ] **T8.1** On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; the noise
  goldens, the zoom-1 regression hash and the AC-2 checksums are identical on all three. Files: tasks.md (results).
  Covers I, FR-12, AC-2.

## Stage 9: console check (the only steps on the 3DS)

- [ ] **T9.1** Builds and strings: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development builds of
  `examples/camera` and `examples/platformer`; `scripts/check_release_strings.sh` on the Release `camera.elf` (no
  camera debug strings); `hello.elf` size recorded. Covers FR-14, AC-8.
- [ ] **T9.2** Old 3DS, nobody touching it after Y. Three runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run camera --script examples/camera/tests/check.nnhs` → every sum line equals
     the PC's, `camera: PASS`, bench < 0.05 ms per pair; screenshots (zoom 2, 0.5, 1.5, tall world) read back: no
     seams, no texel bleeding (else the plan's mitigation);
  2. `nnh run platformer --script examples/platformer/tests/level.nnhs` → the new replay lines, `60 Hz PASS`;
  3. `nnh run platformer --script examples/platformer/tests/zoom_gate.nnhs` → `bench: PASS` at zoom 1 and the
     measured limits at 0.5 and 2.
  Files: plan.md ("Stage 9 results": verdicts, bench numbers, limits, log paths). Covers AC-4, AC-6, AC-7.

## Stage 10: docs, roadmap, status

- [ ] **T10.1** `docs/api/camera.md` (following, dead zone, smoothing, limits, trauma shake with the "shake for N
  frames" recipe, zoom, the screen modes with a picture of the tall-world layout, conversions, pixel snap, debug
  drawing, determinism, costs and measured numbers, misuse) and header comments (allocates? per-frame safe? misuse?).
  Covers VIII, AC-9.
- [ ] **T10.2** Other pages: `docs/api/gfx.md` (`Camera2D::zoom`), `docs/api/tilemap.md` (`bounds()`, zoomed layers,
  the zoom limits measured in T9.2), `docs/architecture/rendering.md` (zoom section status → implemented),
  `docs/tech/devkitpro-3ds.md` and `docs/tech/sdl3.md` (zoomed tiles, the diagonal rectangle), `docs/guides/levels.md`
  (limits from `bounds()`), `docs/REFERENCES.md` (Godot `Camera2D`, Cinemachine, Eiserloh), `CLAUDE.md` (layout
  paragraph and module list); `python -I scripts/docs_tool.py check` → 0 broken links. Covers AC-9.
- [ ] **T10.3** `docs/architecture/decisions.md`: D33 change-log note "implemented" with any stage refinements.
  Covers AC-9.
- [ ] **T10.4** Final measurements (header cost of `camera.hpp`, build times, `hello` sizes) in plan.md and
  `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T10.5** `docs/ROADMAP.md`: "Camera helpers" checkbox.
- [ ] **T10.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T9.2
  pending").
