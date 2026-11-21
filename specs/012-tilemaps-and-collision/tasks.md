# Tasks 012: Tilemaps and collision

Status: approved (2026-11-21); next: stage 0
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task. Every stage ends with
something verifiable on PC (`ctest --preset pc-debug`, a screenshot); console steps are marked **3DS** and are run
by the maintainer.

## Stage 0 — groundwork

- [ ] **T0.1** spike: a minimal `tools/assetc` (prints its version) built by `cmake/nnh_host_tools.cmake` — a
  target in `pc-debug`, an `ExternalProject` with `NNH_HOST_CXX` in `3ds-release` through `scripts/dkp.sh` on
  Windows and on VM 120 (and `-DNNH_ASSETC=<path>` override); measure the 3DS no-op build before/after — files:
  `tools/assetc/CMakeLists.txt`, `tools/assetc/src/main.cpp`, `cmake/nnh_host_tools.cmake`, `scripts/dkp.sh`,
  `CMakeLists.txt` — test: both presets build it; result written in this file — covers FR-1 (risk: host compiler)
- [ ] **T0.2** [P] CRC-32 out of `fs/save_format.cpp` into pure `core/crc32.{hpp,cpp}` (no engine includes) —
  test: existing save tests + `tests/core/test_crc32.cpp` (known vectors) — covers FR-1, FR-11 (shared by map
  reader and converter)

## Stage 1 — collision (pure, no tilemap)

- [ ] **T1.1** `<nnh/collision.hpp>` skeleton; `overlaps`, `contains`, `intersection`, `separation` — files:
  `collision/aabb.cpp` — test: `tests/collision/test_aabb.cpp` — covers FR-9
- [ ] **T1.2** `Grid`, `grid_view`, cell lookup with `outside`, cell range of a box (half-open edges) — files:
  `collision/grid.cpp` — test: `test_grid.cpp` — covers FR-8, FR-9
- [ ] **T1.3** `move`: x then y sweep, slide, touching sides, start-overlapping rule, huge deltas, non-zero origin,
  non-square cells, NaN/inf delta asserts — files: `collision/grid_move.cpp` — test: `test_grid_move.cpp` — covers FR-8
- [ ] **T1.4** one-way cells and `drop_through` — test: `test_one_way.cpp` — covers FR-7, FR-8
- [ ] **T1.5** [P] `contacts`, `cells_touching` — files: `collision/grid_query.cpp` — test: `test_contacts.cpp`,
  `test_cells_touching.cpp` — covers FR-8, FR-9

Verifiable: `nnh_tests collision` green on PC.

## Stage 2 — tilemaps built in code

- [ ] **T2.1** binary format constants, limits and cell bits; layout from capacities with the memory formula —
  files: `tilemap/map_format.hpp`, `tilemap/map_layout.{hpp,cpp}` — test: `tests/tilemap/test_map_layout.cpp`
  (offsets aligned, sizes = formula) — covers FR-11
- [ ] **T2.2** `Tilemap::create`, move, `reset`, memory tag `tilemap`, `LimitReached`/`InvalidArgument` — files:
  `include/nnh/tilemap.hpp`, `tilemap/tilemap.cpp` — test: `test_map_build.cpp` — covers FR-0, FR-11
- [ ] **T2.3** `add_tileset` (margin, spacing, tile count, first ids, source table), `add_tile_layer`, layer
  settings — files: `tilemap/map_build.cpp` — test: `test_map_build.cpp` — covers FR-0, FR-2, FR-3
- [ ] **T2.4** tiles and coordinates: `tile`, `tile_at`, `set_tile`, `fill`, `cell_rect`, `world_to_cell`,
  flips encode/decode — files: `tilemap/map_tiles.cpp` — test: `test_map_tiles.cpp` — covers FR-2, FR-4
- [ ] **T2.5** [P] properties: string pool, `Properties` view, setters for map/layer/tile/object — files:
  `tilemap/map_properties.cpp` — test: `test_map_properties.cpp` — covers FR-0, FR-6
- [ ] **T2.6** [P] object layers and objects: add, list, find by name/type — files: `tilemap/map_objects.cpp`,
  `map_build.cpp` — test: `test_map_objects.cpp` — covers FR-0, FR-5
- [ ] **T2.7** collision: tile flags, `collision` layers, contributing-layer mask, `collision_at`,
  `collision_grid` → `collision::move` on a map — files: `tilemap/map_collision.cpp` — test:
  `test_map_collision.cpp` — covers FR-4, FR-7, FR-8

Verifiable: `nnh_tests tilemap` green; a whole level can be built and collided against in a test.

## Stage 3 — drawing (the gate)

- [ ] **T3.1** contract `TileQuad2D`, `TileBatch2D`, `draw_tiles`; fake records batches, quads and call order —
  files: `contract/nnh/platform/video.hpp`, `platforms/fake/src/video_fake.cpp`, `fake_internal.hpp`,
  `platform_fake.hpp` — test: `tests/platform_conformance/test_contract.cpp` (fake) — covers FR-3
- [ ] **T3.2** visible range and parallax (Tiled's centre-based formula, snapped origin) — files:
  `tilemap/visible_range.{hpp,cpp}` — test: `test_visible_range.cpp` — covers FR-3, AC-2
- [ ] **T3.3** tile expansion: cells → quads, source from the table, flip bits, empty skipped, per-tileset pass —
  files: `tilemap/tile_expand.{hpp,cpp}` — test: `test_tile_expand.cpp` — covers FR-2, FR-3
- [ ] **T3.4** gfx tile batches: command kind, `build()` split points, records with `{expand fn, source}`,
  scratch of 1600 quads, `max_tiles` (+ `AppConfig`, `Stats`, overlay), drops, forget-by-source on map
  destruction — files: `gfx/tile_batches.{hpp,cpp}`, `gfx/draw_list.*`, `gfx/gfx_frame.cpp`, `gfx/gfx_state.hpp`,
  `include/nnh/app.hpp`, `include/nnh/gfx.hpp`, `debug/overlay.cpp` — test: `tests/gfx/test_tile_batches.cpp`,
  `test_draw_list.cpp` — covers FR-3, FR-4, FR-11
- [ ] **T3.5** `gfx::draw_tilemap` (one map layer or all, draw layer, opacity, offset, ignore_camera) — files:
  `tilemap/map_draw.cpp` — test: `test_map_draw.cpp` (fake: batches interleaved with sprites, `set_tile` after
  draw is seen, no allocation over 100 frames in `test_tilemap_alloc.cpp`) — covers FR-3, FR-4, FR-11
- [ ] **T3.6** PC backend — files: `platforms/pc/src/draw_tiles_pc.cpp` — test: conformance pc (render the 8
  orientations of an asymmetric 8×8 texture offscreen, read pixels back) — covers FR-2, FR-3
- [ ] **T3.7** [P] 3DS backend + `C2D_Init` with `max_tiles` — files: `platforms/3ds/src/draw_tiles_3ds.cpp`,
  `app_3ds.cpp` — test: `3ds-release` builds — covers FR-3
- [ ] **T3.8** `examples/platformer` skeleton: art script (tileset, player, backdrops), the **bonus level built in
  code**, camera follow, `PLATFORMER_DEMO=bench` and `=flips` — files: `examples/platformer/{CMakeLists.txt,
  main.cpp, bonus_level.cpp, bench.cpp, art/make_art.py, assets/...}`, `examples/CMakeLists.txt` — test: PC
  screenshots (bonus, bench, flips) read back — covers FR-0, FR-3, AC-1 (part), AC-4
- [ ] **T3.9** **3DS gate** (maintainer): bench on an Old 3DS — how many full 16×16 layers + 300 sprites at 60 Hz;
  the flip row matches the PC screenshot. Target ≥ 4 layers; below it, apply the D26 fallbacks before stage 4 —
  covers AC-4, FR-10 (drawing)

## Stage 4 — loading converted maps

- [ ] **T4.1** bounded validator of `.nnhmap` (header, version, sizes, offsets, CRC, counts vs limits, tile ids
  ≤ count, strings terminated, property/object ranges) — files: `tilemap/map_validate.{hpp,cpp}` — test:
  `test_map_validate.cpp` (a test-only writer makes valid and damaged buffers) — covers FR-1, FR-11
- [ ] **T4.2** [P] fuzz target `tilemap_target` (validate + copy into a layout) — files: `tests/fuzz/fuzz_targets.*`,
  `libfuzzer_tilemap.cpp`, `test_fuzz_smoke.cpp`, `tests/CMakeLists.txt` — test: mini-fuzz in `nnh_tests` —
  covers AC-3
- [ ] **T4.3** `load_tilemap`: asset → validate → copy with `LoadTilemapOptions` extras → tileset textures (errors
  name the image) — files: `tilemap/map_load.cpp` — test: `test_map_load.cpp` (fake assets: ok, not found,
  damaged, truncated, wrong version, texture missing), `test_map_edit_after_load.cpp` — covers FR-0, FR-1, FR-11

## Stage 5 — the Tiled converter

- [ ] **T5.1** bounded JSON reader (depth ≤ 32, document ≤ 64 MiB, strings, numbers, escapes, UTF-8) + mini-fuzz
  — files: `tools/assetc/src/json.{hpp,cpp}` — test: `tools/assetc/tests/test_json.cpp` — covers FR-1, X
- [ ] **T5.2** tilesets: `.tsj` and embedded, image inside the assets root, PNG size vs Tiled's numbers, margin,
  spacing, `solid`/`one_way`, tile properties; refusals (image collections, wang sets, tile size outside 8–64) —
  files: `tiled_tileset.*`, `png_size.*` — test: `test_tiled_tileset.cpp` — covers FR-1, FR-2, FR-7
- [ ] **T5.3** maps: orientation/infinite/render checks, tile layers (CSV data; base64 without compression;
  zlib/zstd refused with "save the layer format as CSV"), flips from gids, layer offset/opacity/parallax/visible,
  `collision`, parallax origin, map and layer properties; refusals (image and group layers) — files:
  `tiled_map.*`, `tiled_properties.*` — test: `test_tiled_map.cpp` — covers FR-1, FR-2, FR-3, FR-6, FR-7
- [ ] **T5.4** [P] objects: rectangles and points, name, class/type, properties; refusals (text, polygon,
  polyline, ellipse, tile objects — see plan question 3) — files: `tiled_objects.*` — test: `test_tiled_objects.cpp`
  — covers FR-1, FR-5, FR-6
- [ ] **T5.5** writer + self-validation with `map_validate`, limits with messages, depfile, CLI and
  `file: reason` diagnostics — files: `map_writer.*`, `depfile.*`, `cli.*`, `main.cpp` — test:
  `test_map_writer.cpp`, `test_cli.cpp` — covers FR-1, FR-11
- [ ] **T5.6** build integration: `.tmj` → `.nnhmap` step with `DEPFILE`, `.tsj`/`.tiled-*` not packed, index of
  output names (stale removal) — files: `cmake/nnh_assets.cmake`, `nnh_asset_index.cmake`, `nnh_host_tools.cmake`
  — test: ctest `asset_index_*` updated; editing a `.tsj` reconverts only its maps (checked with `ninja -d explain`)
  — covers FR-1, III
- [ ] **T5.7** bad-file fixtures and ctest `map_convert_rejects_*` / `map_convert_explains_*` (list in the plan)
  — files: `tests/data/maps/`, `tests/CMakeLists.txt` — covers AC-3, FR-1, FR-11
- [ ] **T5.8** [P] converter fuzzing: whole conversion from in-memory files (mini-fuzz) + `fuzz_assetc_json`,
  `fuzz_assetc_map` (libFuzzer) — files: `tools/assetc/tests/`, `tools/assetc/CMakeLists.txt` — covers AC-3, X
- [ ] **T5.9** code vs file: the fixture `tests/data/maps/good.tmj` converted at build time and loaded, compared
  query by query with the same map built in code — files: `tests/tilemap/test_code_vs_file.cpp`,
  `tests/CMakeLists.txt` — covers FR-0, AC-2

## Stage 6 — animated tiles (cheap; can be cut alone)

- [ ] **T6.1** format and builder: animation table, `animated` flag; `Tilemap::set_tile_animation` (code) —
  files: `map_format.hpp`, `map_build.cpp`, `map_validate.cpp` — test: `test_map_build.cpp`, `test_map_validate.cpp`
  — covers FR-0, FR-2
- [ ] **T6.2** current frame per animation and its use in expansion; default clock from `frame_count()` —
  files: `tilemap/tile_animation.{hpp,cpp}`, `tile_expand.cpp` — test: `test_tile_animation.cpp`,
  `test_tile_expand.cpp` — covers FR-2, FR-10
- [ ] **T6.3** [P] converter: Tiled `animation` arrays (frame tile ids within the tileset, durations) and limits —
  files: `tiled_tileset.cpp` — test: `test_tiled_tileset.cpp` — covers FR-1

## Stage 7 — the example

- [ ] **T7.1** Tiled level: `assets/levels/level1.tmj`, `assets/tiles/world.tsj` from the art script (parallax
  background, ground with `collision`, foreground, one-way planks, water animation, coins, spawn, `music` property)
  — files: `examples/platformer/art/make_art.py`, `assets/` — test: the build converts it; it opens in Tiled
  (maintainer, once) — covers AC-1, FR-1
- [ ] **T7.2** player and level scene: run, jump, drop through planks, coins by `overlaps` + `CoinCollected`,
  camera follow clamped to the map, HUD; switch to the bonus level — files: `examples/platformer/*.cpp` — test:
  PC screenshots of level 1 read back — covers AC-1, FR-3, FR-5, FR-6, FR-8, FR-9
- [ ] **T7.3** `PLATFORMER_DEMO=replay`: scripted input, checksum log every 60 frames — files:
  `examples/platformer/replay.cpp` — test: two PC runs give the same log — covers FR-10
- [ ] **T7.4** **3DS** (maintainer): level 1 and the bonus level at a steady 60 Hz while scrolling (overlay), replay
  checksums equal to the PC's, flips and parallax as on PC; benchmark number for the docs — covers AC-1, AC-4, FR-10

## Stage 8 — verification and docs

- [ ] **T8.1** Linux on VM 120: gcc build + ctest; clang ASan/UBSan for `nnh_tests` and `nnh_assetc_tests`;
  `fuzz_tilemap`, `fuzz_assetc_json`, `fuzz_assetc_map` 60 s each without a crash; `3ds-release` there with the
  host-built converter — covers AC-3, FR-10
- [ ] **T8.2** `docs/api/collision.md` and `docs/api/tilemap.md` (API, limits, memory formula and examples,
  `max_tiles`, benchmark number) + header comments — covers AC-5, FR-11, AC-4
- [ ] **T8.3** [P] guide `docs/guides/levels.md`: a level in Tiled for NNHGE (settings: CSV, finite,
  orthogonal; `solid`/`one_way`/`collision`; objects; parallax; what the build refuses) and the same in code —
  covers AC-5
- [ ] **T8.4** [P] architecture and tech docs: `rendering.md` (tile batches, from design to implemented),
  `decisions.md` (D26, D27 confirmed), `project-structure.md` (`tools/assetc`), `security.md` (new parsers),
  `docs/tech/cmake.md` (map step, host tools, `NNH_ASSETC`, `NNH_HOST_CXX`), `devkitpro-3ds.md` (citro2d quads),
  `docs/api/gfx.md`, `app.md`, `assets.md` (`.tmj` assets) — covers AC-5
- [ ] **T8.5** [P] `CLAUDE.md` (modules, layout, build), `README.md` status, `docs/REFERENCES.md` (Tiled format) —
  covers AC-5
- [ ] **T8.6** build budgets: one game `.cpp` edit (PC, 3DS), one map edit, clean engine; ROADMAP baseline table —
  covers III
- [ ] **T8.7** `docs/ROADMAP.md` M7 checkbox and benchmark line; spec, plan, tasks status → implemented;
  `specs/README.md` — covers AC-5
