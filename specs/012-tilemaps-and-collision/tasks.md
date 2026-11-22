# Tasks 012: Tilemaps and collision

Status: implemented (2026-11-22): stages 0–6 done (T3.9 passed on the Old 3DS XL), stage 7 T7.1–T7.3 and stage 8
T8.2–T8.7 done. **Still pending with the maintainer:** T7.4 (prepared, not run: `level.nnhs` on the Old 3DS); T8.1 (Linux,
sanitizers, fuzzing) done on VM 120
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task. Every stage ends with
something verifiable on PC (`ctest --preset pc-debug`, a screenshot); console steps are marked **3DS** and are run
by the maintainer.

## Stage 0 — groundwork

- [x] **T0.1** spike: a minimal `tools/assetc` (prints its version) built by `cmake/nnh_host_tools.cmake` — a
  target in `pc-debug`, an `ExternalProject` with `NNH_HOST_CXX` in `3ds-release` through `scripts/dkp.sh` on
  Windows and on VM 120 (and `-DNNH_ASSETC=<path>` override); measure the 3DS no-op build before/after — files:
  `tools/assetc/CMakeLists.txt`, `tools/assetc/src/main.cpp`, `cmake/nnh_host_tools.cmake`, `scripts/dkp.sh`,
  `CMakeLists.txt` — test: both presets build it; result written in this file — covers FR-1 (risk: host compiler)

  **Result (2026-11-21): the host build works everywhere tried; the risk is closed.** `nnh-assetc --version` →
  `nnh-assetc 0.1.0 (crc32 CBF43926)` (it compiles core's pure `crc32.cpp`). Target `nnh_assetc` in every preset;
  `nnh_assetc_command()` is the hook for the asset step (stage 7). Details in
  [cmake.md](../../docs/tech/cmake.md) ("nnh-assetc").
  - Windows `pc-debug` (WinLibs GCC 16): normal target, ctest `assetc_version`/`assetc_usage`, static runtime.
    102/102 tests from a clean folder.
  - Windows `3ds-release` through `dkp.sh`: ExternalProject with WinLibs `g++.exe`, MSYS's CMake and Ninja
    driving the native compiler; header changes tracked; the program is static (only UCRT imports). Two findings:
    **Git Bash's environment does not reach devkitPro's MSYS2** (the child starts with ~7 variables), so
    `dkp.sh` passes the compiler as an argument; and **MSYS's CMake misreads `C:/...`** ("CMAKE_CXX_COMPILER: C"),
    so it is converted inside with `cygpath -u`. First build 9.0 s (compiler detection), check 0.7 s.
  - Linux VM 120 (GCC 13.3, CMake 3.28): `pc-debug` clean + 102/102 tests; `3ds-release` with `/usr/bin/c++`
    found on the PATH, first build 0.76 s, check 0.15 s.
  - `-DNNH_ASSETC=<PC build's nnh-assetc>` in a 3DS folder: used as is, nothing built. Without any host compiler:
    3DS configures and `hello` builds; only `nnh_assetc` fails, saying to set `NNH_HOST_CXX` or `NNH_ASSETC`.
  - Budgets (Windows): 3DS no-op 1.69 s with and without the host-tools setup (A/B, 5 runs each: no cost; the
    tool is `EXCLUDE_FROM_ALL`); edit `hello` → `.3dsx` 2.7 → 2.85 s (noise range), simulator 1.4 → 1.38 s; clean
    3DS configure + build 12.1 + 19.3 s (ccache warm), the tool +9.0 s only when needed.
  - Not tried: the `devkitpro/devkitarm` CI image (CI is manual). Its job does not build `nnh_assetc`, so it is
    unaffected; whether the image has a host `c++` is checked when a CI game has maps (fallback: `NNH_ASSETC`).
- [x] **T0.2** [P] CRC-32 out of `fs/save_format.cpp` into pure `core/crc32.{hpp,cpp}` (no engine includes) —
  test: existing save tests + `tests/core/test_crc32.cpp` (known vectors) — covers FR-1, FR-11 (shared by map
  reader and converter). Done: `nnh::detail::crc32` keeps its name; `save_format.hpp` includes it; vectors
  ("123456789", empty/null, "a", the fox sentence, 32×0x00, 32×0xFF) checked against zlib.

## Stage 1 — collision (pure, no tilemap)

- [x] **T1.1** `<nnh/collision.hpp>` skeleton; `overlaps`, `contains`, `intersection`, `separation` — files:
  `collision/aabb.cpp` — test: `tests/collision/test_aabb.cpp` — covers FR-9
- [x] **T1.2** `Grid`, `grid_view`, cell lookup with `outside`, cell range of a box (half-open edges) — files:
  `collision/grid.cpp` — test: `test_grid.cpp` — covers FR-8, FR-9
- [x] **T1.3** `move`: x then y sweep, slide, touching sides, start-overlapping rule, huge deltas, non-zero origin,
  non-square cells, NaN/inf delta asserts — files: `collision/grid_move.cpp` — test: `test_grid_move.cpp` — covers FR-8
- [x] **T1.4** one-way cells and `drop_through` — test: `test_one_way.cpp` — covers FR-7, FR-8
- [x] **T1.5** [P] `contacts`, `cells_touching` — files: `collision/grid_query.cpp` — test: `test_contacts.cpp`,
  `test_cells_touching.cpp` — covers FR-8, FR-9

  **Result (2026-11-21):** 78 tests in `tests/collision/` (aabb 14, grid 10, move 29, one-way 9, contacts 9,
  cells_touching 7), helper `ascii_grid.hpp` (levels drawn as text); PC and 3DS build clean; API page
  [collision.md](../../docs/api/collision.md).
  - **One API addition to the sketch:** `Grid::cells` (`const Cell*`, row-major, used when set; otherwise the
    `cell` callback). The sketch's callback gets only `(context, x, y)`, so `grid_view` over a bare array would
    have nowhere to keep the row width; the array field also saves a function-pointer call per cell. The callback
    is unchanged for tilemaps.
  - Internals: `collision/grid_internal.hpp` (`first_cell`/`last_cell` saturating at ±2^30, `cell_range`,
    `cell_at`, the one-axis `sweep` shared by `move` and `contacts`). Runs of `outside` cells that cannot block are
    skipped, so a 10^7 px delta is cheap.
  - **Float rounding:** after a stop the position is nudged (doubling ulp steps) until the blocking line is out of
    the box's cell range, and clamped between start and target; without it `wall − w + w` can round past the
    wall and the next frame would ignore that wall (falls through). Three regression tests use inputs found by a
    float32 search.
  - `contacts` = a 1/64 px probe `sweep` on each side; `cells_touching` = overlapped (area > 0) non-empty cells,
    visiting only the grid when `outside` is Empty.
  - Mutation check (17 hand mutants of `move`/sweep/one-way: `>=`→`>` on the one-way top, rule removed,
    `drop_through` ignored, scan includes overlapped lines (both directions), scan one short, y before x, both
    rounding nudges removed, start clamp removed, perpendicular range one short, outside one-way ignored,
    sides swapped, `moved` = delta when blocked, NaN assert removed, wrong outside skip): **16 killed**; the
    survivor ("one-way also checked on upward moves") is equivalent — a row entered upward never has its top
    at or below the box's starting bottom.

Verifiable: `nnh_tests collision` green on PC.

## Stage 2 — tilemaps built in code

- [x] **T2.1** binary format constants, limits and cell bits; layout from capacities with the memory formula —
  files: `tilemap/map_format.hpp`, `tilemap/map_layout.{hpp,cpp}` — test: `tests/tilemap/test_map_layout.cpp`
  (offsets aligned, sizes = formula) — covers FR-11
- [x] **T2.2** `Tilemap::create`, move, `reset`, memory tag `tilemap`, `LimitReached`/`InvalidArgument` — files:
  `include/nnh/tilemap.hpp`, `tilemap/tilemap.cpp` — test: `test_map_build.cpp` — covers FR-0, FR-11
- [x] **T2.3** `add_tileset` (margin, spacing, tile count, first ids, source table), `add_tile_layer`, layer
  settings — files: `tilemap/map_build.cpp` — test: `test_map_build.cpp` — covers FR-0, FR-2, FR-3
- [x] **T2.4** tiles and coordinates: `tile`, `tile_at`, `set_tile`, `fill`, `cell_rect`, `world_to_cell`,
  flips encode/decode — files: `tilemap/map_tiles.cpp` — test: `test_map_tiles.cpp` — covers FR-2, FR-4
- [x] **T2.5** [P] properties: string pool, `Properties` view, setters for map/layer/tile/object — files:
  `tilemap/map_properties.cpp` — test: `test_map_properties.cpp` — covers FR-0, FR-6
- [x] **T2.6** [P] object layers and objects: add, list, find by name/type — files: `tilemap/map_objects.cpp`,
  `map_build.cpp` — test: `test_map_objects.cpp` — covers FR-0, FR-5
- [x] **T2.7** collision: tile flags, `collision` layers, contributing-layer mask, `collision_at`,
  `collision_grid` → `collision::move` on a map — files: `tilemap/map_collision.cpp` — test:
  `test_map_collision.cpp` — covers FR-4, FR-7, FR-8

  **Result (2026-11-21):** 94 tests in `tests/tilemap/` (layout 12, build 25, tiles 16, properties 13, objects 12,
  collision 16; helper `map_fixture.hpp`); 653/653 unit tests, ctest 102/102; PC and `3ds-release` build clean. API
  page [tilemap.md](../../docs/api/tilemap.md). Files: `map_format.hpp`, `map_layout.*`, `map_strings.*` (string
  pool, equal strings shared), `map_property_table.*` (ranges + tile-property table, pure), `tilemap.cpp`,
  `map_build.cpp`, `map_tiles.cpp`, `map_properties.cpp`, `map_objects.cpp`, `map_collision.cpp`,
  `tilemap_internal.hpp`; log channel `tilemap`.
  - **Format/API refinements** (written into [plan.md](plan.md), "Stage 2 refinements"): header 128 B (counts *and*
    capacities, section offsets, the runtime collision-layer mask), tileset entry without its own tile size (tiles
    use the map's), tile table as two arrays (4 B sources + 1 B flags), objects carry their layer (no per-layer
    ranges), `TilemapDesc::max_tile_ids` (256), `ObjectDesc::id`, `PropertyOwner::map()/tile_layer()/…`,
    `tile_id_count()`, layer getters; tile-property table capacity = min(max_properties, max_tile_ids).
  - Property ranges stay contiguous whatever the order: inserting shifts later entries and every later range.
  - Collision-layer mask: set by `set_tile`/`fill` with a marked tile, by `collision` layers; `set_tile_collision`
    scans only layers that use the tile's tileset and are not in the mask yet; never cleared by `set_tile` (wider
    mask = extra reads, never a wrong answer). The grid's context is the block, so it survives moving the object.
  - Stale texture: the tileset keeps the texture's {slot, generation}; `detail::tileset_texture_alive` is false
    once it is destroyed, even if the slot is reused (stage 3 skips such tilesets); queries and collision unaffected.
  - Mutation check (34 hand mutants: layout padding/sizes/overlap, cell bits (flip swap, id mask, diagonal bit),
    total-cells cap, fill clipping both edges, tile bounds, last id, diagonal check, masks, floor vs truncation,
    inside test, collision rule (layer flag, solid, one-way, early one-way, mask), set_tile_collision scan,
    set_layer_collision recount, property insert position/shift/sort, int→float, string dedup, tileset id cap,
    margin, spacing, auto id, move-assign leak): **34 killed**.

Verifiable: `nnh_tests tilemap` green; a whole level can be built and collided against in a test.

## Stage 3 — drawing (the gate)

- [x] **T3.1** contract `TileQuad2D`, `TileBatch2D`, `draw_tiles`; fake records batches, quads and call order —
  files: `contract/nnh/platform/video.hpp`, `platforms/fake/src/video_fake.cpp`, `fake_internal.hpp`,
  `platform_fake.hpp` — test: `tests/platform_conformance/test_contract.cpp` (fake) — covers FR-3
- [x] **T3.2** visible range and parallax (Tiled's centre-based formula, snapped origin) — files:
  `tilemap/visible_range.{hpp,cpp}` — test: `test_visible_range.cpp` — covers FR-3, AC-2
- [x] **T3.3** tile expansion: cells → quads, source from the table, flip bits, empty skipped, per-tileset pass —
  files: `tilemap/tile_expand.{hpp,cpp}` — test: `test_tile_expand.cpp` — covers FR-2, FR-3
- [x] **T3.4** gfx tile batches: command kind, `build()` split points, records with `{expand fn, source}`,
  scratch of 1600 quads, `max_tiles` (+ `AppConfig`, `Stats`, overlay), drops, forget-by-source on map
  destruction — files: `gfx/tile_batches.{hpp,cpp}`, `gfx/draw_list.*`, `gfx/gfx_frame.cpp`, `gfx/gfx_state.hpp`,
  `include/nnh/app.hpp`, `include/nnh/gfx.hpp`, `debug/overlay.cpp` — test: `tests/gfx/test_tile_batches.cpp`,
  `test_draw_list.cpp` — covers FR-3, FR-4, FR-11
- [x] **T3.5** `gfx::draw_tilemap` (one map layer or all, draw layer, opacity, offset, ignore_camera) — files:
  `tilemap/map_draw.cpp` — test: `test_map_draw.cpp` (fake: batches interleaved with sprites, `set_tile` after
  draw is seen, no allocation over 100 frames in `test_tilemap_alloc.cpp`) — covers FR-3, FR-4, FR-11
- [x] **T3.6** PC backend — files: `platforms/pc/src/draw_tiles_pc.cpp` — test: conformance pc (render the 8
  orientations of an asymmetric 8×8 texture offscreen, read pixels back) — covers FR-2, FR-3
- [x] **T3.7** [P] 3DS backend + `C2D_Init` with `max_tiles` — files: `platforms/3ds/src/draw_tiles_3ds.cpp`,
  `app_3ds.cpp` — test: `3ds-release` builds — covers FR-3
- [x] **T3.8** `examples/platformer` skeleton: art script (tileset, player, backdrops), the **bonus level built in
  code**, camera follow, `PLATFORMER_DEMO=bench` and `=flips` — files: `examples/platformer/{CMakeLists.txt,
  main.cpp, bonus_level.cpp, bench.cpp, art/make_art.py, assets/...}`, `examples/CMakeLists.txt` — test: PC
  screenshots (bonus, bench, flips) read back — covers FR-0, FR-3, AC-1 (part), AC-4

  **Result (2026-11-21, T3.1–T3.8):** 59 new unit tests (712/712: visible range 14, tile expansion 9, tile batches
  8, tile submit through the frame 8, draw-list splits 3, `draw_tilemap` 16, no allocation over 100 frames 1) and 2
  conformance tests (`draw_tiles` on fake and pc; PC read-back of the 8 orientations of an 8×8 texture, pixel
  exact); ctest 102/102; PC and `3ds-release` build clean (`platformer.3dsx` 296 KB, no debug strings in Release).
  Refinements (bit order of `orient`, `set_parallax_origin`, 64 batches per frame, …) are in
  [plan.md](plan.md) ("Stage 3 refinements"); API in [tilemap.md](../../docs/api/tilemap.md#drawing),
  [gfx.md](../../docs/api/gfx.md#capacities), design in [rendering.md](../../docs/architecture/rendering.md).
  - Screenshots read back (PC): bonus level (parallax clouds and hills, solid and one-way tiles, player between the
    ground and the front layer, camera following), bench (4 full layers + 300 sprites: ~1566 tiles, 304 draws, no
    drops), flips (all 8 orientations as Tiled; sprites agree; no seams under a 0.37 px/frame camera).
    `examples/sprites` screenshots unchanged (`DrawCommand` keeps its size: `tile_batch` fills padding).
  - Mutation check (42 hand mutants of visible range, expansion, batches, splits, submit, `draw_tilemap`, forget,
    stats): **41 killed** (3 survivors of the first run got tests: far-camera guard, non-square row step, items
    drawn twice around a split). The survivor "unknown `map_layer` drawn anyway" is equivalent in Debug: the
    assert fires first (it is the Release fallback). PC backend mutant (diagonal flip mapping swapped): killed by
    the read-back test.
  - On this PC `conformance_pc`'s `idle_input_is_zero` fails while a gamepad with stick drift is connected
    (unrelated to tiles); with `SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x0001/0x0001` it passes.
- [x] **T3.9** **3DS gate** (maintainer): bench on an Old 3DS — how many full 16×16 layers + 300 sprites at 60 Hz;
  the flip row matches the PC screenshot. Target ≥ 4 layers; below it, apply the D26 fallbacks before stage 4 —
  covers AC-4, FR-10 (drawing)
  - **Passed on the Old 3DS XL (2026-11-21)**, run by the PC through spec 013's remote (the maintainer only pressed
    Y), Development build: `bench: PASS 4 layers + 300 sprites, avg 16.7 ms, worst 17.4 ms, missed 0 of 600 frames`.
    Limits (300-frame trials, a frame over 25 ms = missed): **6 layers** with 300 sprites (7 layers: 31 missed, worst
    27.8 ms); **600 sprites** with 4 layers (700: 9 missed, worst 29.0 ms). The flips screenshot matches
    the PC's by eye (all 8 map orientations and 4 sprite flips, wall without seams); the bonus-level screenshot is right.
    Release would be the same or faster. No D26 fallback needed.

  **How to run it** (spec 013 stage 6): on the 3DS open the Homebrew Launcher and press **Y**; on the PC
  `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` (builds `3ds-dev`). It
  plays the bonus level, shows the flips and the benchmark, runs the gate (target trial, then the layer and sprite
  limit searches), saves screenshots of each in `build/logs/platformer-<stamp>/` and ends with `nnh: PASS` or
  `nnh: FAIL`; the console shows a large `GATE: PASS`/`GATE: FAIL` banner. About 1 minute.

## Stage 4 — loading converted maps

- [x] **T4.1** bounded validator of `.nnhmap` (header, version, sizes, offsets, CRC, counts vs limits, tile ids
  ≤ count, strings terminated, property/object ranges) — files: `tilemap/map_validate.{hpp,cpp}` — test:
  `test_map_validate.cpp` (a test-only writer makes valid and damaged buffers) — covers FR-1, FR-11
- [x] **T4.2** [P] fuzz target `tilemap_target` (validate + copy into a layout) — files: `tests/fuzz/fuzz_targets.*`,
  `libfuzzer_tilemap.cpp`, `test_fuzz_smoke.cpp`, `tests/CMakeLists.txt` — test: mini-fuzz in `nnh_tests` —
  covers AC-3
- [x] **T4.3** `load_tilemap`: asset → validate → copy with `LoadTilemapOptions` extras → tileset textures (errors
  name the image) — files: `tilemap/map_load.cpp` — test: `test_map_load.cpp` (fake assets: ok, not found,
  damaged, truncated, wrong version, texture missing), `test_map_edit_after_load.cpp` — covers FR-0, FR-1, FR-11

  **Result (2026-11-21, T4.1–T4.3):** 78 new unit tests (914/914 in pc-debug, 881 in pc-release; ctest 103/103):
  validator 28 + 26 (`test_map_validate.cpp`, `test_map_validate_content.cpp`), loading 17 (`test_map_load.cpp`),
  editing after load 6 (`test_map_edit_after_load.cpp`), mini-fuzz 1 (`tilemap_target_survives_mutated_inputs`:
  8000 mutants of two valid maps, > 100 accepted and queried). Test-only writer `tests/tilemap/map_file.*`
  (`sample`, `minimal`, `two_tilesets`, `from_map`, `seal`), fixture `load_fixture.hpp`. PC (Debug, Release) and
  `3ds-release` build without warnings. `fuzz_tilemap` (libFuzzer) is wired like the others but not run yet (VM 120,
  T8.1). Deviations in [plan.md](plan.md) ("Stage 4 refinements"): `map_copy.*` and `map_validate_content.cpp` /
  `map_validate_parts.hpp` added, fuzz body in `tests/fuzz/fuzz_tilemap.cpp`, header byte `owned_textures`, animated
  tiles refused until stage 6.
  - Mutation check (93 hand mutants of the validator, the copy and the loader): first run 70 killed; 20 real
    survivors got tests (a string capacity running past the end of the file — an out-of-bounds read —, x of the
    parallax origin, a 256-byte string, animation tables, zero-tile tilesets, two-tileset masks, name offsets of
    objects, object layers and properties, a `collision` layer of non-solid tiles, loaded strings overwritten by new
    ones, an image of another height, borrowed textures freed by a code-built map) or sharper expectations (property
    range reasons split into outside / overlap / without an owner). Final: **90 killed, 3 equivalent** (copy's
    `texture_slot = -1` and `owned_textures = 0`, overwritten by the loader before any use; copying the strings'
    final NUL into a zeroed block).

## Stage 5 — the Tiled converter

- [x] **T5.1** bounded JSON reader (depth ≤ 32, document ≤ 64 MiB, strings, numbers, escapes, UTF-8) + mini-fuzz
  — files: `tools/assetc/src/json.{hpp,cpp}` — test: `tools/assetc/tests/test_json.cpp` — covers FR-1, X
- [x] **T5.2** tilesets: `.tsj` and embedded, image inside the assets root, PNG size vs Tiled's numbers, margin,
  spacing, `solid`/`one_way`, tile properties; refusals (image collections, wang sets, tile size outside 8–64) —
  files: `tiled_tileset.*`, `png_size.*` — test: `test_tiled_tileset.cpp` — covers FR-1, FR-2, FR-7
- [x] **T5.3** maps: orientation/infinite/render checks, tile layers (CSV data; base64 without compression;
  zlib/zstd refused with "save the layer format as CSV"), flips from gids, layer offset/opacity/parallax/visible,
  `collision`, parallax origin, map and layer properties; refusals (image and group layers) — files:
  `tiled_map.*`, `tiled_properties.*` — test: `test_tiled_map.cpp` — covers FR-1, FR-2, FR-3, FR-6, FR-7
- [x] **T5.4** [P] objects: rectangles and points, name, class/type, properties; refusals (text, polygon,
  polyline, ellipse, tile objects — see plan question 3) — files: `tiled_objects.*` — test: `test_tiled_objects.cpp`
  — covers FR-1, FR-5, FR-6
- [x] **T5.5** writer + self-validation with `map_validate`, limits with messages, depfile, CLI and
  `file: reason` diagnostics — files: `map_writer.*`, `depfile.*`, `cli.*`, `main.cpp` — test:
  `test_map_writer.cpp`, `test_cli.cpp` — covers FR-1, FR-11
- [x] **T5.6** build integration: `.tmj` → `.nnhmap` step with `DEPFILE`, `.tsj`/`.tiled-*` not packed, index of
  output names (stale removal) — files: `cmake/nnh_assets.cmake`, `nnh_asset_index.cmake`, `nnh_host_tools.cmake`
  — test: ctest `asset_index_*` updated; editing a `.tsj` reconverts only its maps (checked with `ninja -d explain`)
  — covers FR-1, III
- [x] **T5.7** bad-file fixtures and ctest `map_convert_rejects_*` / `map_convert_explains_*` (list in the plan)
  — files: `tests/data/maps/`, `tests/CMakeLists.txt` — covers AC-3, FR-1, FR-11
- [x] **T5.8** [P] converter fuzzing: whole conversion from in-memory files (mini-fuzz) + `fuzz_assetc_json`,
  `fuzz_assetc_map` (libFuzzer) — files: `tools/assetc/tests/`, `tools/assetc/CMakeLists.txt` — covers AC-3, X
- [x] **T5.9** code vs file: the fixture `tests/data/maps/good.tmj` converted at build time and loaded, compared
  query by query with the same map built in code — files: `tests/tilemap/test_code_vs_file.cpp`,
  `tests/CMakeLists.txt` — covers FR-0, AC-2

  **Result (2026-11-21, T5.1–T5.9):** `nnh-assetc map <in.tmj> <out.nnhmap> --assets <dir> --depfile <d>` converts
  Tiled maps; `.tmj` assets become `.nnhmap` at build time. 93 tests in `nnh_assetc_tests` (JSON 17, fields 6, paths
  + PNG header + files 9, tilesets 16, maps 16, objects 7, writer 10, depfile 2, CLI 7, mini-fuzz 3: 8000 JSON and
  6000 map/tileset mutants, > 100 converted and validated), 4 code-vs-file tests in `nnh_tests` (918/918), ctest
  148/148 in pc-debug (new: `assetc_unit`, `map_convert_accepts_good`, 21 `map_convert_rejects_*` /
  `map_convert_explains_*` pairs, `asset_index_names_maps_by_their_output`), 147/147 in pc-release; pc-debug,
  pc-release, `3ds-dev` and `3ds-release` (converter built for the host by the ExternalProject) without warnings.
  `fuzz_assetc_json` / `fuzz_assetc_map` are wired like the others but not run yet (VM 120, T8.1). Deviations in
  [plan.md](plan.md) ("Stage 5 refinements").
  - **Budgets (III)**, Windows, three edits each: edit `examples/hello/main.cpp` → simulator 1.51–1.60 s before,
    1.48–1.52 s after; `.3dsx` 2.64–2.79 s before, 2.68–2.73 s after. With two maps added to a game for the check,
    touching a `.tsj` reconverted only the map using it, on PC and 3DS (`ninja -d explain`); touching an image used
    by both reconverted both; removing them removed the stale outputs. The first 3DS check found every map
    reconverted on every build (MSYS's CMake misread the depfile's `E:/...` paths): depfile paths are now relative to
    the working directory.
  - Mutation check (103 hand mutants of the JSON reader, fields, properties, tilesets, map, tile layers, objects,
    writer, paths, depfile, CLI and PNG header): first run 88 killed (4 of them by the build itself: `good.tmj` no
    longer converted); the 15 survivors were real and got tests (control character 0x1F, a high surrogate followed by
    a non-low `\u` escape, U+0800 encoding, image height mismatch, a 4-px image, tile height only, a tile listed
    twice, exactly 8192 ids, total cells over 4 Mi — now checked before any layer data is read —, layer height only,
    opacity over 1, too many CSV values and base64 bytes, a false bool property, a stale depfile hiding an unwritten
    one). Final: **103 killed, 0 equivalent**.

## Stage 6 — animated tiles (cheap; can be cut alone)

- [x] **T6.1** format and builder: animation table, `animated` flag; `Tilemap::set_tile_animation` (code) —
  files: `map_format.hpp`, `map_build.cpp`, `map_validate.cpp` — test: `test_map_build.cpp`, `test_map_validate.cpp`
  — covers FR-0, FR-2
- [x] **T6.2** current frame per animation and its use in expansion; default clock from `frame_count()` —
  files: `tilemap/tile_animation.{hpp,cpp}`, `tile_expand.cpp` — test: `test_tile_animation.cpp`,
  `test_tile_expand.cpp` — covers FR-2, FR-10
- [x] **T6.3** [P] converter: Tiled `animation` arrays (frame tile ids within the tileset, durations) and limits —
  files: `tiled_tileset.cpp` — test: `test_tiled_tileset.cpp` — covers FR-1

  **Result (2026-11-21, T6.1–T6.3):** animated tiles work in code, from Tiled, loaded, drawn and fuzzed. API:
  `TilemapDesc::max_animations`/`max_animation_frames` (0 by default), `Tilemap::set_tile_animation` (`count` 0
  removes), `tile_animation`, `animated_tile_id`, `TilemapDrawOptions::animation_time` (default `frame_count()` at
  60 Hz); [tilemap.md](../../docs/api/tilemap.md#animated-tiles). 36 new tests in `nnh_tests` (954/954 in pc-debug,
  921 in pc-release): builder and drawing 14 (`test_map_animations.cpp`), validator 10
  (`test_map_validate_animations.cpp`, replacing the old refusal test), pure clock/frame/lookup/table 8
  (`test_tile_animation.cpp`), expansion 2, the gfx clock 1, a loaded animated map 1, code vs file 1; the
  `tilemap_target` mini-fuzz gained an animated seed (12 000 mutants, > 30 accepted with animations) and runs
  expansion at two clocks and animation edits. 8 new tests in `nnh_assetc_tests` (101/101,
  `test_tiled_animations.cpp`); the converter mini-fuzz tileset has animations; ctest 150/150 (new pair
  `map_convert_{rejects,explains}_animation` on `bad_animation.tmj`; `good.tmj` animates its water tile), 149/149 in
  pc-release. PC (Debug, Release), `3ds-dev` and `3ds-release` build without warnings. The platformer's bonus level
  animates its water: screenshots at frames 20 and 30 differ only in the pit's water pixels.
  - Mutation check (68 hand mutants of the clock, frame lookup, table range, expansion, the draw/gfx clock, the
    builder, the validator, the converter's reader and writer): first run 49 killed of 56 run (12 more skipped by a
    line-ending slip in the script and run after); 5 real survivors got tests (an explicit time 0 taken as the
    default clock, animations of a second tileset, negative times, a frame run past the last frame, a removed
    animation leaving its flag), one exposed a redundant check (removed). Final: **64 killed, 3 equivalent** (the
    `animations == 0` early exit, zeroing freed frames and a freed entry beyond the counts), 1 removed with its code.
  Deviations in [plan.md](plan.md) ("Stage 6 refinements"): the builder in its own `map_animations.cpp`, the
  getters, defaults of 0, no total duration in the entry, the clock through gfx as an opaque `clock`, no load extras
  for animations.

## Stage 7 — the example

- [x] **T7.1** Tiled level: `assets/levels/level1.tmj`, `assets/tiles/world.tsj` from the art script (parallax
  background, ground with `collision`, foreground, one-way planks, water animation, coins, spawn, `music` property)
  — files: `examples/platformer/art/make_art.py`, `assets/` — test: the build converts it; it opens in Tiled
  (maintainer, once) — covers AC-1, FR-1
- [x] **T7.2** player and level scene: run, jump, drop through planks, coins by `overlaps` + `CoinCollected`,
  camera follow clamped to the map, HUD; switch to the bonus level — files: `examples/platformer/*.cpp` — test:
  PC screenshots of level 1 read back — covers AC-1, FR-3, FR-5, FR-6, FR-8, FR-9
- [x] **T7.3** `PLATFORMER_DEMO=replay`: scripted input, checksum log every 60 frames — files:
  `examples/platformer/replay.cpp` — test: two PC runs give the same log — covers FR-10

  **Result (2026-11-22, T7.1–T7.3):** `examples/platformer` plays level 1 made in Tiled and the bonus level built in
  code, as scenes that talk through events, with a replay checked against the console script. Deviations in
  [plan.md](plan.md) ("Stage 7 refinements"): scenes + a `Demos` switcher on events, level 1 first, the level written
  by `art/make_level.py`, a door object, a 900-frame replay over both levels started by X, an in-game 60 Hz verdict.
  - **T7.1**: `art/make_level.py` (called by `make_art.py`; the PNGs come out byte-identical) writes
    `assets/tiles/world.tsj` and `assets/levels/level1.tmj` (128×20, Tiled 1.10 JSON: `clouds` parallax 0.25 × 0.5,
    `hills` 0.5 × 1, `ground` with `collision`, `platforms` with planks marked `one_way` in the tileset, `things`
    with a `spawn` point, 47 `coin`s (`value`) and a `door` (`to`), `front` at 0.85 with the animated water and
    flipped tufts; properties `title`, `music`). The build converts it (28 KB `levels/level1.nnhmap`) on PC and 3DS;
    `load_tilemap("levels/level1")` loads it with its own tileset texture. **Not opened in Tiled yet** (maintainer).
  - **T7.2**: `level_play.*` (player through `collision_grid()` with `outside = Solid`, `follow_camera` clamped to
    the map on both axes, coins by `collision::overlaps` → `CoinCollected`, back to the spawn after the water →
    `LevelRestarted`, doors → `ShowDemo`), `level_scene.*` (buttons, or the replay), `hud.*` (score and coins from the
    events; title and music from the map's properties), `demos.*` (SELECT / START / `ShowDemo`), `main.cpp`
    (`nnh::run`). Screenshots read back: level 1 at the start, scrolled (hills and clouds behind at their parallax),
    on the bridge with the stone steps; the bonus level; flips; HUD on the bottom screen.
  - **T7.3**: `replay_run.*` (pure: steps, checkpoints every 60 frames, FNV-1a checksum, per-level meters),
    `replay_script.*` (the inputs: level 1 600 frames — plank, two pits, under the bricks, the bridge, Down + A
    through it, the stone steps; bonus 300 — a jump into the bricks' underside, a pit), `replay.*` (log, `ShowDemo`
    between levels, panel and banner). Two PC runs: the 15 checksum lines are identical (only the times in the
    verdict differ); ctest `platformer_replay_matches_script` replays it on every test run against the lines of
    `tests/level.nnhs` (a changed checksum in the script makes it fail: checked).
  - Tests: 48 new in `nnh_tests` (1002/1002): camera 5 (`test_camera_follow.cpp`), coins 10 (`test_coin_book.cpp`),
    60 Hz meter 6 (`test_frame_meter.cpp`), player physics 8 (`test_player.cpp`), replay 16 (`test_replay_run.cpp`),
    replay script 3 (`test_replay_script.cpp`); each seen failing first (stubs, or the old `Player::update`
    signature). ctest 151/151 in pc-debug.
- [ ] **T7.4** **3DS** (maintainer): level 1 and the bonus level at a steady 60 Hz while scrolling (overlay), replay
  checksums equal to the PC's, flips and parallax as on PC; benchmark number for the docs — covers AC-1, AC-4, FR-10

  **Prepared (2026-11-22), not run yet.** Everything is automatic: on the 3DS open the Homebrew Launcher and press
  **Y**; on the PC `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs`
  (builds `3ds-dev`). It plays level 1 with the buttons (screenshots `level1-start`, `level1-parallax`,
  `level1-bridge`), the bonus level (`bonus`) and the flip test (`flips`), then presses X: the replay must log the
  PC's 15 checksum lines exactly and end with `replay: done 900 frames sum 26B21D57: 60 Hz PASS (...)` (a frame over
  25 ms = missed; the verdict is on both screens too, screenshot `replay-result`). `nnh: PASS` or `nnh: FAIL`; about
  40 s. In the simulator: `nnh: PASS (73 lines, frames 1-1553)`. The benchmark number for the docs is the T3.9 gate's
  (`tests/gate.nnhs`, still valid with the new demo order).

## Stage 8 — verification and docs

- [x] **T8.1** Linux on VM 120: gcc build + ctest; clang ASan/UBSan for `nnh_tests` and `nnh_assetc_tests`;
  `fuzz_tilemap`, `fuzz_assetc_json`, `fuzz_assetc_map` 60 s each without a crash; `3ds-release` there with the
  host-built converter — covers AC-3, FR-10
  - Done on VM 120 (2026-11-22), except `3ds-release` there (not run; the Windows `3ds-dev`/`3ds-release` builds
    use the same host-built converter). gcc 14 `pc-debug`: ctest 151/151; it showed three `-Wdangling-reference`
    false positives (a reference into the map buffer taken through a temporary `MapView`), silenced by naming the
    view (`map_tiles.cpp`, `test_map_writer.cpp`). clang 18 ASan/UBSan: `nnh_tests` 1002/1002, `nnh_assetc_tests`
    101/101, `nnh_tool_tests` 156/156, clean. libFuzzer 60 s each, empty corpus: `fuzz_tilemap` 1.83 M runs cov 66,
    `fuzz_assetc_json` 1.25 M runs cov 425, `fuzz_assetc_map` 475 k runs cov 775; `fuzz_tilemap` again 120 s seeded
    with the converted `good`, `good_check` and `level1` maps: cov 831, ft 1737. No crash anywhere.
- [x] **T8.2** `docs/api/collision.md` and `docs/api/tilemap.md` (API, limits, memory formula and examples,
  `max_tiles`, benchmark number) + header comments — covers AC-5, FR-11, AC-4
- [x] **T8.3** [P] guide `docs/guides/levels.md`: a level in Tiled for NNHGE (settings: CSV, finite,
  orthogonal; `solid`/`one_way`/`collision`; objects; parallax; what the build refuses) and the same in code —
  covers AC-5
- [x] **T8.4** [P] architecture and tech docs: `rendering.md` (tile batches, from design to implemented),
  `decisions.md` (D26, D27 confirmed), `project-structure.md` (`tools/assetc`), `security.md` (new parsers),
  `docs/tech/cmake.md` (map step, host tools, `NNH_ASSETC`, `NNH_HOST_CXX`), `devkitpro-3ds.md` (citro2d quads),
  `docs/api/gfx.md`, `app.md`, `assets.md` (`.tmj` assets) — covers AC-5
- [x] **T8.5** [P] `CLAUDE.md` (modules, layout, build), `README.md` status, `docs/REFERENCES.md` (Tiled format) —
  covers AC-5
- [x] **T8.6** build budgets: one game `.cpp` edit (PC, 3DS), one map edit, clean engine; ROADMAP baseline table —
  covers III
- [x] **T8.7** `docs/ROADMAP.md` M7 checkbox and benchmark line; spec, plan, tasks status → implemented;
  `specs/README.md` — covers AC-5

  **Result (2026-11-22, T8.2–T8.7):** docs checked against the code; `docs_tool.py check` 0 broken links; pc-debug
  1002/1002 unit tests, ctest 151/151; `3ds-release` builds without warnings.
  - **T8.2**: status → implemented and the Old 3DS benchmark in [tilemap.md](../../docs/api/tilemap.md#drawing),
    `<nnh/tilemap.hpp>` (`draw_tilemap`), `<nnh/app.hpp>` (`max_tiles`: ~20 KB, not ~22); stale "stage 3" and
    "later a tilemap's grid" header comments fixed.
  - **T8.3**: new guide [levels.md](../../docs/guides/levels.md) (files, map settings, tilesets, collision, layers
    and parallax, animations, objects and properties, loading, the build's refusals with the fixtures' messages,
    the same level in code), linked from getting-started, README, tilemap.md and assets.md.
  - **T8.4**: rendering.md (gate result, animated tiles), decisions.md (D26/D27 confirmed; D26's quads are 10 bytes,
    not 12), project-structure.md (`tests/data/`, shared pure tilemap files), security.md (threat row for Tiled files,
    T8.1 noted), cmake.md (converter check cost, build-speed line), devkitpro-3ds.md (flips confirmed on hardware,
    tile cost), gfx.md (benchmark, SELECT twice to reach it), app.md (`FixedStep` exists: the "planned" line was
    false), assets.md.
  - **T8.5**: CLAUDE.md (one shorter paragraph for collision + tilemaps), README (status, guide, budgets sentence),
    REFERENCES.md (Tiled JSON format, gids, parallax).
  - **T8.6**: measured on Windows (three runs each), in the [ROADMAP](../../docs/ROADMAP.md) table: `hello` edit →
    simulator 1.41–1.52 s, `.3dsx` 2.61–2.78 s; platformer `.cpp` edit → simulator 1.73 s, `.3dsx` 3.92–4.07 s; map
    edit → PC 0.92–0.95 s (conversion 0.21–0.23 s), `.3dsx` 3.21–3.30 s; clean 3DS engine 11.4 s. A game with maps
    pays ~1 s on every 3DS build for the host converter's up-to-date check.
  - **T8.7**: ROADMAP M7 ticked; spec, plan, tasks and specs/README → implemented, with T7.4 and T8.1 still open.
