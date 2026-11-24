# Tasks 021: Prefabs

Status: implemented 2026-11-23 (stages 0–8 and 11); stage 9 (Linux) and stage 10 (console) left to the lead and the
maintainer · approved 2026-11-23, with the plan's six decisions as recommended
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The world's all-or-nothing reservation comes first and is proven against spec 020's own calls and goldens
before any prefab code uses it; the library is pure and tested on the fake platform; AC-2's twin-world golden proves
the equivalence; the platformer swaps to prefabs (stage 6) with its golden lines **unchanged**. The ARM11 disassembly
is checked against the plan's cost table before the first console run (T7.6). Stage 9 is run on the Linux build VM by
the lead; the only console stage is stage 10: four scripted runs the maintainer only starts (Homebrew Launcher → Y).
Deviations found while building go into plan.md as "Stage N refinements", as in plans 012–020.

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each): `hello.exe` (pc-release, pc-debug) and `hello.elf`/`.3dsx`
  (3ds-release, 3ds-dev) sizes (`size` text/data/bss) and `nm -C` lists (`build/ref-021/`); edit
  `examples/entities/arena.cpp` → simulator and `.3dsx` times (the reference for the prefabs example's edit); clean
  `nnh_core`; preprocessed lines and `-fsyntax-only` time of a one-line TU including `<nnh/entities.hpp>` (the
  reference for `<nnh/prefabs.hpp>`); `.text` of the platformer's and `entities`' 3ds-release ELFs; 3ds-dev
  `world_add`/`spawn` instruction counts (`arm-none-eabi-objdump -drC`, the stage 1 guard). Files: plan.md ("Stage 0
  results"). Covers III, FR-16, AC-8.
- [x] **T0.2** [P] Reference screenshots for AC-8 in `build/ref-021/ref/` (not committed): `hello`, `sprites`,
  `scenes`, `camera` (`CAMERA_DEMO=zoom1`), `queries` (`QUERIES_DEMO=all`), `entities` (showcase at 30), platformer
  `PLATFORMER_DEMO=bench` (top screen), level 1 at frame 90, `PLATFORMER_DEMO=replay` at 312 and at 64 (sparkles);
  `build/ref-021/compare.sh` retakes them into `now/` and compares. Files: plan.md (list and frames). Covers AC-8.
- [x] **T0.3** [P] Goldens today: ctest `platformer_replay_matches_script` passes with last line
  `replay: done 900 frames sum B2418A28: 60 Hz PASS`; ctest `entities_self_check_pc` → `entities: PASS C8FB9A41`;
  `nnh_tests` count (raise `kMaxTests` only if fewer than 400 are left). Files: plan.md. Covers AC-4, FR-16, VI.

## Stage 1: the world's all-or-nothing reservation (entities-internal, pure)

- [x] **T1.1** Shared bookkeeping: inline `take_entity(w, slot)` and `attach_component(w, kind, entity, record, slot)`
  in `world_internal.hpp`; `World::spawn()` and `world_add()` rewritten over them. No new test: spec 020's whole suite,
  the self-check `C8FB9A41` and the replay `B2418A28` are the test (run before and after). Files:
  `engine/core/src/entities/world_internal.hpp`, `world_spawn.cpp`, `world_components.cpp`. Covers FR-16.
- [x] **T1.2** `world_kind_of(world, type id)` (straight compares for the first four kinds, then a scan; empty world
  −1) and `world_component(world, kind, slot)`. Files: `engine/core/src/entities/entities_internal.hpp`,
  `world_reserve.cpp`, CMake `_nnh_entities_sources`. Test: `tests/entities/test_world_reserve.cpp` (part 1: every
  registered kind, kind 5 of 32, unregistered → −1, equal to `kind_index<T>`). Covers FR-5, FR-14.
- [x] **T1.3** `world_reserve`: entity + one slot per kind + tags, all or nothing; refusals `Empty`, `Clearing` (no
  counter), `WorldFull` (spawn misses + 1), `KindFull` with the first full kind (its misses + 1); nothing else changes
  on a refusal (records, bitmaps, extents, counts compared before/after); during an iteration the slots are fresh and
  not visited; inside a remove hook allowed. Test: `test_world_reserve.cpp` (part 2): twin worlds on 20 seeded
  sequences of 2000 operations — reserve + plain copies vs `spawn` + `add<T>(e, value)` + `add_tag` — equal handles,
  slots, bytes, tags, stats; plus each refusal case. Covers FR-5, FR-8.
- [x] **T1.4** Stage end: ctest pc-debug and pc-release; 3ds-release without a warning; 3ds-dev `world_add`/`spawn`
  instruction counts equal T0.1's (± 2); `hello` sizes and `nm` equal T0.1. Files: plan.md ("Stage 1 results").
  Covers FR-16, II.

## Stage 2: the library — creation, names, registration, queries (pure)

- [x] **T2.1** Block layout: pure function from a description and the flavour (names or not) to offsets and total
  (header, hashes, table = power of two ≥ 2 × prefabs, records, names, entries, values aligned to 16). Files:
  `engine/core/src/prefabs/library_layout.{hpp,cpp}`, `prefab_internal.hpp`. Test:
  `tests/prefabs/test_library_layout.cpp` (1 and 1024 prefabs, 0 entries, the 128 × 4 × 16 example ≈ 26 KB as in
  plan.md, PC and 3DS-sized records via `sizeof` checks). Covers FR-1, II.
- [x] **T2.2** `PrefabLibraryDesc`, `PrefabLibrary::create` (capacities 0/1025/−1, entries −1/65 536, value bytes −1 /
  over 1 MiB → `InvalidArgument`; allocation failure → `OutOfMemory`), the stamp, destructor, move, moved-from (queries
  answer nothing, commands assert), tag `prefabs`. Files: `engine/core/include/nnh/prefabs.hpp`,
  `detail/prefab_values.hpp`, `src/prefabs/library_create.cpp`, CMake `_nnh_prefabs_sources`. Test:
  `tests/prefabs/test_library_create.cpp` (block size = layout via tag stats). Covers FR-1, FR-15.
- [x] **T2.3** Names: `prefab_name` (constexpr, sized), the one-pass run-time hash (equal to `detail::fnv1a32` on the
  FNV test vectors and on 1000 generated names), the open-addressing table (insert, probe, wrap-around at the end of
  the table, full at `max_prefabs`), `find` (three forms, unknown → null). Files: `src/prefabs/library_find.cpp`. Test:
  `tests/prefabs/test_names.cpp` (with two hand-found colliding probes: different names landing on the same table slot).
  Covers FR-3, FR-14.
- [x] **T2.4** Registration without variants: `add(name, options, values...)` and the array form; `component_value<T>`,
  `place<C, M>`; checks before writes (name, duplicate hash, a kind twice, 33 entries, 9 tags, `kNoTag` inside, the
  placement kind not among the entries, capacities with alignment); values copy-constructed in call order; names kept
  in Debug/Development (31 bytes). Files: `src/prefabs/library_add.cpp`, `prefabs.hpp`, `detail/prefab_values.hpp`.
  Test: `tests/prefabs/test_add.cpp` (block hash unchanged after each refusal; `Counted` copies at registration only
  and destructions at the library's end; a hook-only prefab; a commented list of the compile-time refusals: not
  copyable, `alignas(32)`, a non-Vec2/Rect member). Covers FR-2, FR-3, FR-15.
- [x] **T2.5** [P] Queries and stats: `count`, `capacity`, `valid` (null, out of range, another library's stamp),
  `name` (Debug/Development; Release path `""`), `kinds`, `tags`, `has<T>`, `value<T>`, `stats()`. Files:
  `src/prefabs/library_queries.cpp`. Test: `tests/prefabs/test_queries.cpp`. Covers FR-11, FR-3.
- [x] **T2.6** Stage end: ctest; include check (a TU including only `<nnh/prefabs.hpp>` preprocesses without
  `tilemap`, `data`, `collision`, `tween`, `animation`, `gfx` or a standard container header; `build/ref-021/inccheck.sh`);
  `hello` sizes equal T0.1. Files: plan.md ("Stage 2 results"). Covers FR-16, III.

## Stage 3: variants (pure)

- [x] **T3.1** Flattening: entries replaced in place by kind, others appended in call order, inherited entries share
  the base's values (owner bit; destroyed once), tags united (duplicates dropped, 9 → `LimitReached`), placement
  inherited or replaced, base null or of this library (else `InvalidArgument`). Files: `src/prefabs/prefab_flatten.cpp`,
  `library_add.cpp`. Test: `tests/prefabs/test_variants.cpp` (part 1; `Counted`: no copy for shared values, one
  destruction each). Covers FR-4.
- [x] **T3.2** Hook chains: own hook + link to the nearest hooked base, chain length ≤ 8 (9th → `LimitReached`), variant
  of a variant of a variant; queries on variants (`kinds`, `tags`, `value<T>` reads the shared value). Files:
  `prefab_flatten.cpp`, `library_queries.cpp`. Test: `test_variants.cpp` (part 2; the chain's order is checked in
  stage 4 when hooks run). Covers FR-4, FR-11.

## Stage 4: spawning (pure)

- [x] **T4.1** `spawn(world, prefab)`: handle checks, kinds per entry (`world_kind_of`), `world_reserve`, copies in
  entry order (destroy the slot's `T{}` when the type has a destructor, then copy), stats; returns the entity. Files:
  `src/prefabs/prefab_spawn.cpp`, `prefabs.hpp`. Test: `tests/prefabs/test_spawn.cpp` (twin worlds by hand: handles,
  slots, values, tags; lowest free slots; quarantined slots skipped; `events::stats()` unchanged; only the prefab's
  kinds and tags). Covers FR-5, FR-12, FR-13.
- [x] **T4.2** Overrides: `spawn_with` (replace an entry's value: constructed once, `Counted`; add a kind not in the
  prefab, appended after the entries), the array form's duplicate kind asserts and the second is ignored; compile-time
  refusals (duplicate type, bare `Vec2`) documented in the test file. Files: `prefab_spawn.cpp`, `prefabs.hpp`. Test:
  `tests/prefabs/test_overrides.cpp` (twin worlds). Covers FR-7.
- [x] **T4.3** Placement: `spawn(world, p, at)` and `spawn_with(world, p, at, ...)` into a `Vec2` member and a `Rect`
  member (x, y only), the override of the placement kind (position wins), a position without placement asserts and
  the spawn continues without it. Files: `prefab_spawn.cpp`. Test: `tests/prefabs/test_placement.cpp` (part 1; the
  map sources in T5.2). Covers FR-7.
- [x] **T4.4** Hooks: `SpawnInfo` (position or none), the chain base first (call log), hooks that spawn (this library
  and another), destroy another entity, destroy the new entity (stale handle returned, the twin world agrees), add and
  remove components; spawns inside `each` (not visited) and from a world's remove hook. Files: `prefab_spawn.cpp`.
  Test: `tests/prefabs/test_spawn_hooks.cpp`. Covers FR-6, FR-4.
- [x] **T4.5** All or nothing: full world, full first/middle/override-only kind, unregistered kind (always asserts),
  inside `clear()` (null, no miss, no assert); `WhenFull::Assert` (assert hook + error log naming the prefab) vs
  `WhenFull::Miss` (quiet); misses in the library's stats and in the world's (spawn misses / that kind's misses);
  nothing created on any refusal (counts, extents, records). Files: `prefab_spawn.cpp`. Test:
  `tests/prefabs/test_all_or_nothing.cpp`. Covers FR-8, FR-11.
- [x] **T4.6** [P] Misuse and cost: null/foreign handle, empty/moved-from library, a world that doesn't store a kind,
  more prefab tags than the world holds (assert; Release keeps the first), the library destroyed from its own hook
  (Debug: assert before anything changes; Release: freed when the outermost spawn returns); the allocation counter
  unchanged over 100 000 spawns/destroys after creation; no log line in the normal path. Files: tests, fixes where
  needed. Test: `tests/prefabs/test_prefab_misuse.cpp`. Covers FR-14, FR-15.

## Stage 5: map objects, data assets, the golden script

- [x] **T5.1** Fixture map: `make_fixtures.py` writes `tests/data/maps/good_prefabs.tmj` (layers `things` and `more`;
  objects: `coin` rectangle with int `value` 5, `coin` point, `walker` point, `walker` with `prefab` = `flying_walker`,
  `door`, `spawn` point, an empty type, `prefab` = `coinn` (misspelt), `prefab` as an int, one `coin` in `more`); CMake
  converts it to `good_prefabs.nnhmap` for `nnh_tests` and adds `map_convert_accepts_good_prefabs`. Files:
  `tests/data/maps/make_fixtures.py`, `good_prefabs.tmj`, `tests/CMakeLists.txt`. Test: the converter test passes.
  Covers AC-3.
- [x] **T5.2** `spawn_map_objects` / `check_map_objects`: map order, `prefab` property first, else the type; skipped
  (door, spawn, empty type), unknown explicit names (log line `object <id> "<name>" (layer <layer>): no prefab
  "<name>"` caught by the log hook, assert in Debug, Release path logs only), the non-string `prefab`; the layer filter;
  placement from point and rectangle objects (`Rect` gets w, h unless `keep_size`; `Vec2` gets the top-left);
  `SpawnInfo.object`/`object_index` in hooks (a hook reads `value`); the summary and the library's map counters; check
  = the spawn's summary without spawning, logging or asserting; restart after `clear()` gives the first load's slots;
  the type cache (equal pointers, and equal text at different pointers). Files: `src/prefabs/prefab_map.cpp`. Test:
  `tests/prefabs/test_map_objects.cpp`, `test_placement.cpp` (part 2). Covers FR-9, FR-6, FR-7, AC-3.
- [x] **T5.3** [P] Data-asset values: a prefab per `Enemy` asset of spec 014's test game, named `data::name<Enemy>(i)`,
  registered in a loop; spawned components equal `data::load`'s struct (and a converted struct the test builds from
  it); spawning reads no file (the fs call counter). An object-level check: no `src/prefabs/*` object references a
  `data` or `tilemap` symbol except `prefab_map.o` (tilemap). Files: tests only. Test:
  `tests/prefabs/test_data_values.cpp`; the check in `build/ref-021/linkcheck.sh`. Covers FR-10, FR-16, AC-3.
- [x] **T5.4** Self-check (AC-2): `examples/prefabs/self_check.{hpp,cpp}` (pure: plan's kinds A–E, two worlds of 1024
  × 4 tags, a `WhenFull::Miss` library of 16 prefabs with variants, hooks and placements, the hand side's flattened
  table written independently, 50 000 operations, per-step handle comparison, full-state hashes every 1000 steps and
  after clears, checkpoints every 10 000); golden values from pc-debug in `kPrefabsCheckGolden`, equal in pc-release.
  Files: `examples/prefabs/self_check.*`, `tests/prefabs_example/test_self_check.cpp`, `tests/CMakeLists.txt`. Test:
  the unit test against the goldens. Covers AC-2, FR-5, FR-13.
- [x] **T5.5** [P] Fuzz: `tests/fuzz/fuzz_prefab_ops.cpp` drives the self-check's operation set from bytes (prefab side
  vs hand side must agree after every operation); mini-fuzz in `nnh_tests`, libFuzzer with `NNH_BUILD_FUZZERS`. Files:
  `tests/fuzz/*`, `tests/CMakeLists.txt`. Covers X, FR-13.
- [x] **T5.6** Stage end: ctest pc-debug and pc-release; 3ds-release without a warning; `hello` sizes and `nm` equal
  T0.1; `compare.sh` (every reference identical). Files: plan.md ("Stage 3–5 results"). Covers FR-16, AC-8.

## Stage 6: the platformer adopts prefabs (same play)

- [x] **T6.1** `level_prefabs.{hpp,cpp}`: the library (`coin`, `turret`, `spark` as plan.md describes; the coin's hook
  reads `value`, the spark's hook starts the fade through the tween set it captures). Test:
  `tests/platformer/test_level_prefabs.cpp` (the three prefabs' kinds, tags, values and placements; a coin from a map
  object equals the old `spawn_coin` result on a twin world; a spark equals the old `burst`'s entity, tween included).
  Files: `examples/platformer/level_prefabs.*`, `CMakeLists.txt`, `tests/CMakeLists.txt`. Covers AC-4.
- [x] **T6.2** Swap: `LevelPlay` owns the library; `spawn_things()` = `clear()`, the player by hand,
  `spawn_map_objects`; `coin_count_` from the coin storage; `Sparkles::burst` by `spawn_with(..., at, Velocity{v})`;
  `spawn_coin`, `TurretBook::spawn_turret` and the `strcmp` dispatch removed (doors and the spawn point stay in
  `read_objects`). `test_coins.cpp`, `test_turrets.cpp`, `test_sparkles.cpp` keep every case and expected result,
  spawning through the library. Files: `examples/platformer/level_play.*`, `coins.*`, `turrets.*`, `sparkles.*`,
  `tests/platformer/*`. Test: ctest `platformer_replay_matches_script` passes with **unchanged** lines; `nnh run
  platformer --pc --no-build --script examples/platformer/tests/level.nnhs` → PASS; `compare.sh` (level 1, the replay
  at 64 and 312, the bench identical). If a byte differs, stop and explain in plan.md before touching a golden line.
  Files: plan.md ("Stage 6 results"). Covers AC-4, FR-5, FR-9.

## Stage 7: `examples/prefabs` (PC)

- [x] **T7.1** Art and room: `art/make_art.py` (original tiles: walls, floor, door), `art/make_room.py` →
  `assets/rooms/room.tmj` (6 `walker`, 2 `walker` objects with `prefab` = `flying_walker`, 5 `coin`, 1 `coin` with
  `value` 5, 1 `door`), `data_types.cpp` (`EnemyData`: hp, speed, size, colour), `assets/enemies/*.enemy` (4 enemies).
  Files: `examples/prefabs/art/*`, `assets/*`, `data_types.cpp`, `CMakeLists.txt` (`ASSETS`, `DATA_SCHEMA`),
  `examples/CMakeLists.txt`. Test: builds; the converters accept the files. Covers AC-5.
- [x] **T7.2** Room and library: `components.hpp`, `tags.hpp` (`static_assert(tags_distinct(...))`), `room.{hpp,cpp}`:
  world (bound to the showcase scene, before its tween set), library (`WhenFull::Miss`: `walker`, `flying_walker`
  variant with a `Bob` kind, a `flying` tag and a hook starting a bob tween, `coin` with a hook reading `value` and
  colouring gold when > 1), `spawn_map_objects` logging `map: spawned N skipped N unknown N misses N`; walkers move and
  turn at walls; `roster.{hpp,cpp}` registers one prefab per `EnemyData` asset in a loop; `room_draw`, `hud` (library
  stats, the last map summary, the roster). Test: PC screenshot `PREFABS_DEMO=showcase` (the room with its variant and
  gold coin) read back. Covers AC-5, FR-4, FR-6, FR-9, FR-10, FR-11.
- [x] **T7.3** Touch spawns and selection: the bottom screen shows the room through a `Camera2D`; a touch spawns the
  selected prefab at the world point (placement) logging `spawned: <name> slot n gen g at x,y tags a,b`; L/R cycle the
  selection logging `selected: <name> kinds <list> tags <list>` (the debug line, kinds named by the example's
  `kind_id` table); A spawns the gold coin by override (`Coin{5}`) at the room's centre; a full walker kind logs
  `missed: walker (misses N)`; X restarts (`clear()` + map spawn: the same map line). Files: `showcase_scene.*`,
  `room.cpp`, `hud.cpp`, `main.cpp`, `demos.hpp`. Test: PC screenshots (a variant spawned, the override, the full-kind
  miss) read back; log lines in a `--pc` run. Covers AC-5, FR-7, FR-8, FR-11.
- [x] **T7.4** Self-check scene: SELECT runs T5.4's script, 2000 operations per frame, logging `prefabs: <n> sum
  <checksum>` and `prefabs: PASS <checksum>` / `prefabs: FAIL ...`; `PREFABS_DEMO=check` starts in it; ctest
  `prefabs_self_check_pc`. Files: `self_check_scene.*`, `main.cpp`, `tests/CMakeLists.txt`. Test: ctest. Covers AC-2,
  AC-6.
- [x] **T7.5** [P] Bench (AC-7): `bench.{hpp,cpp}` (the four workloads, pure; the hand-written spawn reference on
  alternate frames; the code-built 512-object map; the 128-prefab library and its shuffled names) and
  `bench_scene.{hpp,cpp}` (300 frames each, profile scopes, one line per workload with the spawn line's hand time and
  ratio, then the verdict; Debug: `not judged (Debug)`); Y runs it, `PREFABS_DEMO=bench` starts in it. Files:
  `examples/prefabs/bench*`. Test: a pc-dev run logs every line and PASS. Covers AC-7.
- [x] **T7.6** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects of
  `bench.cpp`, `src/prefabs/*` and `src/entities/world_reserve.cpp`, `world_spawn.cpp`, `world_components.cpp`; for each
  AC-7 hot path record instructions, calls, stack reloads, `vmrs`, `__aeabi_*`, `memcpy`/`memset`, and compare with
  plan.md's ARM11 table (prefab spawn ≈ 260 per entity, `find` one-pass hash and a straight-line first probe, spec
  020's `world_add` ≈ 92 and `spawn` ≈ 65 unchanged). Fix what differs (no result changes: every golden holds). Files:
  plan.md ("Stage 7 ARM pre-check": the table). Covers AC-7, II.
- [x] **T7.7** The script `examples/prefabs/tests/check.nnhs`: SELECT → every checkpoint and PASS line; the showcase's
  map line; fixed touches with expected `spawned:` lines; L/R with expected `selected:` lines; A (override); touches to
  `missed: walker`; X (restart, the same map line); screenshots (room, variant, override, miss); Y → the bench lines and
  `prefabs bench: PASS`; `fail-on "prefabs: FAIL"`, `fail-on "prefabs bench: FAIL"`, `fail-on "] E "`. Test:
  `nnh run prefabs --pc --no-build --script examples/prefabs/tests/check.nnhs` → PASS, twice. Covers AC-5, AC-6, AC-7.

## Stage 8: mutation testing, opt-in checks

- [x] **T8.1** Mutation pass as in plans 019–020: hand mutants of `src/prefabs/*`, `src/entities/world_reserve.cpp` and
  the shared helpers, `prefabs.hpp`'s templates, the platformer's `level_prefabs`, one at a time with the whole
  `nnh_tests` (pc-release for Release-path mutants); survivors become tests or are argued equivalent. Files: plan.md
  ("Mutation testing"). Covers VI.
- [x] **T8.2** [P] Release strings: `scripts/check_release_strings.sh` on the 3ds-release and pc-release `prefabs`
  executables passes (add the module's Development-only log texts to its list if it has any). Files:
  `scripts/check_release_strings.sh` (if needed). Covers FR-3.
- [x] **T8.3** [P] Code-only games link no tilemap code: a probe (`build/ref-021/probe_code_only.cpp`, not committed:
  registers and spawns prefabs from code only) linked against the 3ds-release libraries; `nm -C` shows no
  `nnh::Tilemap`, `nnh::Properties`, `load_tilemap` or `prefab_map` symbol; `linkcheck.sh` (T5.3) still holds. Files:
  plan.md ("Stage 8 results"). Covers FR-16, AC-8.
- [x] **T8.4** Opt-in and regression: `hello` sizes and `nm` equal T0.1 on PC and 3DS; `compare.sh` (hello, sprites,
  scenes, camera, queries, entities, the platformer's bench, level 1 and the replay identical); every example's script
  passes with `--pc` (`gate.nnhs`, `level.nnhs`, `entities/check.nnhs` included); `.text` of the platformer and
  `prefabs` recorded; build times of an `examples/prefabs/room.cpp` edit (simulator, `.3dsx`) against T0.1's
  `examples/entities` edit and constitution III. Files: plan.md ("Stage 8 results"). Covers FR-16, AC-8, III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; the AC-2 golden,
  the twin-world tests, the reservation tests, spec 020's goldens (`C8FB9A41`) and the platformer's replay
  (`B2418A28`) identical on all three (they are test assertions); ASan/UBSan clean over the `Counted` balance tests,
  shared variant values, the library destroyed from its hook (Release path) and hooks that destroy their copy;
  libFuzzer `fuzz_prefab_ops` 10 minutes, clean. Files: tasks.md (results). Covers I, X, FR-13, AC-1, AC-2.

## Stage 10: console check (the only steps on the 3DS)

- [x] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development
  builds of `prefabs`, `platformer` and `entities` without a warning; T7.6's table still true; `hello.elf` sizes as in
  T0.1. Files: plan.md ("Stage 10: console run (prepared)": commands and every expected line). Covers AC-8, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Four runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run prefabs --script examples/prefabs/tests/check.nnhs` → every `prefabs: <n> sum`
     line and `prefabs: PASS` equal the PC's, the map, `spawned:`, `selected:` and `missed:` lines as on the PC, the
     bench lines and the verdict (numbers recorded); screenshots read back;
  2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the **unchanged**
     replay lines, `60 Hz PASS`;
  3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS`;
  4. `build/pc-debug/tools/nnh/nnh run entities --script examples/entities/tests/check.nnhs` → `entities: PASS
     C8FB9A41` and `entities bench: PASS` (spec 020's budgets after the reservation refactor).
  Files: plan.md ("Stage 10 results": verdicts, bench numbers, log paths). Covers AC-4, AC-6, AC-7.
- [ ] **T10.3** (only if a bench target is missed) Console bench refinement: find the cost in the 3ds-dev disassembly,
  apply plan risks 1–3 without changing a result (every golden holds), then T10.2 run 1 again; if still missed, the
  maintainer sets the budget from the measurement with margin, written in the spec's AC-7 and `docs/api/prefabs.md`
  with the numbers. Files: plan.md ("Console bench refinement"). Covers AC-7.

## Stage 11: docs, roadmap, status

- [x] **T11.1** (the Old 3DS costs are added after T10.2) `docs/api/prefabs.md`: libraries and capacities (memory
  formula), registering, names and hashes (collisions refused, `find` by handle, no names in Release), variants
  (flattening, shared values, hook order), spawning and its equivalence with hand-written calls, overrides and
  placement, hooks and `SpawnInfo`, all-or-nothing and `WhenFull`, map objects and the level error, the data-asset
  pattern (one prefab per asset in a loop), determinism, costs measured in T10.2, misuse, pitfalls (padding, a hook
  destroying the world, `MapObject` valid during the hook); header comments (allocates? per-frame safe? misuse?).
  Covers VIII, AC-9.
- [x] **T11.2** Other pages: `docs/api/entities.md` (link from "Worlds": spawning by prefab), `docs/api/data.md` (the
  pattern), `docs/guides/levels.md` ("Placing prefabs in Tiled": types, the `prefab` property, what is skipped, the
  error message), `docs/api/tilemap.md` (objects: a link to prefabs), `docs/architecture/patterns.md` (Prototype row:
  implemented), `docs/REFERENCES.md` (Unity, Godot, flecs, Bevy, Tiled, *Game Programming Patterns*' Prototype),
  `CLAUDE.md` (layout paragraph: prefabs, `examples/prefabs`); the `nnh-architecture` skill's pattern list (Prototype);
  `python -I scripts/docs_tool.py check` → 0 broken links. Covers AC-9.
- [x] **T11.3** `docs/architecture/decisions.md`: D36 change-log note "implemented" with any stage refinements. Covers
  AC-9.
- [x] **T11.4** Final measurements (header cost of `prefabs.hpp` against T0.1's `entities.hpp`, build times, `hello`
  sizes, `.text` growth) in plan.md and `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [x] **T11.5** `docs/ROADMAP.md`: the "Prefabs" checkbox, with the implemented summary.
- [x] **T11.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
