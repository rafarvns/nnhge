# Tasks 019: Collision layers and queries

Status: approved (2026-11-22, maintainer pre-approved)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The tilemap is changed only after the `collision_grid()` regression test is pinned (T0.2), and the platformer
swaps to layers and queries (stage 6) before it gains anything visible, so its unchanged golden lines prove the swap.
Stage 9 is run on the Linux build VM by the lead; the only console stage is stage 10: three scripted runs the
maintainer only starts (Homebrew Launcher → Y). Deviations found while building go into plan.md as "Stage N
refinements", as in plans 012–018.

## Stage 0: baseline and regressions

- [x] **T0.1** Baseline numbers (Windows, 3 runs each): `hello.exe` (pc-release, pc-debug) and `hello.elf`/`.3dsx`
  (3ds-release, 3ds-dev) sizes (`size` text/data/bss); edit `examples/hello/main.cpp` → simulator and `.3dsx` build
  times; clean `nnh_core` build; preprocessed line count and compile time of a one-line TU including
  `<nnh/collision.hpp>` and one including `<nnh/tilemap.hpp>`; `objdump -d` of `map_collision.cpp.o` (3ds-release and
  pc-release) saved as `build/ref-019/map_collision.{3ds,pc}.dis`. Files: plan.md ("Stage 0 results"). Covers III,
  FR-18, AC-8.
- [x] **T0.2** `collision_grid()` regression test, written and pinned on today's code: on the fake platform, the cells
  of `collision_at` and of `collision_grid()` (through `detail::cell_at` and through `collision::move`/`contacts` of a
  few boxes) over the loaded `tests/data/maps/good.tmj`, the platformer's `level1` map (its converted `.nnhmap` as a
  test fixture or loaded from the build's assets), a code-built map with collision layers, solid and one-way tiles on
  several layers and tilesets, and the same map after `set_tile`/`fill`/`set_layer_collision`/`set_tile_collision`
  edits; an FNV hash of every (x, y, kind) and of the move results, pinned as golden values. Files:
  `tests/tilemap/test_collision_grid_regression.cpp`, `tests/CMakeLists.txt`. Test: passes on `main` before any change.
  Covers FR-5, FR-6, FR-18, AC-3.
- [x] **T0.3** [P] Converter baseline: convert `tests/data/maps/good.tmj` and `examples/platformer/assets/levels/level1.tmj`
  with today's `nnh-assetc` and record their SHA-256 in plan.md (the outputs stay in `build/ref-019/`). Covers FR-7.
- [x] **T0.4** [P] Reference screenshots for AC-8 (kept in `build/ref-019/`, not committed): `hello`, `sprites`,
  `scenes`, `camera` (`CAMERA_DEMO=zoom1`), platformer `PLATFORMER_DEMO=bench` (top screen) and level 1 at frame 90,
  plus `build/ref-019/compare.sh` that rebuilds them and `cmp`s each PNG. Files: plan.md (list and frames). Covers AC-8.
- [x] **T0.5** [P] Test runner capacity: `kMaxTests` 2048 → 4096 (1849 used, the spec adds ≈ 200). Files:
  `tests/framework/test_main.cpp`. Test: `nnh_tests` passes. Covers VI.

## Stage 1: layers, views, filters, point and box queries (pure)

- [x] **T1.1** Public types: `Layers` and its constants, `layer(n)` (`static_assert`s in constant expressions;
  `detail::bad_layer` asserts at run time), `detects`, `BoxView` and the three `box_view` helpers (member-pointer
  template with the 32-bit `static_assert`), `QueryFilter`, `RayHit`, `CellCrossing`, every query declaration; CMake
  list `_nnh_query_sources` with `-ffp-contract=off`. Files: `engine/core/include/nnh/collision.hpp`,
  `src/collision/layers.cpp`, `engine/core/CMakeLists.txt`. Test: `tests/collision/test_layers.cpp`. Covers FR-1, FR-2.
- [x] **T1.2** Reading views and checking misuse: box/layers `i` by `memcpy`, `passes(filter, i)` (mask, ignore, no
  area), view validation (null with count, strides, negative count), buffer validation, finite inputs, grid cell
  sizes; assert in Debug/Development, "no hits" in the Release path. Files: `src/collision/box_view.hpp`,
  `query_check.{hpp,cpp}`. Test: `tests/collision/test_box_view.cpp` (plain, parallel, structs with padding, layer
  field before/after the box, enum-typed field, absent layers = all layers, count 0, null items). Covers FR-3, FR-4,
  FR-17.
- [x] **T1.3** Hit list: capped insertion sorted by fraction, stable on ties, total returned, null + 0 counts; the
  index variant for point/box queries. Files: `src/collision/hit_list.{hpp,cpp}`. Test:
  `tests/collision/test_hit_list.cpp`. Covers FR-11.
- [x] **T1.4** `boxes_at`, `boxes_overlapping`, `cell_at(grid, point)`. Files: `src/collision/point_queries.cpp`.
  Test: `tests/collision/test_point_queries.cpp` (half-open edges, ascending order, mask, ignore, buffer contract,
  `outside` kinds, negative coordinates, saturation; `cells_touching` unchanged). Covers FR-14, FR-15.
- [x] **T1.5** Stage end: `ctest --preset pc-debug` (spec 012's collision tests unchanged and passing). Covers VI,
  FR-5.

## Stage 2: rays against boxes, box casts (pure)

- [x] **T2.1** Exact predicates: segment setup, ratio comparison by cross products in `double`, fraction and point
  rounding (face coordinate exact, other axis clamped to the face), one-ulp float steps on the bit pattern. Files:
  `src/collision/exact.{hpp,cpp}`. Test: `tests/collision/test_exact.cpp` (ties exact where float division would
  round apart, monotone fractions, lattice exactness bound, steps across powers of two and zero). Covers FR-8, FR-16.
- [x] **T2.2** Slab test of one box: entry per face, normals, corner entry (x face), corner touch and edge graze
  (miss), zero size, NaN fields (miss), start inside, start flush on a face going in (fraction 0) or out (miss).
  Files: `src/collision/ray_box.{hpp,cpp}`. Test: `tests/collision/test_ray_box.cpp`. Covers FR-10.
- [x] **T2.3** `raycast`/`raycast_all` on a view: filter, ignore, inside with and without `hit_from_inside`, nearest
  with ties to the lowest index, all hits sorted, overflow keeps the nearest, counting. Files:
  `src/collision/raycast_boxes.cpp`. Test: `tests/collision/test_raycast_boxes.cpp`. Covers FR-8, FR-10, FR-11.
- [x] **T2.4** [P] Distance: `detail::ray_length` with `std::sqrt(float)` in its own file compiled with
  `-fno-math-errno`; `distance = fraction × length`. Check: `objdump -d` of `ray_distance.cpp.o` on pc-release
  (`sqrtss`, no call) and 3ds-release (`vsqrt.f32`, no call), and no fused multiply-add in any `_nnh_query_sources`
  object (plan 015's check). Files: `src/collision/ray_distance.cpp`, CMake. Test: `test_raycast_boxes.cpp` (3-4-5
  distances exact). Covers FR-16.
- [x] **T2.5** `box_cast`/`box_cast_all`: grown target in `double` numerators, each face, flush contact position
  (`overlaps` false at it, true one ulp further), corner tie, sliding along a face (miss), overlapping at the start,
  zero-size cast box, relative motion of two movers, all contacts sorted. Files: `src/collision/box_cast.cpp`. Test:
  `tests/collision/test_box_cast.cpp`. Covers FR-13.
- [x] **T2.6** Stage end: ctest. Covers VI.

## Stage 3: rays against grids (pure)

- [x] **T3.1** The grid walk: line-on-axis (nothing), clip when `outside` can't block, start cell by direction
  (entered through a face or inside), steps with exact comparisons, corners (x-neighbour, y-neighbour, diagonal at
  the same fraction), stop before `t ≥ 1`, saturation. An exact reference (`ray_reference.hpp`: lattice coordinates
  ×256 in `int64`, every cell of the bounding range tested against the segment's interior, corner side cells added,
  sorted by exact ratio) must equal the walk cell for cell on 5000 seeded rays (integer hash, no `rand`) over origins
  negative/fractional and cells 8×8, 16×16, 12×20, 24×10; plus a sampling check. Files:
  `src/collision/grid_walk.{hpp,cpp}`, `tests/collision/ray_reference.hpp`. Test: `tests/collision/test_grid_walk.cpp`
  (8 octants, 4 axis cases, edges, faces, corners, 10 000-px rays, far outside clipped). Covers FR-9.
- [x] **T3.2** `cells_crossed`: the walk with kinds and entry fractions, buffer contract, clip only for an `Empty`
  outside, outside cells listed otherwise. Files: `src/collision/cells_crossed.cpp`. Test:
  `tests/collision/test_cells_crossed.cpp`. Covers FR-11.
- [x] **T3.3** `raycast` on a grid: solid, one-way (top going down, corner going down, from below and the side pass),
  `ignore_one_way`, inside a solid cell with/without `hit_from_inside`, inside a one-way cell (never), `outside`
  Solid/Empty/OneWay, flush points and normals, lookup count = cells visited (counting callback). Files:
  `src/collision/raycast_grid.cpp`. Test: `tests/collision/test_raycast_grid.cpp`; over the 5000 seeded rays, the
  first hit equals the first blocking cell of `cells_crossed`. Covers FR-8, FR-9.
- [x] **T3.4** `raycast_all` on a grid (a piercing laser: every blocking cell in walk order, overflow, counting).
  Files: `raycast_grid.cpp`. Test: `tests/collision/test_raycast_all_grid.cpp`. Covers FR-11.
- [x] **T3.5** Grid + boxes and line of sight: nearest of both, cell wins a tie (a box flush behind a wall), absent
  grid/boxes, `line_of_sight` (clear, a cell, a box, viewer excluded by `ignore` and by layers, start inside never
  counts). Files: `src/collision/raycast_world.cpp`. Test: `tests/collision/test_raycast_world.cpp`. Covers FR-12.
- [x] **T3.6** [P] Misuse sweep over every query (FR-17's list: assert hook in Debug, false/0/`Empty` cell in the
  Release path, nothing read), no allocation over 10 000 mixed queries, no `init()`. Files: tests only. Test:
  `tests/collision/test_query_misuse.cpp`. Covers FR-17.
- [x] **T3.7** Stage end: ctest; opt-in check: `hello` Release sizes against T0.1 (identical) and `nm` (no query
  symbol) on PC and 3DS; a test object using one query links only its own files (`nm` of the test map). Files:
  plan.md ("Stage 3 results"). Covers FR-18, AC-8.

## Stage 4: layer sets on tilemaps

- [x] **T4.1** The runtime area (D34): layout, `map_block_bytes`, `init_map_runtime` (sets = default, slots unused),
  accessors; `create` and `tilemap_from_file` allocate and initialise it. Files: `src/tilemap/map_runtime.{hpp,cpp}`,
  `tilemap.cpp`, `map_load.cpp`, CMake. Test: `tests/tilemap/test_map_collision_layers.cpp` (part 1: defaults on
  created and loaded maps; memory tag grows by the formula); `test_collision_grid_regression.cpp` unchanged. Covers
  FR-6.
- [x] **T4.2** Sets in code: `set/layer_collision_layers`, `set/tile_collision_layers` (invalid layer/id/0 assert and
  keep), `TileLayerDesc::collision_layers` in `add_tile_layer`, `collision_layers_at` (unions; empty, outside).
  Files: `src/tilemap/map_collision_layers.cpp`, `map_build.cpp`, `engine/core/include/nnh/tilemap.hpp`. Test:
  `test_map_collision_layers.cpp` (part 2). Covers FR-6.
- [x] **T4.3** `collision_grid(mask)`: slots (same mask = same context, never reused), `grid_cell_masked`,
  `kAllLayers` → `collision_grid()`, out of slots (assert; Release unfiltered + one log). Files:
  `src/tilemap/map_filtered_grid.cpp`. Test: `tests/tilemap/test_filtered_grid.cpp` (masks that keep, drop or leave
  a one-way; masked == unmasked over 200 random maps with default sets; live edits; `move` and `cells_touching`
  through a filtered grid; grids survive moving the `Tilemap`). Covers FR-6, AC-3.
- [x] **T4.4** Loading: `collision_layers` int properties of tile layers and tiles → runtime sets after `copy_map`;
  non-int or 0 → default + one warning per map; maps without it unchanged. Files: `src/tilemap/map_load_layers.cpp`,
  `map_load.cpp`. Test: `tests/tilemap/test_map_load_collision_layers.cpp` (hand-made `.nnhmap` blobs through
  `detail::tilemap_from_file`); fuzz seeds with the property for `fuzz_tilemap` (mini-fuzz in `nnh_tests`). Covers
  FR-6, FR-7, X.
- [x] **T4.5** [P] AC-8 comparison (`build/ref-019/compare.sh`): every reference screenshot identical. Covers AC-8.
- [x] **T4.6** Stage end: ctest (`test_collision_grid_regression.cpp` passes unchanged); `map_collision.cpp` not in
  the diff and its `objdump -d` equal to T0.1's on both presets. Files: plan.md ("Stage 4 results"). Covers FR-18,
  AC-3, AC-7.

## Stage 5: Tiled `collision_layers` in the converter

- [ ] **T5.1** `check_collision_layers`: an int (a flag enum saved as a number carries `propertytype`, accepted) and
  ≠ 0, on tile layers and on tiles, kept as a property; refusals `file: layer "water": property "collision_layers"
  must be an integer` and `... must not be 0: untick collision instead` (exact text in the test). Fixtures (generated
  by `make_fixtures.py`): `good_collision_layers.tmj`, `bad_collision_layers_type.tmj`, `bad_collision_layers_zero.tmj`.
  Files: `tools/assetc/src/tiled_properties.{hpp,cpp}`, `tiled_layer_data.cpp`, `tiled_tileset.cpp`,
  `tests/data/maps/*`, `tools/assetc/CMakeLists.txt` (0.4.0). Test: `tools/assetc/tests/test_tiled_collision_layers.cpp`.
  Covers FR-7.
- [ ] **T5.2** End to end: the good fixture converted by the build and loaded has the expected layer and tile sets and
  filtered grids. Files: tests only (`tests/tilemap/load_fixture.hpp` reuse). Test:
  `test_map_load_collision_layers.cpp` (part 2). Covers FR-6, AC-3.
- [ ] **T5.3** [P] Old maps: `good.tmj` and `level1.tmj` convert to the same bytes as T0.3 (SHA-256); the libFuzzer
  map target of `nnh-assetc` gets a seed with the property. Files: plan.md ("Stage 5 results"). Covers FR-7, AC-3.

## Stage 6: the platformer's swap (same play)

- [ ] **T6.1** Layers and coins: `layers.hpp` (`kWorld = kDefaultLayer`, `kPlayer`, `kCoin`, `kEnemy`,
  `kEnemyBullet`, `kPlayerMask`); `CoinBook::Coin` gets `layers` (`kCoin`), `collect(box, mask, ...)` =
  `boxes_overlapping` over `box_view(coins_, count_, &Coin::bounds, &Coin::layers)` with the player's mask, taken coins
  get `kNoLayers` (the `taken_` array goes), `restart()` puts `kCoin` back. Files: `examples/platformer/layers.hpp`,
  `coin_book.{hpp,cpp}`, `level_play.cpp`. Test: `tests/platformer/test_coin_book.cpp` (same results as before on its
  cases, mask without `kCoin` takes nothing, order ascending). Covers AC-4 step 1, FR-3.
- [ ] **T6.2** The player moves through `map.collision_grid(kPlayerMask)` (outside Solid as today). Files:
  `level_play.cpp`. Test: ctest `platformer_replay_matches_script` passes with **unchanged** lines; `nnh run
  platformer --pc --no-build --script examples/platformer/tests/level.nnhs` → PASS; AC-8 comparison (level 1 and the
  bench identical). Files: plan.md ("Stage 6 results"). Covers AC-4 step 1, FR-6, FR-18.

## Stage 7: `examples/queries` (PC)

- [ ] **T7.1** Example core: original tileset from `art/make_art.py` (wall, water, plank, floor; tile properties
  `name`), the room built in code (25 × 15 tiles of 16 px: a `walls` collision layer on `kWorld`, a `water` collision
  layer on `kWater`, one-way planks on `kWorld`), 12 boxes on three layers (red, green, blue), a fixed emitter and a
  target moved by Circle Pad / D-Pad; modes (X): first hit, all hits with the cells crossed shaded, box cast; Y
  toggles ignore one-way; A toggles the water bit, B / L / R the box layers (HUD shows the mask in hex);
  `query_draw` draws the ray, crossed cells, hits with their normals and the first hit highlighted (≤ 200 draws);
  `QUERIES_DEMO=first|all|cast|water|touch` presets; SELECT self-check, START quits. Files: `examples/queries/*`,
  `examples/CMakeLists.txt`. Test: builds; PC screenshots of the five presets (`NNH_EXIT_AFTER_FRAMES`,
  `NNH_SCREENSHOT`) read back: crossed cells under the ray, the hit flush on the wall with its normal, the water
  passed with its bit off, the cast box flush against a box. Covers AC-5, FR-8..FR-13.
- [ ] **T7.2** The bottom screen: the room through `Camera2D{position (40, 0)}`, a touch → `world = position +
  touch`, marked on both screens; the HUD lists the cell (x, y, kind, `collision_layers_at`), the tile id and its
  properties, and the boxes containing the point (`boxes_at`); logs `under: cell 5,7 solid layers 0x1 tile 3 boxes 2`
  on each touch, and `queries: first hit cell 12,6 solid fraction 0.4375 normal -1,0` (or `box 3 ...`, `none`) when
  the first hit changes. Files: `queries_scene.cpp`, `hud.{hpp,cpp}`. Test: PC screenshot `touch`; the log lines in a
  `--pc` run. Covers AC-5, FR-14.
- [ ] **T7.3** Self-check (AC-2): `self_check.{hpp,cpp}` (pure: a fixed array grid with solid and one-way cells, 64
  boxes on layers from the hash, 4096 rays — `raycast` grid/boxes/both, `raycast_all` grid and boxes with capacity
  8, `cells_crossed` with capacity 32, `line_of_sight`, filters from the hash — then 512 box casts; FNV-1a over every
  result's bits; a checkpoint every 1024 rays); golden values from the simulator in `kQueriesCheckGolden`;
  `self_check_scene` runs 64 rays per frame and logs `queries: <n> sum <checksum>` and `queries: PASS <checksum>` or
  `queries: FAIL ...`. Files: `examples/queries/self_check.{hpp,cpp}`, `self_check_scene.{hpp,cpp}`,
  `tests/queries_example/test_self_check.cpp`, `tests/CMakeLists.txt`. Test: the unit test against the goldens;
  ctest `queries_self_check_pc`. Covers AC-2, AC-6, FR-16.
- [ ] **T7.4** [P] Bench (AC-7): `bench_scene` on the room's filtered grid + 64 boxes: 128 first-hit rays of 0–400 px
  per frame for 600 frames, then 256 point queries over 256 boxes per frame, then 64 `move`s per frame through
  `collision_grid()` and through a filtered grid; profile scopes; logs `queries bench: rays avg X ms worst Y ms
  (target < 2 ms) PASS|FAIL`, `queries bench: points avg X ms, Z us per box (target <= 0.1 us) PASS|FAIL`,
  `queries bench: move unfiltered X us filtered Y us`. Files: `examples/queries/bench_scene.{hpp,cpp}`. Test: PC run
  logs the lines. Covers AC-7.
- [ ] **T7.5** The script `examples/queries/tests/check.nnhs`: screenshots of each mode (set by presses), a mask
  toggle that lets the ray pass the water (expect the first-hit line), stick moves with the expected first-hit lines,
  touches at fixed bottom-screen points with their expected `under:` lines, SELECT → every `queries: <n> sum` line
  and `queries: PASS`, then the bench lines; `fail-on "queries: FAIL"`, `fail-on "] E "`. Test: `nnh run queries --pc
  --no-build --script examples/queries/tests/check.nnhs` → PASS, twice. Covers AC-5, AC-6, AC-7.

## Stage 8: the platformer's turrets, opt-in checks

- [ ] **T8.1** Turrets, pure: `TurretBook` (≤ 4 turrets, ≤ 16 bullets): line of sight each frame within a range box
  (`line_of_sight` through the line-of-sight grid with `ignore_one_way`), fire every 90 frames while seen, 8 fixed
  directions of short binary fractions toward the player, bullets moved by `raycast(bullet_grid, bodies, pos, pos +
  vel, hit, {.mask = kBulletMask})` (bodies = the player and the turrets in one struct view: turrets filtered out by
  the mask), a wall stops a bullet flush (stuck 45 frames, then gone), the player counts a hit and flashes 20 frames;
  a state checksum word. Files: `examples/platformer/turrets.{hpp,cpp}`, `tests/CMakeLists.txt`. Test:
  `tests/platformer/test_turrets.cpp` (sight through a gap / blocked / planks ignored, cooldown, directions, a fast
  bullet stops at a one-cell wall, hits the player, ignores turrets, state word). Covers AC-4 step 2.
- [ ] **T8.2** Turrets in level 1: two `turret` objects in `art/make_level.py` (regenerated `level1.tmj`) where the
  replay run passes them; `level_play` reads them, updates the book after the player, draws turrets (shapes), the
  sight line when seen, bullets (a 3×3 square behind the hit point, along the normal), the player's flash; the HUD
  shows hits; `ReplaySample::turrets` joins the checksum. Regenerate the replay lines with `PLATFORMER_DEMO=replay
  NNH_EXIT_AFTER_FRAMES=920 build/pc-debug/examples/platformer/platformer.exe` and copy them into
  `examples/platformer/tests/level.nnhs` **in the same commit**, whose message says why they changed (the `x`, `y` and
  `coins` columns must not change: bullets don't move the player). Files: `examples/platformer/level_play.{hpp,cpp}`,
  `replay_run.{hpp,cpp}`, `hud.{hpp,cpp}`, `art/make_level.py`, `assets/levels/level1.tmj`, `tests/level.nnhs`,
  `main.cpp` (comment), `tests/platformer/test_replay_run.cpp`. Test: ctest `platformer_replay_matches_script`;
  `level.nnhs --pc` → PASS; a PC screenshot (`PLATFORMER_DEMO=replay NNH_EXIT_AFTER_FRAMES=<frame chosen here>`) read
  back: a turret's sight line to the player and a bullet stopped flush at a wall (frame recorded in plan.md). Covers
  AC-4 step 2.
- [ ] **T8.3** [P] Mutation pass as in plan 018: hand mutants of `src/collision/*` (new files), the tilemap's masked
  rule, slots and loader, the converter check, the coin book and turrets, applied one at a time with the whole
  `nnh_tests`; survivors become tests. Files: plan.md ("Mutation testing"). Covers VI.
- [ ] **T8.4** Opt-in and regression: `hello` sizes against T0.1 (identical; no query or `grid_cell_masked` symbol),
  AC-8 comparison of every reference screenshot (the bench and `camera` identical; level 1 is allowed to differ by the
  turrets only), `gate.nnhs --pc` and the other examples' scripts unchanged and passing. Files: plan.md ("Stage 8
  results"). Covers FR-18, AC-8.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests`, `nnh_assetc_tests` and ctest
  pass; the regression hash (T0.2), the 5000-ray reference comparison and the AC-2 self-check golden are identical on
  all three (they are test assertions); libFuzzer `libfuzzer_tilemap` and the assetc map target run 10 minutes each
  with the new seeds, clean. Files: tasks.md (results). Covers I, X, FR-16, AC-1, AC-2, AC-3.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development
  builds of `examples/queries` and `examples/platformer`; `objdump` of `ray_distance.cpp.o` (3DS: `vsqrt.f32`, no
  call); `hello.elf` size recorded. Files: plan.md ("Stage 10: console run (prepared)": commands and every expected
  line). Covers FR-16, AC-8.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Three runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run queries --script examples/queries/tests/check.nnhs` → every `queries: <n> sum`
     line and `queries: PASS` equal the PC's, every `under:` and first-hit line as on the PC, the bench lines (rays
     < 2 ms; points per box ≤ 0.1 µs; the move numbers recorded); screenshots read back;
  2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the regenerated
     replay lines, `60 Hz PASS`;
  3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS`
     (spec 012's gate still passes).
  Files: plan.md ("Stage 10 results": verdicts, bench numbers, log paths). Covers AC-4, AC-6, AC-7.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** `docs/api/collision.md` (layers and masks with the detection rule and a layer-enum pattern, box views
  incl. the custom-view case, filters, rays with the traversal rules and a picture of the corner rule, first/all
  hits, cells crossed, line of sight, box casts and the two-movers recipe, point and box queries, determinism, costs
  measured in T10.2, misuse, the cell/box corner note) and header comments (allocates? per-frame safe? misuse?).
  Covers VIII, AC-9.
- [ ] **T11.2** Other pages: `docs/api/tilemap.md` (collision layer sets, `collision_grid(mask)` and its 8 masks,
  `collision_layers_at`, the memory formula's new term, the `collision_layers` row of the Tiled table, the
  "set in code doesn't change collision" pitfall), `docs/guides/levels.md` (a `collision_layers` flag enum step by
  step, bits 0..30), `docs/REFERENCES.md` (Godot, Unity, Box2D, Tiled's custom enums, Amanatides & Woo), `CLAUDE.md`
  (layout paragraph: queries, `examples/queries`); `python -I scripts/docs_tool.py check` → 0 broken links. Covers
  AC-9.
- [ ] **T11.3** `docs/architecture/decisions.md`: D34 change-log note "implemented" with any stage refinements.
  Covers AC-9.
- [ ] **T11.4** Final measurements (header cost of `collision.hpp` and `tilemap.hpp`, build times, `hello` sizes) in
  plan.md and `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T11.5** `docs/ROADMAP.md`: "Collision layers and masks" and "Raycasts and shape queries" checkboxes.
- [ ] **T11.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
