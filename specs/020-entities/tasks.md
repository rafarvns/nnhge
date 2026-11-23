# Tasks 020: Entities, tags and object pools

Status: approved (2026-11-23, by the maintainer)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). Pools come first (the world builds on their slot machinery), the world is checked operation by operation
against a reference model, and the platformer swaps to entities and pools (stage 6) before it gains anything visible,
so its unchanged golden lines prove the swap. The ARM11 disassembly is checked against the plan's cost table before
the first console run (T7.6). Stage 9 is run on the Linux build VM by the lead; the only console stage is stage 10:
three scripted runs the maintainer only starts (Homebrew Launcher → Y). Deviations found while building go into
plan.md as "Stage N refinements", as in plans 012–019.

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each): `hello.exe` (pc-release, pc-debug) and `hello.elf`/`.3dsx`
  (3ds-release, 3ds-dev) sizes (`size` text/data/bss) and `nm` symbol lists; edit `examples/hello/main.cpp` →
  simulator and `.3dsx` build times; clean `nnh_core` build; preprocessed line count and `-fsyntax-only` time of a
  one-line TU including `<nnh/events.hpp>`, `<nnh/tween.hpp>` and `<nnh/collision.hpp>` (the references for the new
  headers); `.text` of the platformer's and `queries`' 3ds-release ELFs. Files: plan.md ("Stage 0 results"). Covers
  III, FR-25, AC-8.
- [x] **T0.2** [P] Reference screenshots for AC-8 in `build/ref-020/` (not committed): `hello`, `sprites`, `scenes`,
  `camera` (`CAMERA_DEMO=zoom1`), `queries` (`QUERIES_DEMO=all`), platformer `PLATFORMER_DEMO=bench` (top screen),
  level 1 at frame 90 and `PLATFORMER_DEMO=replay` at frame 312; `build/ref-020/compare.sh` retakes them into `now/`
  and compares the PNGs. Files: plan.md (list and frames). Covers AC-8.
- [x] **T0.3** [P] The platformer's replay lines today: run `PLATFORMER_DEMO=replay NNH_EXIT_AFTER_FRAMES=920` and
  confirm they equal `examples/platformer/tests/level.nnhs` (ctest `platformer_replay_matches_script` passes); record
  the final `sum` in plan.md. Covers AC-4 step 1.
- [x] **T0.4** [P] Test runner capacity: count `nnh_tests` tests; raise `kMaxTests` only if fewer than 400 are left.
  Files: `tests/framework/test_main.cpp` (if needed). Covers VI.

## Stage 1: handles, slot bits, pools (pure)

- [x] **T1.1** `Handle<T>`: null, packing, `from_bits`, `==`, `operator bool`, 4 bytes, trivially copyable;
  `detail::next_generation` (65 535 → 1, never 0). Files: `engine/core/include/nnh/detail/handle.hpp`,
  `detail/slot_bits.hpp` (generation part), `engine/core/CMakeLists.txt` (header list). Test:
  `tests/pool/test_handle.cpp`. Covers FR-1.
- [x] **T1.2** Slot bits: lowest free with a hint word (`~(live | quarantine)`), take (live, extent, fresh while
  iterating), release to quarantine, flush of dirty words below the extent, lowest-bit scan (`31 − clz(x & −x)`),
  end-of-iteration fresh clear. Files: `detail/slot_bits.hpp`. Test: `tests/pool/test_slot_bits.cpp` (word
  boundaries 31/32/33, capacity not a multiple of 32, full, hint after releases and flushes, against a naive bit-array
  reference on 10 000 seeded operations). Covers FR-4, FR-5, FR-6, FR-22.
- [x] **T1.3** Inline `Pool<T, N>`: storage (`alignas(T)`, N + 1 slots: the last is the checked accessor's scratch),
  meta words, `acquire()`/`acquire(value)` (copy and move), `release`, `get`, `alive`, `at`, `flush`, `clear`, stats;
  free slots hold a constructed `T{}`. Files: `engine/core/include/nnh/pool.hpp`, `detail/pool_core.hpp`,
  `src/pool/pool_core.cpp`, `src/pool/pool_internal.hpp`, CMake `_nnh_pool_sources`. Test: `tests/pool/test_pool.cpp`
  (lowest free, full → null + miss without assert, release → `T{}` + quarantine, acquire after release in the same
  frame takes another slot, flush, clear restarts at slot 0, stale/double release false, forged next-generation handle
  refused, `at` asserts / scratch `T{}` in the Release path, a `Counted` type balances, no allocation). Covers FR-2..FR-6,
  FR-8, FR-23.
- [x] **T1.4** Iteration and raw access: `items()` range with its guard, `each(fn)`, fresh bitmap (acquire during an
  iteration, also into a hole above the cursor, not visited), release of self/others during iteration, `break`,
  `flush()` inside an iteration asserts and is ignored; `data()`, `extent()`, `stride()`, `is_live`, `handle_at`.
  Files: `pool.hpp`, `detail/pool_core.hpp`. Test: `tests/pool/test_pool_iteration.cpp`. Covers FR-7, FR-6.
- [x] **T1.5** [P] Run-time `Pool<T>`: `create(capacity, tag)` (0, −1, 65 536 → `InvalidArgument`; allocation failure →
  `OutOfMemory`), one block (tag `pool` by default; tag stats equal the computed size), move and moved-from (asserts,
  answers nothing), same behaviour as the inline form (the T1.3/T1.4 cases run on both through a typed test helper).
  Files: `src/pool/pool_runtime.cpp`. Test: `tests/pool/test_pool_runtime.cpp`. Covers FR-3, FR-23.
- [x] **T1.6** Stage end: ctest; include check (a TU including only `<nnh/pool.hpp>` preprocesses without
  `collision`, `tween`, `animation`, `gfx`, `entities` or any standard container header); `hello` sizes equal T0.1.
  Files: plan.md ("Stage 1 results"). Covers FR-20, FR-25.

## Stage 2: worlds — creation, entities, components, storage views (pure)

- [x] **T2.1** World layout: pure function from a description to offsets and total (header, kind table, type ids,
  entity records `8 + 4·tags + 2·n` rounded to 4, bitmaps, queue, per-kind data with `cap + 1` slots aligned to
  `alignof(T)`, owner, four bitmaps). Files: `src/entities/block_layout.{hpp,cpp}`. Test:
  `tests/entities/test_block_layout.cpp` (an `alignas(32)` kind, 1 and 32 kinds, the 1024 × 8 × 4 example ≈ 64 KB plus
  components, as in plan.md). Covers FR-9, II.
- [x] **T2.2** `component<T>(capacity[, hook])`, `WorldDesc`, `World::create` (every `InvalidArgument` case: entities
  0/65 536, tags 9, no kind, 33 kinds, a kind twice, a kind capacity 0 or above `max_entities`; `OutOfMemory`), the
  block (tag `entities`), `T{}` in every slot, destructor, move, empty/moved-from; `detail::WorldHot` and the inline
  kind lookup. Files: `engine/core/include/nnh/entities.hpp`, `detail/world_layout.hpp`, `src/entities/world_create.cpp`,
  `world_lifetime.cpp`, `world_internal.hpp`, CMake `_nnh_entities_sources`. Test:
  `tests/entities/test_world_create.cpp` (also a non-movable kind and a `Counted` kind balancing). Covers FR-9, FR-10,
  FR-23.
- [x] **T2.3** `spawn`, `alive`: lowest free entity slot, zeroed record, full → null + miss + assert, stale after
  destroy (with T3.1) and the forged-handle case. Files: `src/entities/world_spawn.cpp`. Test:
  `tests/entities/test_spawn.cpp`. Covers FR-11, FR-2.
- [x] **T2.4** `add<T>` (with and without value), `get<T>` (inline: meta compare, record slot, typed address), `has<T>`,
  `remove<T>` without hooks for now, `at<T>`; add twice (assert, existing unchanged), kind full (assert, miss),
  unregistered kind (assert, null), stable addresses across other spawns/adds/removes, component quarantine. Files:
  `entities.hpp`, `src/entities/world_components.cpp`. Test: `tests/entities/test_components.cpp`. Covers FR-10, FR-12,
  FR-2.
- [x] **T2.5** [P] `Storage<T>`: `data`, `extent`, `stride`, `is_live`, `owner`, `slot_of`, `get`, `items()`, count,
  capacity, `KindStats`; `WorldStats`. Files: `entities.hpp`, `detail/world_layout.hpp`, `src/entities/world_stats.cpp`.
  Test: `tests/entities/test_storage.cpp`. Covers FR-15, FR-24.

## Stage 3: destroy, hooks, flush, clear, iteration (pure)

- [x] **T3.1** `destroy`: step 1 hooks in registration order with every component attached, step 2 components `T{}`
  and quarantined, tags zeroed, handle stale; dying flag; FIFO queue for destroys from hooks (each entity once, no
  recursion: a 1000-long hook chain with constant stack depth); `remove<T>` runs its hook (component readable),
  `remove` from its own hook and on a dying entity → false; destroy of the hooked entity from a `remove` hook processed
  after; hooks that spawn; add to a dying entity asserts. Files: `src/entities/world_destroy.cpp`,
  `world_components.cpp`. Test: `tests/entities/test_destroy_hooks.cpp`. Covers FR-13.
- [x] **T3.2** `flush`, `clear`, `clearing`: quarantine of entities and dirty kinds, clear in ascending slot order with
  hooks, `spawn` → null (no assert) while clearing, first-load slots after clear (generations differ), extents back to
  0. Files: `src/entities/world_flush.cpp`. Test: `tests/entities/test_flush_clear.cpp`. Covers FR-14.
- [x] **T3.3** `each<A, B...>`: the first kind's ascending slots (extent snapshotted), the fold over the other kinds,
  fresh skipping (only when `any_fresh_`), live word reloaded after each callback, the iteration guard, the outermost
  end clearing fresh words; `entities()` range with the same rules; `flush` inside asserts. Files: `entities.hpp`,
  `detail/world_layout.hpp`, `world_flush.cpp`. Test: `tests/entities/test_each.cpp` (one/two/three kinds, the order
  rule with A rarer and with B rarer, destroy/remove/spawn/add of self, earlier and later entities, into holes, nested
  `each`, `break` out of `entities()`). Covers FR-15, FR-14.
- [x] **T3.4** Reference model: `tests/entities/world_model.hpp` (plain arrays, linear searches, the plan's rules
  written the obvious way, including the hook queue and quarantine) and `test_world_model.cpp`: 50 seeded sequences
  of 5000 operations (spawn, destroy, add/remove of three kinds with hooks that destroy and spawn, flush, clear,
  iterations with destroys inside) compared after every operation (handles, liveness, component values, visiting
  orders, stats), and `events::stats()` unchanged. Covers FR-13..FR-15, FR-19, FR-21.
- [x] **T3.5** [P] Misuse sweep (FR-23's list: assert hook in Debug, harmless in the Release path) and cost: the
  allocation counter unchanged over 100 000 operations after creation, no log line in the normal path; destroying a
  world from inside its own hook (busy: freed by the outermost operation). Files: tests only, fixes where needed. Test:
  `tests/entities/test_world_misuse.cpp`. Covers FR-22, FR-23.

## Stage 4: tags (pure)

- [x] **T4.1** `Tag`, `tag()` (literal and sized buffer, `constexpr`: `case` labels and `static_assert` against FNV-1a
  test vectors), `tags_distinct` (true, a duplicate, `Tag::None`; one `static_assert`, the rest as values). Files:
  `entities.hpp`. Test: `tests/entities/test_tags.cpp` (part 1). Covers FR-16.
- [x] **T4.2** `add_tag`, `remove_tag`, `has_tag`: stale → false, `Tag::None` asserts, already there → false, full →
  assert; tags gone after destroy; `tags_per_entity` 0 (every add asserts). Files: `src/entities/world_tags.cpp`. Test:
  `test_tags.cpp` (part 2). Covers FR-17.
- [x] **T4.3** `find`, `first`: record scans specialised for 1..8 tags (a template instantiated per count in the
  `.cpp`), ascending order, total beyond capacity, null + 0 counts, null + capacity > 0 asserts; `each_tagged` with
  destroys and spawns inside. The model (T3.4) gains tags and `find`. Files: `src/entities/tags_find.cpp`,
  `entities.hpp`. Test: `tests/entities/test_find.cpp`, `test_world_model.cpp`. Covers FR-17, FR-21.
- [x] **T4.4** [P] Fuzz: `tests/fuzz/fuzz_world_ops.cpp` turns bytes into operations against the model (mini-fuzz in
  `nnh_tests`, libFuzzer with `NNH_BUILD_FUZZERS`). Files: `tests/fuzz/*`, `tests/CMakeLists.txt`. Covers X, FR-21.

## Stage 5: scene binding, interop, the golden script

- [x] **T5.1** Pool binding: node bytes in the inline pool (`static_assert` against the internal struct), the node in
  the run-time pool's block, `Pool(Scene&)`, `bind`, notify (`BeforeUpdate` → flush, `AfterExit` → clear), moving a
  bound run-time pool asserts and moves nothing. `scene.hpp`'s destructor message names "a bound TweenSet, World or
  Pool". Files: `src/pool/pool_scene.cpp`, `pool.hpp`, `engine/core/include/nnh/scene.hpp`. Test:
  `tests/pool/test_pool_scene.cpp` (flushed before the scene's update and only on top, frozen under a pushed scene,
  cleared after `on_exit`, empty on re-entry). Covers FR-18.
- [x] **T5.2** World binding: `bind`, notify, the D30 proof as tests: hooks run right after `on_exit` and before the
  next scene's `on_enter` (call log), no events published (bus stats), `spawn` refused in the exit clear, a hook
  requesting a scene change is queued, attach order with a bound `TweenSet` (world first: hooks kill tweens, then the
  set clears; world second: `kill` on stale handles is harmless), destroying a scene with a world still attached
  asserts, moving a bound world asserts. Files: `src/entities/world_scene.cpp`. Test:
  `tests/entities/test_world_scene.cpp`. Covers FR-18, FR-19, AC-3.
- [x] **T5.3** Interop (no include between modules): a `collision::box_view` over `storage<Body>()` with live, free and
  quarantined slots — `boxes_overlapping`, `boxes_at` and `raycast` report only live bodies, `owner(index)` gives
  their entities; a `SpriteAnimator` component updated in place with a callback capturing `[this]` that destroys its
  entity; a tween writing a component through a `Setter` capturing `[&world, e]` stops writing after `destroy`; a
  pointer-targeted tween killed by the kind's remove hook. Files: tests only. Test: `tests/entities/test_interop.cpp`.
  Covers FR-20, AC-3.
- [x] **T5.4** Self-check (AC-2): `examples/entities/self_check.{hpp,cpp}` (pure: the plan's 100 000-operation script
  on a 1024-entity world and a 256-slot run-time pool, FNV-1a over handles, visits, `find` results and stats,
  checkpoints every 10 000); golden values from pc-debug in `kEntitiesCheckGolden`, equal in pc-release. Files:
  `examples/entities/self_check.{hpp,cpp}`, `tests/entities_example/test_self_check.cpp`, `tests/CMakeLists.txt`.
  Test: the unit test against the goldens. Covers AC-2, FR-21.
- [x] **T5.5** Stage end: ctest; opt-in: `hello` sizes and `nm` (no pool/world symbol) equal T0.1 on PC and 3DS
  (3ds-release built without a warning); AC-8 comparison (every reference identical). Files: plan.md ("Stage 1–5
  results"). Covers FR-25, AC-8.

## Stage 6: the platformer's swap (same play)

- [x] **T6.1** Level world and coins: `level_world.{hpp,cpp}` (`Body`, `Coin`, `Turret` components, tags `"player"`,
  `"coin"`, `"turret"` with `static_assert(nnh::tags_distinct(...))`, the description); `coins.{hpp,cpp}` replaces
  `coin_book.*`: `spawn_coin`, `collect_coins` = `boxes_overlapping` over the body storage with the player's mask,
  `owner` → value → destroy, ascending; `test_coin_book.cpp` becomes `test_coins.cpp` with the same cases and expected
  results. Files: `examples/platformer/level_world.*`, `coins.*`, `CMakeLists.txt`, `tests/platformer/test_coins.cpp`,
  `tests/CMakeLists.txt`. Covers AC-4 step 1.
- [x] **T6.2** Turrets: turret entities (`Body{box, kEnemy}` + `Turret`), bullets in `Pool<Bullet, 16>` (flushed at the
  start of `update()`), bodies read through a box view of the body storage, a hit mapped by `owner` to the player
  entity; `state()` over `each<Turret>` then the pool's `items()`; `test_turrets.cpp` keeps every case (tests that fire
  between updates adapted for quarantine, as plan risk 6 says). Files: `examples/platformer/turrets.{hpp,cpp}`,
  `tests/platformer/test_turrets.cpp`. Covers AC-4 step 1.
- [x] **T6.3** `LevelPlay`: the world created and bound to the level scene (`LevelScene` passes `*this`), the turrets'
  pool bound too; the player is an entity whose body is written once per frame; restart = `clear()` + spawns (player,
  then map objects in order); coins and turrets drawn from the world in slot order. Files: `level_play.{hpp,cpp}`,
  `level_scene.{hpp,cpp}`. Test: ctest `platformer_replay_matches_script` passes with **unchanged** lines;
  `nnh run platformer --pc --no-build --script examples/platformer/tests/level.nnhs` → PASS; AC-8 comparison (level 1,
  the replay at 312 and the bench identical). If a byte differs, stop and explain in plan.md before touching a golden
  line. Files: plan.md ("Stage 6 results"). Covers AC-4 step 1, FR-18, FR-20.

## Stage 7: `examples/entities` (PC)

- [x] **T7.1** Example core: components, tags (`static_assert(tags_distinct(...))`), the arena on the top screen
  (walls as shapes), walkers (`Body` + `Velocity`, `"enemy"`), flyers (+ a `Bob` driven by the scene's bound
  `TweenSet` through a handle-capturing setter, `"enemy"`, `"flying"`), coins (`"coin"`); the ship (Circle Pad/D-Pad)
  firing bullets from `Pool<Bullet, 32>` (A; HUD live/capacity/misses); movement by `each<Body, Velocity>`; the world
  and pool bound to the showcase scene (world before the tween set). Files: `examples/entities/*`,
  `examples/CMakeLists.txt`. Test: builds; PC screenshot `ENTITIES_DEMO=showcase` read back. Covers AC-5, FR-15,
  FR-18.
- [x] **T7.2** Homing missile and tags: the missile keeps its target's `Entity`; when it goes stale the HUD and log say
  `target lost (stale handle slot n gen g)` and it retargets with `first(kEnemy)`; X cycles the filter (none → enemy →
  flying → coin) highlighting `find`'s entities and logging `filter: <tag> found N`; L/R freeze/unfreeze the group with
  `each_tagged`. Files: `showcase_scene.cpp`, `arena.cpp`, `hud.cpp`. Test: PC screenshots (tag highlight, stale
  retarget) read back. Covers AC-5, FR-2, FR-17.
- [x] **T7.3** Touch to destroy: the bottom screen shows the arena through a hand-set `Camera2D`; a touch becomes a
  world point, `boxes_at` over the body storage picks the entity, which is destroyed (its `Body` hook spawns an
  `Explosion` unless `clearing()`), logging `destroyed: slot n gen g tags enemy,flying`; the HUD lists `WorldStats`
  and per-kind stats. Files: `showcase_scene.cpp`, `arena.cpp`, `hud.cpp`. Test: PC screenshot `touch` and the log lines
  in a `--pc` run. Covers AC-5, FR-13, FR-20, FR-24.
- [x] **T7.4** Self-check scene: SELECT runs T5.4's script, 2000 operations per frame, logging
  `entities: <n> sum <checksum>` and `entities: PASS <checksum>` / `entities: FAIL ...`; `ENTITIES_DEMO=check` starts
  in it; ctest `entities_self_check_pc`. Files: `self_check_scene.{hpp,cpp}`, `main.cpp`, `tests/CMakeLists.txt`. Test:
  ctest. Covers AC-2, AC-6.
- [x] **T7.5** [P] Bench (AC-7): `bench.{hpp,cpp}` (the five workloads, pure) and `bench_scene.{hpp,cpp}` (300 frames
  each, profile scopes, one line per workload `entities bench: <name> avg X ms worst Y ms (target < Z ms) PASS|FAIL`,
  `get` through `Storage<T>::get` logged for information, then the verdict line `entities bench: PASS` or
  `entities bench: FAIL (<names>)`); Y runs it, `ENTITIES_DEMO=bench` starts in it. Files: `examples/entities/bench*`.
  Test: PC run logs every line. Covers AC-7.
- [x] **T7.6** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -dr` of the 3ds-dev objects of
  `bench.cpp` and the module; for each AC-7 hot loop record instructions per element, calls, `vmrs`, `ldm`/`stm` of
  stack structs, `__aeabi_*`, `memcpy`/`memset` — expected per plan.md's ARM11 table: none of those in `each`, `get`,
  `find`'s inner loop or the pool pair; lowest-bit scans as `clz`. Fix what differs (no result changes: every golden
  holds). Files: plan.md ("Stage 7 ARM pre-check": the table). Covers AC-7, II.
- [x] **T7.7** The script `examples/entities/tests/check.nnhs`: SELECT → every checkpoint and PASS line; fixed touches
  with expected `destroyed:` lines, X cycles with expected `filter:` lines, a fire hold to `pool: full, misses N`, the
  missile's `target lost` line, screenshots (showcase, tag highlight, stale retarget, full pool, touch); Y → bench
  lines and `entities bench: PASS`; `fail-on "entities: FAIL"`, `fail-on "entities bench: FAIL"`, `fail-on "] E "`.
  Test: `nnh run entities --pc --no-build --script examples/entities/tests/check.nnhs` → PASS, twice. Covers AC-5,
  AC-6, AC-7.

## Stage 8: the platformer's sparkles, opt-in checks

- [ ] **T8.1** Sparkles, pure: `sparkles.{hpp,cpp}` (4 per coin at its centre, fixed velocities of short binary
  fractions, tag `"fx"`, `Spark{position, alpha, fade}` + `Velocity`, faded over 24 frames by a `TweenSet` through a
  `Setter` capturing `[this, e]` that writes only while `alive(e)`, `on_finish` destroys the sparkle, `Spark`'s hook
  kills its tween unless `clearing()`). Files: `examples/platformer/sparkles.*`, `level_world.*`. Test:
  `tests/platformer/test_sparkles.cpp` (spawned count, faded and gone after 24 updates, slots reused only after a
  flush, a clear leaves no tween running). Covers AC-4 step 2, FR-13, FR-20.
- [ ] **T8.2** Sparkles in the level: taking a coin bursts it; the level scene's `TweenSet` bound after the world; the
  HUD shows the world's live count; `ReplaySample::entities` and `::handles` join the checksum after the turrets' word.
  Regenerate the replay lines with `PLATFORMER_DEMO=replay NNH_EXIT_AFTER_FRAMES=920
  build/pc-debug/examples/platformer/platformer.exe` and copy them into `examples/platformer/tests/level.nnhs`, header
  comment updated, **in the same commit**, whose message says why (the x, y and coins columns must not change). Files:
  `level_play.{hpp,cpp}`, `level_scene.{hpp,cpp}`, `replay_run.{hpp,cpp}`, `hud.{hpp,cpp}`, `tests/level.nnhs`,
  `tests/platformer/test_replay_run.cpp`. Test: ctest `platformer_replay_matches_script`; `level.nnhs --pc` → PASS; a PC
  screenshot (`PLATFORMER_DEMO=replay NNH_EXIT_AFTER_FRAMES=<frame chosen here>`) read back: a coin's sparkles and the
  entity count on the HUD (frame recorded in plan.md). Covers AC-4 step 2.
- [ ] **T8.3** [P] Mutation pass as in plan 019: hand mutants of `src/pool/*`, `src/entities/*`, the inline headers,
  `coins`, `turrets`, `sparkles` and the replay word, one at a time with the whole `nnh_tests` (and pc-release for
  Release-path mutants); survivors become tests or are argued equivalent. Files: plan.md ("Mutation testing"). Covers
  VI.
- [ ] **T8.4** Opt-in and regression: `hello` sizes and `nm` equal T0.1; AC-8 comparison (bench, `camera`, `queries`,
  `sprites`, `scenes` identical; level 1 may differ by the HUD count only); every example's script passes with `--pc`
  (`gate.nnhs` included); `.text` growth of the platformer and `examples/entities` recorded; build times of an
  `examples/entities` edit (simulator < 2 s, `.3dsx` < 5 s). Files: plan.md ("Stage 8 results"). Covers FR-25, AC-8,
  III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; the AC-2
  golden, the model comparisons and the platformer's replay are identical on all three (they are test assertions);
  ASan/UBSan clean over the `Counted` balance tests, the model sequences and the destroy-in-hook chains; libFuzzer
  `fuzz_world_ops` 10 minutes, clean. Files: tasks.md (results). Covers I, X, FR-21, AC-1, AC-2.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development
  builds of `examples/entities` and `examples/platformer` without a warning; T7.6's table still true; `hello.elf`
  sizes as in T0.1. Files: plan.md ("Stage 10: console run (prepared)": commands and every expected line). Covers
  AC-8, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Three runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run entities --script examples/entities/tests/check.nnhs` → every
     `entities: <n> sum` line and `entities: PASS` equal the PC's, every `destroyed:`, `filter:`, `pool:` and
     `target lost` line as on the PC, the bench lines and the verdict (numbers recorded); screenshots read back;
  2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the regenerated
     replay lines, `60 Hz PASS`;
  3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS`
     (spec 012's gate still passes).
  Files: plan.md ("Stage 10 results": verdicts, bench numbers, log paths). Covers AC-4, AC-6, AC-7.
- [ ] **T10.3** (only if a bench target is missed) Console bench refinement: find the cost in the 3ds-dev disassembly,
  apply plan risks 1–2 without changing a result (every golden holds), then T10.2 run 1 again; if still missed, the
  maintainer sets the budget from the measurement with margin, written in the spec's AC-7 and
  `docs/api/entities.md`/`pool.md` with the numbers. Files: plan.md ("Console bench refinement"). Covers AC-7.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** `docs/api/pool.md` (handles and stale use, the two forms, acquire/release, quarantine and flush,
  iteration rules, raw access, binding, costs measured in T10.2, the wrap limit, misuse) and `docs/api/entities.md`
  (worlds, components and kinds, `component<T>` and hooks, destroy and the hook queue, iteration rules, storage views,
  tags and `tags_distinct`, Godot groups as `each_tagged`, binding and the "world before tween sets" rule with
  `clearing()`, the box-view, animator and tween patterns, the `EnemyDefeated` event pattern, determinism, memory
  formula, costs, misuse); header comments (allocates? per-frame safe? misuse?). Covers VIII, AC-9.
- [ ] **T11.2** Other pages: `docs/api/scene.md` and `docs/architecture/scenes.md` (attachment users: bound pools and
  worlds), `docs/architecture/patterns.md` (Object Pool row → the module), `docs/REFERENCES.md` (EnTT, flecs, Bevy,
  Godot, Unity, *Game Programming Patterns*, Fabian, Bitsquid), `CLAUDE.md` (layout paragraph: pools, entities,
  `examples/entities`); `python -I scripts/docs_tool.py check` → 0 broken links. Covers AC-9.
- [ ] **T11.3** `docs/architecture/decisions.md`: D35 change-log note "implemented" with any stage refinements.
  Covers AC-9.
- [ ] **T11.4** Final measurements (header cost of `pool.hpp` and `entities.hpp` against T0.1's references, the
  10-`each` TU, build times, `hello` sizes) in plan.md and `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T11.5** `docs/ROADMAP.md`: "Entities/components", "Tags and groups" and "Object pools" checkboxes.
- [ ] **T11.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2
  pending").
