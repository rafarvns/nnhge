# Tasks 025: Pathfinding on grids

Status: approved 2026-11-24 (overnight pre-approval)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The work happens in a **separate git worktree** (branch `spec-025`) and is merged after specs 022 and 023;
follow plan.md's "Merge notes" for every shared file (one block per CMake file at the stated anchor, never the
alphabetical lists; **nothing** under `examples/platformer/`). Maps first, then the reference code the tests compare
against, the one-shot search and its total order, bounds/partial/slicing, smoothing and the follower, fields, AC-2's
random references and AC-3's golden, the example and the ARM pre-check (T7.6) before the first console run. Stage 9 is
run on the Linux build VM by the lead; the only console stage is stage 10: two scripted runs the maintainer only starts
(Homebrew Launcher → Y). Deviations found while building go into plan.md as "Stage N refinements", as in plans
012–023.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release without a warning;
`hello` sizes and `nm` list equal T0.1's; `git diff --exit-code engine/core/include/nnh/collision.hpp
engine/core/include/nnh/tilemap.hpp examples/platformer`; ctest `platformer_replay_matches_script` with the lines in
the tree; `queries: PASS 4ACAA177`, `entities: PASS C8FB9A41`, `prefabs: PASS 7BB7901E` (their ctests).

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-025/`, not committed): `hello.exe`
  (pc-release, pc-debug) and `hello.elf`/`.3dsx` (3ds-release, 3ds-dev) sizes and `nm -C` lists; `.text` of the
  3ds-release `queries`, `entities`, `prefabs` and `platformer` ELFs; edit `examples/queries/playground.cpp` (or the
  example's main scene file) → simulator and `.3dsx` times (the reference for the pathfinding example's edit); clean
  `nnh_core`; preprocessed lines and `-fsyntax-only` time of one-line TUs including `<nnh/collision.hpp>`,
  `<nnh/collision.hpp>` + `<nnh/result.hpp>`, and `<nnh/tilemap.hpp>`; 3ds-dev disassembly of
  `collision::detail::first_cell` and `collision::cell_at` saved in `build/ref-025/dis0/`. Files: plan.md ("Stage 0
  results"). Covers III, FR-24, AC-8.
- [x] **T0.2** [P] Reference screenshots for AC-8 in `build/ref-025/ref/`: `hello`, `queries` (`QUERIES_DEMO=all` at
  30), `entities`, `prefabs` (room at 60), platformer level 1 at 90 and `PLATFORMER_DEMO=replay` at 64 and 312;
  `build/ref-025/compare.sh` retakes them into `now/`. Files: plan.md. Covers AC-6, AC-8.
- [x] **T0.3** [P] Goldens and counts today: the replay ctest's last line as in the tree, `4ACAA177`, `C8FB9A41`,
  `7BB7901E`, `5201E6CF`; `nnh_tests` count against `kMaxTests` (4096; this spec adds ≈ 260 tests: raise only if
  fewer than 400 are left, and note it for the merge). Files: plan.md. Covers VI.

## Stage 1: path maps

- [x] **T1.1** The header skeleton and map creation: `<nnh/pathfinding.hpp>` (`CellPos`, `CellRect`, `kPathBlocked`,
  `kUnreachable`, `Diagonals`, `PathMapDesc`, `PathWeights`, `PathMap`), `path_block.hpp` (padded block, index
  helpers, neighbour offsets, users count, doomed flag, the `path` tag), `path_check.{hpp,cpp}` (misuse reporting),
  `map_create.cpp` (validation, the 64-bit check of `W × H × diagonal × 259 < 2³²`, allocation, move/destroy,
  `path_map_desc`). Test: `tests/pathfinding/test_path_map_create.cpp` (sizes 1/1024 accepted, 0/1025 refused; costs,
  agent size, cell size; the bound's edge both ways; a new map all weight 1, outside blocked; tag stats = header + P (+
  W·H for agent 2); move leaves the source invalid). Files: those, `engine/core/CMakeLists.txt` (the
  `_nnh_pathfinding_sources` block after the `_nnh_query_sources` block, `-ffp-contract=off`), `tests/CMakeLists.txt`
  (025's block before `add_test(NAME unit ...)`). Covers FR-1, FR-2, FR-6, FR-24.
- [x] **T1.2** Sync: `map_sync.cpp` — whole map and region, cells read once with `collision::detail::cell_at` (region
  offset; `outside` beyond the grid), Empty/Solid/OneWay and `one_way_walkable`, the weight callback (once per walkable
  cell, grid coordinates, row-major; bad values assert, Release clamps). Test: `tests/pathfinding/test_path_map_sync.cpp`
  (array grid; callback grid; a `Tilemap` built in code with `collision_grid()` and two masks (boats, walkers); region
  offset with cells past the grid's edges and `outside` Solid/Empty; the callback counted and never called after sync;
  a region sync changes only its rectangle). Covers FR-3, FR-23.
- [x] **T1.3** [P] Edits and queries: `map_edit.cpp` — `set_cell`, `fill` (clipped), `set_diagonals`, `blocked`,
  `weight`, `version` (+1 per call), size and cost accessors. Test: `tests/pathfinding/test_path_map_edit.cpp`. Covers
  FR-2, FR-4.
- [x] **T1.4** Footprints: `footprint.{hpp,cpp}` — erosion of a rectangle for agent sizes 2–4 (blocked if any square
  cell is blocked or outside, else the square's largest weight), the affected rectangle of a change; sync, `set_cell`
  and `fill` through it. Test: `tests/pathfinding/test_footprint.cpp` (sizes 1–4 against a brute-force erosion on 200
  seeded maps; the last k − 1 columns/rows blocked; partial sync and a `set_cell` next to a changed cell recompute
  exactly the overlapping cells). Covers FR-5.
- [x] **T1.5** [P] Geometry: `map_geometry.cpp` — `contains`, `cell_center` (the k × k footprint's centre), `cell_at`
  (the anchor of an agent centred at the point, through `collision::detail::first_cell`), `cells_to_points`. Test:
  `tests/pathfinding/test_path_points.cpp` (`cell_at` = `collision::cell_at` on the same grid for 10 000 seeded points
  at agent 1, its anchor shift at agents 2–4, `cell_center` ∘ `cell_at` = identity, NaN points (assert; Release
  `{-1, -1}`), the buffer contract). Covers FR-8.
- [x] **T1.6** Stage end: the stage-end guards; include check (`<nnh/pathfinding.hpp>` pulls only `<cstdint>`,
  `nnh/math.hpp`, `nnh/collision.hpp`, `nnh/result.hpp` and what they include; preprocessed lines and `-fsyntax-only`
  against T0.1's `collision.hpp` + `result.hpp`, within 0.05 s). Files: plan.md ("Stage 1 results"). Covers FR-24,
  III.

## Stage 2: the search (one-shot) and its total order

- [x] **T2.1** The references (test code first): `tests/pathfinding/path_reference.{hpp,cpp}` — ASCII maps (`#`, `.`,
  digits, `S`, `G`), seeded random maps (FNV-1a of seed, x, y, salt; sizes, blocked ratio, weights, modes, agent
  sizes), the reference Dijkstra (`std::priority_queue`), the **literal reference A*** (a `std::vector` open set
  scanned for the minimum `(f, h, −seq)`, decrease-key in place, FR-10's neighbour order, partial best by a final scan),
  the multi-source reference for fields, a path validator (adjacent steps allowed by the mode, entered cells walkable,
  summed cost), a sampled segment checker (points every 1/64 cell plus exact corner tests), brute-force erosion. Test:
  `tests/pathfinding/test_reference_self.cpp` (the references on hand-computed tiny cases agree with each other and with
  the expected numbers). Covers VI, AC-2.
- [x] **T2.2** Neighbours and heuristic: `neighbors.hpp`, `heuristic.hpp` (inline). Test:
  `tests/pathfinding/test_neighbors.cpp` (order N, E, S, W, NE, SE, SW, NW; offsets from the stride; a diagonal gap
  between two walls passes only with Always; one wall passes with AtLeastOneWalkable; OnlyIfNoObstacles never cuts;
  Never has 4; octile/Manhattan values; `(h × scale) >> 4` for 16, 24, 64). Covers FR-6, FR-9.
- [x] **T2.3** [P] The open heap: `open_heap.{hpp,cpp}` — 16-byte entries, 64-bit `(f, h)` key then `seq` descending,
  hole sifts, the best-child buffer, lazy staleness, compaction and heapify (cold). Test:
  `tests/pathfinding/test_open_heap.cpp` (pop order = a sorted reference for 10 000 random keys with many ties; the same
  with the buffer in front; stale entries skipped; compaction through a test hook that shrinks the capacity gives the
  same pop order). Covers FR-10, FR-12.
- [x] **T2.4** Searchers and `find`: `search_internal.hpp`, `searcher_create.cpp` (create, 8-byte records, stamps,
  move/destroy), `search_begin.cpp` (checks, start and goal rules, finish: status, cost, length), `search_step.cpp` (the
  expansion loop, one template per `Diagonals` mode, unrolled), `search_path.cpp` (`path`, `path_points`). Test:
  `tests/pathfinding/test_find_basic.cpp` (Found/NoPath/Invalid; from == to walkable and blocked; blocked start leaves;
  blocked goal → NoPath, 0 expanded; path buffers: exact fit, too small keeps the first cells, null + 0 counts, readable
  until the next search), `tests/pathfinding/test_find_costs.cpp` (step × weight of entered cells, the start's weight
  never counts, every mode; Found cost = the reference Dijkstra's on 500 seeded maps; scales 24/64 within their bound).
  Covers FR-7, FR-8, FR-9, FR-11.
- [x] **T2.5** The total order: Test: `tests/pathfinding/test_tie_order.cpp` (hand-drawn ties with their expected cells
  — open room straight and diagonal, a symmetric pillar, equal weights both ways, a parent kept on an equal g — and the
  literal reference A* giving the same status, cost, expansions and cells on 300 seeded maps of every mode). Covers
  FR-10.
- [x] **T2.6** Stage end: the stage-end guards; `hello` unchanged (no new symbol); a probe linking only `PathMap` and
  `find` builds for 3ds-release. Files: plan.md ("Stage 2 results"). Covers FR-24, AC-8.

## Stage 3: bounds, partial paths, slicing, reuse, inspection, misuse

- [x] **T3.1** Limits and partial paths: the limit checked before each pop (the goal as the limit-th pop is Found),
  LimitReached, partial tracking at every g assignment (unscaled h, then g, then row-major index), Partial for an
  unreachable goal, a blocked goal and a limit. Test: `tests/pathfinding/test_limits_partial.cpp` (every status; crafted
  ties for the closest cell; an island's shore; a comb map keeping ≈ 4/5 of its cells open never overflows; without the
  option NoPath/LimitReached with 0 cells). Covers FR-11, FR-12, FR-13.
- [x] **T3.2** Time slicing: `begin`/`step`/`cancel`/`running`/`result` (`Running`), stale at the end, the map's users
  count and the doomed block. Test: `tests/pathfinding/test_slicing.cpp` (slices of 1, 7, 64 and the whole equal `find`
  on 300 seeded searches; cancel; begin while running restarts; an edit mid-search → stale and the search continues
  over current cells; the map moved mid-search → the same result; destroyed mid-search → assert, Release path `Invalid`
  and the block freed by the searcher (allocation counter back to its start)). Covers FR-14, FR-23.
- [x] **T3.3** [P] Reuse: Test: `tests/pathfinding/test_searcher_reuse.cpp` (10 000 searches on one searcher equal
  fresh searchers'; maps smaller than the searcher; across the stamp wrap via `detail::path_searcher_set_stamp`; two
  searchers sliced over one map, interleaved; `static_assert`s: record 8, entry 16). Covers FR-7.
- [x] **T3.4** [P] Inspection: `reached`, `expanded`, `cost_to` during a sliced search and after. Test:
  `tests/pathfinding/test_inspect.cpp`. Covers FR-20.
- [x] **T3.5** Misuse and cost: every row of plan.md's misuse table for maps and searchers, asserting (Debug) and on
  the Release path (pc-release run). Test: `tests/pathfinding/test_path_misuse.cpp`; `tests/pathfinding/test_path_cost.cpp`
  (no allocation after creation over 10 000 operations, no log line, `events::stats()` unchanged, the weight callback
  the only callback). Covers FR-21, FR-22, FR-23.
- [x] **T3.6** Stage end: the stage-end guards. Files: plan.md ("Stage 3 results"). Covers AC-8.

## Stage 4: smoothing and following

- [x] **T4.1** The walk: `smooth_walk.hpp` — centres at odd doubled coordinates, the `(2a + 1)·|dy|` vs `(2b + 1)·|dx|`
  compare, corners (x-neighbour, y-neighbour, diagonal), cells crossed through their interior vs touched. Test:
  `tests/pathfinding/test_smooth_walk.cpp` (equal to `collision::cells_crossed` on a 16-px grid, cell for cell and
  corner for corner, for 20 000 seeded centre pairs on maps up to 64 × 64, plus horizontal, vertical and 45° cases). A
  PC micro-measurement of the walk against spec 019's `GridWalk` over the same pairs, recorded (decision 18). Covers
  FR-15.
- [x] **T4.2** `smooth_path`: the greedy run, the weight rule (no heavier than the replaced part), the corner rule per
  mode, first and last kept, the anchor unchecked, in place, the buffer contract. Test: `tests/pathfinding/test_smooth.cpp`
  (open room → 2 waypoints; L-shaped corridor → 3; a corner it must not cut in each mode; a heavier cell it must not
  cross; a blocked start; every segment of 500 seeded smoothings passes the sampled checker). Files: `smooth.cpp`.
  Covers FR-15.
- [x] **T4.3** [P] The follower: `follower.cpp`. Test: `tests/pathfinding/test_follower.cpp` (radius advance, several
  waypoints in one call, overshoot of the last point then arrival, empty list, NaN position and radius (assert;
  Release unchanged), the same bits for the same inputs with `-ffp-contract=off`). Covers FR-19.
- [x] **T4.4** Stage end: the stage-end guards. Files: plan.md ("Stage 4 results", the walk measurement). Covers AC-8.

## Stage 5: distance fields

- [x] **T5.1** Fields in one call: `field_internal.hpp`, `field_create.cpp` (double/single buffers, heap, move),
  `field_build.cpp` (begin fill, goals, Dijkstra with `d + S·w(c)` / `d + D·w(c)`, max cost, compaction). Test:
  `tests/pathfinding/test_field_build.cpp` (one and several goals, duplicates, max cost, weights, every mode, blocked
  goals ignored, blocked cells unreachable, outside goals (assert; Release ignored), against the multi-source
  reference on 300 seeded maps). Covers FR-16, FR-23.
- [x] **T5.2** Slices and buffers: `begin`/`step`/`cancel`/`building`/`ready`/`stale`/`settled`, the swap, the users
  count. Test: `tests/pathfinding/test_field_slices.cpp` (slices of 1, 37, 512 equal one call; the previous build answers
  during a double-buffered rebuild; single-buffered → not ready; stale on an edit; the map destroyed mid-build). Covers
  FR-17, FR-20.
- [x] **T5.3** [P] Queries: `field_query.cpp` — `distance`, `next` (argmin, neighbour-order ties, the rule, none at a goal
  or unreachable cell), `next_point`. Test: `tests/pathfinding/test_field_next.cpp` (brute-force `next`; field = the
  search's cost from every walkable cell on 200 seeded maps). Covers FR-18.
- [x] **T5.4** Stage end: the stage-end guards. Files: plan.md ("Stage 5 results"). Covers AC-8.

## Stage 6: random references, the golden, fuzz

- [x] **T6.1** AC-2: `tests/pathfinding/test_reference_random.cpp` — 2048 seeded maps (1 × 1 to 64 × 64, 0–50 %
  blocked, weights 1–9, every mode, agent sizes 1–2): 4 searches each (partial on/off, a limit sometimes, one sliced)
  against the reference Dijkstra and the literal A*; one field (1–4 goals) against the multi-source reference and
  `next`; every smoothed segment against the sampled checker. Time it in pc-debug: ≤ 5 s, else 512 maps in Debug and
  2048 elsewhere (recorded). Covers AC-2.
- [x] **T6.2** The golden script: `examples/pathfinding/check_maps.{hpp,cpp}` (`rooms`, `maze`, `swamp`, seeded),
  `examples/pathfinding/self_check.{hpp,cpp}` (M1–M4; 512 searches, door edits, stale searches, 64 smoothings, 16 fields
  with `next`; FNV-1a of every result; checkpoints every 64 searches), goldens from pc-debug in
  `kPathfindingCheckGolden`/`kPathfindingCheckpoints`, equal in pc-release. Test:
  `tests/pathfinding_example/test_self_check.cpp` (on the fake platform; a changed tie rule or neighbour order fails it —
  checked once by hand with a temporary swap). Files: those, `tests/CMakeLists.txt` (025's block). Covers AC-3.
- [x] **T6.3** [P] Fuzz: `tests/fuzz/fuzz_path_ops.cpp` (bytes → map size, cells, settings, then a stream of syncs,
  edits, searches (one-shot and sliced, with edits between slices), smoothings, field builds and queries, follower
  steps, including out-of-range values on the Release-path build; checks: paths valid, sliced = one-shot, nothing out of
  bounds), `libfuzzer_path_ops.cpp`, `test_fuzz_path_ops.cpp` (mini-fuzz in `nnh_tests`). Files: `tests/CMakeLists.txt`
  (025's block: `target_sources(nnh_fuzz_targets …)`, the `if(NNH_BUILD_FUZZERS)` executable). Covers X, FR-23.
- [x] **T6.4** Stage end: the stage-end guards. Files: plan.md ("Stage 6 results": golden and checkpoints, AC-2's
  timing). Covers AC-3, AC-8.

## Stage 7: `examples/pathfinding` (PC), the bench and the ARM pre-check

- [ ] **T7.1** Art, level and scaffolding: `art/make_art.py` (tiles, hero 1 × 1 and 2 × 2, chasers, marker),
  `art/make_level.py` (`assets/levels/keep.tmj`: `ground` with `path_cost`, `collision` with walls and three doors,
  the island, the one-cell corridor, spawn objects), `level.{hpp,cpp}` (cost table from `path_cost`, two maps from
  `collision_grid()`, doors), `minimap.{hpp,cpp}`, `hud.{hpp,cpp}`, `showcase_scene.{hpp,cpp}`, `main.cpp`, `demos.hpp`,
  `CMakeLists.txt`, `examples/CMakeLists.txt` (between `input` and `platformer`). Test: PC screenshots of the level and
  the minimap read back. Covers AC-4.
- [ ] **T7.2** Hero, crowd, debug view: `hero.{hpp,cpp}` (search, smoothing toggle, `cells_to_points`, follower,
  `collision::move`, re-plan on a version change), `crowd.{hpp,cpp}` (32 chasers, a double-buffered field rebuilt in
  slices of 512 when the hero changes cell, `next_point`), `debug_view.{hpp,cpp}` (expanded cells, frontier, raw path vs
  waypoints, heat map), touch → goal, A/X/L/R/D-Pad Up, the log lines of plan.md; `PATHFINDING_SHOT` modes. Test: PC
  screenshots `explored`, `smoothed`, `field`, `partial`, `door-closed`, `door-open`, `big` read back (shapes, both
  screens). Covers AC-4, FR-20.
- [ ] **T7.3** Check scene: SELECT (or `PATHFINDING_DEMO=check`) runs T6.2's script at 8 searches per frame and returns
  to the showcase (`pathfinding: showcase`); ctest `pathfinding_self_check_pc` (after the `data_self_check_pc` block).
  Files: `check_scene.{hpp,cpp}`, `tests/CMakeLists.txt`. Covers AC-3, AC-5.
- [ ] **T7.4** [P] Bench: `bench.{hpp,cpp}` (workloads `astar`, `maze` one and slices, `replans`, `field` one and
  slices, `next`, `next_crowd`, `smooth`, `sync`, `door` on the seeded maps and a code-built 64 × 64 `Tilemap`),
  `bench_scene.{hpp,cpp}` (300 frames each, average and worst, the lines and verdict; Debug `not judged (Debug)`); Y and
  `PATHFINDING_DEMO=bench`. Test: a pc-dev run logs every line. Covers AC-7.
- [ ] **T7.5** The script `examples/pathfinding/tests/check.nnhs`: SELECT → every checkpoint and the PASS line;
  `pathfinding: showcase`; fixed touches with their `goal:` lines; A → `door:` and `replan:` (a different cost); the
  island touch → `partial`; X → `hero: 2x2`, the corridor touch → its refusal; L → `mode:`; screenshots at fixed waits;
  a `wait 3` after every `expect` an input follows; Y → the bench lines and the verdict; `fail-on "pathfinding: FAIL"`,
  `fail-on "pathfinding bench: FAIL"`, `fail-on "] E "`; ctest `pathfinding_check_script_pc` (`RESOURCE_LOCK
  nnh_remote_port`). Test: `nnh run pathfinding --pc --no-build --script examples/pathfinding/tests/check.nnhs` → PASS,
  twice. Files: plan.md ("Stage 7 results": every expected line). Covers AC-5.
- [ ] **T7.6** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects
  `src/pathfinding/*` and the example's `bench.o`; per AC-7 hot path record instructions, calls, stack reloads,
  `__aeabi_*`, `vmrs`, `memcpy`/`memset`, constant-multiply chains: the expansion per mode (pop through the buffer and
  through the heap, one neighbour skipped/relaxed/pushed), the heap's sift step, the field's settle and relaxation,
  `next`, the walk step, sync per cell (with the tilemap's callback); the miss model per workload from the working sets
  (map, records, heap top, field, tilemap layers); compare with plan.md's table and fix what differs without changing a
  result (every golden holds). Files: plan.md ("Stage 7 ARM pre-check": the table and the expected console numbers).
  Covers AC-7, II.

## Stage 8: mutation testing, opt-in checks

- [ ] **T8.1** Mutation pass as in plans 019–023: hand mutants (`build/ref-025/mutants.py`, applied one at a time by
  `mutate.py`, the whole `nnh_tests` each) of `src/pathfinding/*` (the order's three keys, the neighbour order, the
  diagonal rules, the strict-g rule, the buffer, compaction, staleness, the limit, partial ties, footprints, the walk's
  corners, the weight rule, field relaxations and `next` ties, the follower, misuse paths); Release-path mutants in
  pc-release; survivors become tests or are argued equivalent. Files: plan.md ("Mutation testing"). Covers VI.
- [ ] **T8.2** [P] Release strings: `scripts/check_release_strings.sh` on the 3ds-release and pc-release `pathfinding`
  executables passes. Covers FR-23.
- [ ] **T8.3** [P] Opt-in by linking: probes (`build/ref-025/probe_*`, not committed) linked against the 3ds-release
  libraries: a game calling only `PathMap` and `find` links no field, smoothing or follower code; a game with
  `collision::move` only links nothing of `pathfinding`; `hello`'s sizes and `nm` equal T0.1's. Files: plan.md ("Stage 8
  results"). Covers FR-24, AC-8.
- [ ] **T8.4** Regression and build times: `compare.sh` (every T0.2 reference identical); every example's console
  script passes with `--pc`; `.text` of `queries`, `entities`, `prefabs`, `platformer` unchanged and of `pathfinding`
  recorded; edit `examples/pathfinding/hero.cpp` → simulator and `.3dsx` times against T0.1's and constitution III.
  Files: plan.md ("Stage 8 results"). Covers AC-6, AC-8, III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; AC-3's golden
  and checkpoints, AC-2's random references, the walk = `cells_crossed`, the replay ctest with the tree's lines
  identical on all three; ASan/UBSan clean over compaction, the doomed block, sliced searches and builds, the
  stamp wrap and in-place smoothing; libFuzzer `fuzz_path_ops` 10 minutes, clean. Files: tasks.md (results), plan.md
  ("Stage 9 results"). Covers I, X, AC-1, AC-3.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh
  cmake --build --preset 3ds-dev --target pathfinding_3dsx platformer_3dsx` without a warning; T7.6's table still true;
  `hello.elf` equal to T0.1's; plan.md's "Stage 10: console run (prepared)" completed with the recorded golden,
  checkpoints and showcase lines. Covers AC-8, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Two runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run pathfinding --script examples/pathfinding/tests/check.nnhs` → every
     `pathfinding: <n> sum` line and `pathfinding: PASS` equal the PC's, every showcase line, the bench lines and the
     verdict (numbers recorded); screenshots read back (the same pictures as the `--pc` run's);
  2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the tree's replay
     lines, `60 Hz PASS`.
  Files: plan.md ("Console run 1": verdicts, bench numbers, log paths). Covers AC-5, AC-6, AC-7.
- [ ] **T10.3** (only if a bench target is missed) Console bench refinement: find the cost in the 3ds-dev disassembly,
  apply plan risk 1's list without changing a result (every golden holds); then T10.2 run 1 again; if still missed, the
  budget is set from the measurement with ≈ 25 % margin, written in the spec's AC-7, `bench_scene.cpp` and
  `docs/api/pathfinding.md` with the numbers. Files: plan.md ("Optimisation round", "Console run 2 and budgets set").
  Covers AC-7.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** (the Old 3DS costs added after T10.2) `docs/api/pathfinding.md`: maps and syncing from grids and
  tilemaps (regions), weights and the `path_cost` pattern, agent sizes, the diagonal modes with a picture of each rule
  (`docs/images/pathfinding/*.png`), costs and the 32-bit bound, searches and statuses, the total order, limits and
  partial paths, time slicing and stale results, smoothing (centre line, greedy runs), fields and crowds (the naive flee),
  the follower, re-planning patterns, determinism, costs, memory per cell, misuse; header comments (allocates? per-frame
  safe? misuse?). Covers VIII, AC-9.
- [ ] **T11.2** Other pages: `docs/api/collision.md` (a link at the end of "Grids"), `docs/api/tilemap.md` (a link under
  "Collision layers"), `docs/guides/levels.md` (a "Slow tiles for pathfinding" subsection at the end of "4. Collision"),
  `docs/architecture/patterns.md` (the row: implemented), `docs/REFERENCES.md` (Godot `AStarGrid2D`/`AStar2D`, the A*
  Pathfinding Project, Unity NavMesh, Red Blob Games, Amit Patel, Hart–Nilsson–Raphael, Brian Walker, Emerson, Harabor &
  Grastien, Botea et al.), `CLAUDE.md` (layout paragraph after "Collision layers and queries", commands after the
  `ANIMATION_DEMO=check` line, the `pathfinding/` example in the layout); `python -I scripts/docs_tool.py check` → 0
  broken links. Covers AC-9.
- [ ] **T11.3** `docs/architecture/decisions.md`: D40 change-log note "implemented" with the stage refinements. Covers
  AC-9.
- [ ] **T11.4** Final measurements (header costs, build times, `hello` sizes, `.text`) in plan.md and
  `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T11.5** `docs/ROADMAP.md`: the "Pathfinding on grids" checkbox, with the implemented summary.
- [ ] **T11.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
- [ ] **T11.7** (the lead, after 022 and 023 are merged) Merge `spec-025` into `main` per plan.md's "Merge notes"; then
  the full ctest (pc-debug, pc-release), every example's `--pc` script, AC-3's golden, 022's and 023's goldens and the
  replay ctest with the tree's lines, one 3ds-release build; T10.2 run 2 is repeated if the merge happened after it.
