# Tasks 022: Scene tree and transforms

Status: approved 2026-11-23 (overnight pre-approval)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The transform math comes first, pure and proven against a `double` reference with spec 015's sine moved and
its bits unchanged; the entities-internal additions are proven against spec 020's own calls and goldens before the
hierarchy uses them; the hierarchy and the prefab trees are pure and tested on the fake platform; AC-2's twin-world
golden proves the prefab-tree equivalence; the platformer adopts barrels (stage 6) and its replay lines are
**regenerated deliberately** (decision 24). The ARM11 disassembly is checked against the plan's cost table before the
first console run (T7.6). Stage 9 is run on the Linux build VM by the lead; the only console stage is stage 10: five
scripted runs the maintainer only starts (Homebrew Launcher → Y). Deviations found while building go into plan.md as
"Stage N refinements", as in plans 012–021.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release without a warning;
`hello` sizes and `nm` equal T0.1; `entities: PASS C8FB9A41` (ctest `entities_self_check_pc`), `prefabs: PASS
7BB7901E` (ctest `prefabs_self_check_pc`), `tweens: PASS B44CA3F6` (`examples/tweens/tests/check.nnhs --pc`); the
replay ctest with the lines of the moment (spec 021's until T6.3, the new ones after).

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-022/`, not committed): `hello.exe`
  (pc-release, pc-debug) and `hello.elf`/`.3dsx` (3ds-release, 3ds-dev) sizes and `nm -C` lists; edit
  `examples/prefabs/room.cpp` → simulator and `.3dsx` times (the reference for the hierarchy example's edit); clean
  `nnh_core`; preprocessed lines and `-fsyntax-only` time of one-line TUs including `<nnh/entities.hpp>` and
  `<nnh/math.hpp>`; `.text` of the platformer's, `prefabs`' and `entities`' 3ds-release ELFs; 3ds-dev disassembly
  counts (`fcount.py`): `World::spawn` 109, `detail::world_add` 143, `examples/entities` `bench.o` (`get` 33, `Storage::get`
  19, `each` 27), `detail::prefab_spawn`, `detail::sin_quarter`; saved listings `build/ref-022/dis0/`. Files: plan.md
  ("Stage 0 results"). Covers III, FR-24, AC-8.
- [x] **T0.2** [P] Reference screenshots for AC-8 in `build/ref-022/ref/`: `hello`, `sprites`, `scenes`, `camera`
  (`CAMERA_DEMO=zoom1`), `queries` (`QUERIES_DEMO=all`), `entities` (showcase at 30), `prefabs` (showcase at 30),
  platformer `PLATFORMER_DEMO=bench` (top screen), level 1 at 90, `PLATFORMER_DEMO=replay` at 64 and 312;
  `build/ref-022/compare.sh` retakes them into `now/`. (The platformer's level and replay images are expected to change
  in stage 6: barrels; new references are taken there.) Files: plan.md. Covers AC-8.
- [x] **T0.3** [P] Goldens today: replay ctest (`sum B2418A28: 60 Hz PASS`), `C8FB9A41`, `7BB7901E`, `B44CA3F6`;
  `nnh_tests` count (raise `kMaxTests` if fewer than 500 are left: this spec adds ≈ 250). Files: plan.md. Covers VI.

## Stage 1: transform math (pure)

- [x] **T1.1** Move `sin_quarter` to `engine/core/src/core/det_sine.{hpp,cpp}` (same body; `tween/ease_math.hpp`
  includes the new header so `tests/tween/test_ease_math.cpp` compiles unchanged); `det_sine.cpp` in a source list with
  `-ffp-contract=off`. No new test: spec 015's tests and `tweens: PASS B44CA3F6` are the test; the 3ds-dev
  disassembly of `sin_quarter` and `ease.o` identical to T0.1's. Files: `src/core/det_sine.*`, `src/tween/ease_math.*`,
  `engine/core/CMakeLists.txt`. Covers FR-3.
- [x] **T1.2** `sin_cos`: `× float(2/π)`, nearest quarter by sign bits and truncation, the `2⁻²⁰` snap, quadrants,
  non-finite → {0, 1} + misuse, `|q| ≥ 2²³` integer path; inline in `src/transform/sin_cos.hpp`, public wrapper; the
  four constants. Test: `tests/transform/test_sin_cos.cpp` (constants and `k × kQuarterTurn` for k = −8..8 exact;
  `sin_cos(−x)` bits = negated; 4096 angles against `std::sin`/`std::cos` within 7·10⁻⁷ + snap; NaN, ±inf, 2³⁰ rad;
  monotonic around the snap). Files: `include/nnh/transform.hpp`, `src/transform/sin_cos.*`, CMake
  `_nnh_transform_sources`. Covers FR-3.
- [x] **T1.3** Values and composition: `Transform2D`, `Affine2D`, `WorldTransform2D`; inline root rule, pure-translation
  rule, full composition with drawing values and the mirror sign by bits (`src/transform/compose.hpp`); public `to_world`,
  `compose`. Test: `tests/transform/test_compose.cpp` (identity; scale → rotate → move order; root bit-equal to the
  local; translation rule vs the full rule on non-zero values; quarter turns on whole pixels; mirrors: positions,
  rotations the other way, drawing scale; shear: drawing values = accumulated rotation, product of scales). Covers FR-1,
  FR-2, FR-4.
- [x] **T1.4** Points and inverses: `transform_point`, `transform_direction`, `inverse` (exponent threshold),
  `to_local`, `is_finite`. Test: `tests/transform/test_affine.cpp` (round trips; a zero scale refused, a 2⁻⁶⁰ scale
  accepted, determinant below 2⁻¹⁰⁰ refused; directions ignore translation; finiteness on every special pattern).
  Files: `src/transform/affine.cpp`. Covers FR-2.
- [x] **T1.5** [P] `relative` (keep-world math) and `bounds`. Test: `tests/transform/test_relative.cpp` (compose(parent,
  relative(parent, w)) reproduces `w`'s points within the bound and its drawing values exactly for power-of-two scales
  and quarter turns; mirrored parents; non-invertible → false), `tests/transform/test_bounds.cpp` (unrotated exact with
  `w = |a|·r.w`, mirrored normalised, rotated enclosing, quarter turns exact). Files: `src/transform/relative.cpp`,
  `bounds.cpp`. Covers FR-10, FR-14.
- [x] **T1.6** Accuracy (open question 1): `tests/transform/test_accuracy.cpp` — 2000 seeded chains (depth 1..16,
  coordinates within ±4096 px, scales ¼–4, angles any) against a `double` reference using the engine's sine values
  (bound 1/32 px) and against `std::sin`/`std::cos` (the measured bound recorded); exactness cases (roots, unrotated
  unscaled chains = hand sums as IEEE values, quarter turns). Files: plan.md ("Stage 1 results": both bounds). Covers
  FR-3, AC-3.
- [x] **T1.7** Stage end: the stage-end guards; include check (`<nnh/transform.hpp>` preprocesses to `math.hpp` only);
  a probe TU shows no float arithmetic in the header (it has none). Files: plan.md. Covers FR-24, III.

## Stage 2: entities-internal additions (pure)

- [x] **T2.1** `WorldModuleState` at the end of `WorldCold` (zeroed at creation), `world_module_state`, `world_dying`,
  `world_busy`. Test: `tests/entities/test_world_module.cpp` (zero at creation; dying inside a destroy's hooks and while
  queued, false after; busy inside `each` and hooks). Files: `src/entities/entities_internal.hpp`, `world_internal.hpp`,
  `world_module.cpp`. Covers FR-11, FR-23.
- [x] **T2.2** `world_reserve_many`: room found first (the k-th free entity and kind slots by bit scans from the hint,
  no state change), then taken with `take_entity`/`attach_component`; refusals as the first failing `world_reserve`
  would count them; `Clearing`, `Empty`. Test: `tests/entities/test_world_reserve_many.cpp` (twin worlds on 20 seeded
  sequences of 2000 operations: `world_reserve_many(n ≤ 32)` vs `n` × `world_reserve` — every record word, bitmap,
  counter, owner and component byte equal; each refusal leaves the world byte-identical but its counter; inside a
  remove hook and an iteration (fresh, not visited)). Files: `src/entities/world_reserve_many.cpp`. Covers FR-17.
- [x] **T2.3** [P] Stage end: the stage-end guards; `test_world_reserve.cpp` unchanged and passing.
- [x] **T2.4** ARM guard: 3ds-dev disassembly of `World::spawn` (109), `world_add` (143), `world_destroy`/`process` and
  `examples/entities` `bench.o` identical to T0.1 (the cold part grew at its end only). Files: plan.md ("Stage 2
  results"). Covers II, FR-24.

## Stage 3: nodes and links (pure)

- [x] **T3.1** `Node` (layout in the plan's order, `static_assert(sizeof(Node) == 80)`, defaults, `Node(local)`, copy
  without links, assignment keeping links), `node_component(capacity)` and with a game hook (game's first, then
  `detail::node_removed`; a stub until T4.4), `Hierarchy(World&)` (asserts without the kind). Test:
  `tests/hierarchy/test_node.cpp`, `hierarchy_fixture.hpp` (kinds, a world maker, a block hash). Files:
  `include/nnh/hierarchy.hpp`, `detail/hierarchy_walk.hpp`, `src/hierarchy/hierarchy_internal.hpp`, `node.cpp`, CMake
  `_nnh_hierarchy_sources`. Covers FR-5.
- [x] **T3.2** Links: append (the first child's previous = the last child), unlink, subtree height and depth rewrite
  (stackless walks). Test: `tests/hierarchy/test_attach.cpp` (part 1: link invariants after seeded sequences checked by a
  brute-force rebuild). Files: `src/hierarchy/node_links.cpp`. Covers FR-6.
- [x] **T3.3** `can_attach`, `attach`, `detach`, `detach_children` in both modes; refusals in the plan's order (stale,
  dying, no node, cycle, self, depth 16, not invertible) — false, assert, stats refusal, block byte-identical; attach to
  the current parent; keep-world leaves the stored world unchanged, keep-local computes it; descendants keep locals.
  Test: `test_attach.cpp` (part 2), `tests/hierarchy/test_keep_world.cpp`. Files: `src/hierarchy/attach.cpp`. Covers
  FR-6, FR-10.
- [x] **T3.4** [P] Queries: `parent`, `depth`, `root`, `child_count`, `first_child`, `next_sibling`, `children()`
  range (next read before yielding), `is_ancestor`, `local`, `node`, `world_transform`, `stats` (refusals so far).
  Test: `tests/hierarchy/test_queries.cpp`. Files: `src/hierarchy/hierarchy_queries.cpp`, `detail/hierarchy_walk.hpp`.
  Covers FR-7, FR-22.
- [x] **T3.5** Stage end: the stage-end guards; include check (`<nnh/hierarchy.hpp>` pulls no gfx, collision, camera,
  tween, animation, tilemap or prefab header; preprocessed lines and `-fsyntax-only` time against T0.1's entities
  header). Files: plan.md ("Stage 3 results"). Covers FR-24, III.

## Stage 4: the pass, fresh values, walks, cascades, drawing (pure)

- [x] **T4.1** `propagate()`: roots in ascending slots, stackless depth-first walk, finiteness test (Release: identity +
  `bad_locals`), the sine cache, both composition rules, prefetch, stats at the end; refused inside iterations and
  hooks. Test: `tests/hierarchy/test_propagate.cpp` (visiting order by a call log in test builds, each node once, roots
  bit-equal to locals, nodes created/attached between passes, a sine counter shows rotation-unchanged nodes skip it,
  refusals). Files: `src/hierarchy/propagate.cpp`. Covers FR-8, FR-21.
- [x] **T4.2** `world_now`: climb (≤ 16 slots on the stack), same inline code, cache when its bits match. Test:
  `tests/hierarchy/test_world_now.cpp` (equals the pass's stored bits for every node of 50 seeded hierarchies before and
  after moves; identity without a node; the muzzle pattern). Files: `src/hierarchy/world_now.cpp`. Covers FR-9.
- [x] **T4.3** `each_descendant`: path, remembered next siblings, iteration guard (fresh nodes skipped), the cursor
  fix-ups on unlink (module state's walk list), nested walks. Test: `tests/hierarchy/test_each_descendant.cpp`
  (pre-order; destroying the current node, an ancestor, the next sibling, an unvisited subtree; detaching and attaching
  into and out of the unvisited part; spawning; nested walks). Files: `src/hierarchy/descendant_walk.cpp`,
  `node_links.cpp`. Covers FR-7.
- [x] **T4.4** The engine's remove hook: cascade when dying (children's links cleared, queued destroys: breadth-first in
  child order), removal when not (children become roots keeping their stored worlds, depths rewritten). Test:
  `tests/hierarchy/test_cascade.cpp` (hook order log, node data and last world intact in each hook, parent stale and
  `parent(child)` null, a hook destroying or attaching during a cascade, `detach_children` first, `clear()` with
  hierarchies and the first load's slots after it, a game hook first), `tests/hierarchy/test_remove_node.cpp`. Files:
  `src/hierarchy/node_removed.cpp`. Covers FR-11.
- [x] **T4.5** [P] Scene binding and quarantine. Test: `tests/hierarchy/test_scene_binding.cpp` (a bound world with
  nodes flushed before update and cleared after exit; node slots quarantined until the flush; no new D30 notification).
  Covers FR-12.
- [x] **T4.6** [P] Drawing: `gfx::transformed` and the `draw` overload (`src/gfx/draw_transform.cpp`, `-ffp-contract=off`;
  `gfx.hpp` forward-declares `WorldTransform2D`). Test: `tests/gfx/test_draw_transform.cpp` (the fake's recorded items:
  position, rotation, scale, flips, for given options and an animator's `draw_options()`; a mirrored world subtracts the
  given rotation; zoom applies). Covers FR-13.
- [x] **T4.7** [P] Debug draw (Debug/Development; Release compiles to nothing) and the camera pattern. Test:
  `tests/hierarchy/test_debug_draw.cpp` (recorded lines: one per non-root to its parent, two axes per node),
  `tests/hierarchy/test_camera_follow.cpp` (a `CameraRig` on a child's world after the pass equals one on the
  hand-computed point). Files: `src/hierarchy/hierarchy_debug_draw.cpp`. Covers FR-15, FR-22.
- [x] **T4.8** Misuse and cost: every FR-23 case asserts and is harmless on the Release path (pc-release); the
  allocation counter unchanged over 100 000 operations after creation; no log line in the normal path; `events::stats()`
  unchanged. Test: `tests/hierarchy/test_hierarchy_misuse.cpp`. Covers FR-20, FR-21, FR-23.
- [x] **T4.9** Stage end: the stage-end guards; `compare.sh` (every reference identical). Files: plan.md ("Stage 4
  results"). Covers AC-8.

## Stage 5: prefabs with children, the golden script

- [x] **T5.1** Registration: `PrefabChild`, `nnh::child()`, `detail::kPrefabTreeOps` (`prefab_tree.cpp`, `node_kind`),
  `PrefabOptions::children`, `PrefabLibraryDesc::max_children`, the record's `child_count`, the child array and tree facts
  (only when `max_children > 0`), inherited children in variants, every check (a child of another library, a
  `PrefabChild` not from `child()`, no node kind, 9 entries, 33 entities, 129 components, height 16, the capacity), the
  `children()` query, `place(&Node::local)`. Test: `tests/prefabs/test_children.cpp` (block hash unchanged on each
  refusal; spec 021's layout tests unchanged with `max_children = 0`). Files: `prefabs.hpp`, `detail/prefab_values.hpp`,
  `src/prefabs/prefab_internal.hpp`, `library_layout.*`, `library_add.cpp`, `prefab_flatten.cpp`, `prefab_tree_plan.cpp`,
  `library_queries.cpp`. Covers FR-16.
- [x] **T5.2** Tree spawn: the dispatch in `prefab_spawn`, the creation list, kinds, `world_reserve_many`, copies,
  placement, the root's world after placement, children linked keep-local with their entry locals, post-order hooks
  (each chain base first), the root's handle. Test: `tests/prefabs/test_tree_spawn.cpp` (twin worlds: prefab trees vs
  the plan's hand-written equivalent — handles, slots, bytes, tags, links, world bits; hooks find children ready; a hook
  destroying its own entity or spawning under itself). Files: `src/prefabs/prefab_spawn.cpp`, `prefab_tree.cpp`.
  Covers FR-17.
- [x] **T5.3** All or nothing and `spawn_under`: full world, each kind full (root's, a child's, an override-only kind),
  too deep under a parent — nothing created, misses in the library and the world, `WhenFull::Assert` vs `Miss`; inside
  `clear()` null, nothing counted; under a parent: position local, depth, attach keep-local. Test:
  `tests/prefabs/test_tree_all_or_nothing.cpp`, `tests/prefabs/test_spawn_under.cpp`. Covers FR-17, FR-18.
- [x] **T5.4** Map objects with children: `make_fixtures.py` writes `tests/data/maps/good_prefab_trees.tmj` (objects of
  types `ship` and `crate`, one `prefab` = `ship_shielded`); converted for `nnh_tests`; ctest
  `map_convert_accepts_good_prefab_trees`. Test: `tests/prefabs/test_map_trees.cpp` (whole hierarchies in map order, the
  root placed by the object, every `SpawnInfo` carrying the root's object, the summary). Covers FR-18.
- [x] **T5.5** Self-check (AC-2): `examples/hierarchy/check_kinds.hpp`, `self_check_defs.cpp` (the hand table written
  independently), `self_check_sides.{hpp,cpp}` (`Side<true>` through the library, `Side<false>` by hand, the same hook
  code), `self_check.{hpp,cpp}` (plan's 12 prefabs, kinds A–D, two worlds of 1024 × 4 tags, 50 000 operations, a pass
  every 64 with `world_now` = stored on every node, per-step handle comparison, full-state hashes every 1000 and after
  clears, checkpoints every 10 000); goldens from pc-debug in `kHierarchyCheckGolden`, equal in pc-release. Test:
  `tests/hierarchy_example/test_self_check.cpp`; a one-value mistake in the hand table fails. Files: those,
  `tests/CMakeLists.txt`. Covers AC-2, FR-17, FR-19.
- [x] **T5.6** [P] Fuzz: `tests/fuzz/fuzz_hierarchy_ops.cpp` (every 4 bytes one self-check operation, both sides
  compared), `tests/fuzz/fuzz_transform.cpp` (random float bits into every transform function: no UB, `sin_cos` finite,
  `inverse` refusals by exponent); mini-fuzz in `nnh_tests`, libFuzzer with `NNH_BUILD_FUZZERS`. Covers X, FR-23.
- [x] **T5.7** Stage end: the stage-end guards (`7BB7901E` with the dispatch in); `compare.sh`; `linkcheck.sh`: only
  `prefab_tree.o` refers to hierarchy symbols among prefab objects, and only `nnh::child()`'s `kPrefabTreeOps` refers to
  `prefab_tree.o`. Files: plan.md ("Stage 5 results": goldens and checkpoints, the script's reach). Covers FR-24, AC-8.

## Stage 6: the platformer adopts barrels (replay lines regenerated)

- [x] **T6.1** Art and kinds: `art/make_turret.py` → `assets/art/turret.png` (original base and barrel); `level::Barrel`,
  `node_component(8)` registered after the existing kinds; the library's `barrel` prefab and the `turret` prefab's node,
  child entry and centring hook (`max_children` 1). Test: `tests/platformer/test_level_prefabs.cpp` (a turret from a
  map object has a barrel child at the box's centre; its Body equals the old one's bytes). Files:
  `examples/platformer/level_world.*`, `level_prefabs.*`, `art/*`, `CMakeLists.txt`. Covers AC-4.
- [x] **T6.2** Aim and fire: `TurretBook` with a `Hierarchy`; the barrel turns to one of the 8 angles of `aim(d)` while
  the turret sees the player; shots from `transform_point(world_now(barrel).map, kMuzzle)`; `state()` hashes each barrel's
  stored world origin and rotation; `LevelPlay` calls `propagate()` after the turrets and before the camera; turrets
  and barrels drawn with `gfx::draw(kTop, tex, world, options)`. Test: `tests/platformer/test_barrels.cpp` (8 directions
  → 8 angles; the muzzle point for each, exact on the axes; a shot starts there), `test_turrets.cpp` (expected starts
  updated: from the muzzle). Files: `turrets.*`, `level_play.*`. Covers AC-4.
- [x] **T6.3** Regenerate the replay lines (plan "The platformer"): the ctest fails as expected; `PLATFORMER_DEMO=replay
  NNH_EXIT_AFTER_FRAMES=920 build/pc-debug/examples/platformer/platformer.exe` → copy the 15 `replay: … sum` lines into
  `examples/platformer/tests/level.nnhs` (the last up to its colon); pc-release prints the same lines; the header
  comment says why (spec 022 T6.3); the x, y and coins columns unchanged — else stop and explain in plan.md; ctest
  `platformer_replay_matches_script` passes; `nnh run platformer --pc --no-build --script
  examples/platformer/tests/level.nnhs` → PASS; new references for level 1 at 90 and the replay at 64 and 312 (barrels
  visible: read back), the bench screenshot identical. One commit with the barrels. Files: `tests/level.nnhs`, plan.md
  ("Stage 6 results": old and new last lines). Covers AC-4.
- [x] **T6.4** Stage end: the stage-end guards (new lines); `gate.nnhs --pc`, `zoom_gate.nnhs --pc` PASS. Covers AC-4, AC-8.

## Stage 7: `examples/hierarchy` (PC)

- [ ] **T7.1** Art and yard: `art/make_art.py` (original ship, gun, hand, walker, hat, cart, rider, squash block,
  crates, bullet, posts), `components.hpp`, `yard.{hpp,cpp}` (world bound to the showcase scene, library with
  `ship`/`walker`/`cart`/`squash`/`crate` trees in `yard_prefabs.cpp`, the hierarchy, the frame order), `ship`, `walker`
  (turning by `scale.x = −1`, the hat's bob tween by pointer), `cart` (the rider followed by a `CameraRig` on the top
  screen), `yard_draw` (every node through the gfx overload, centred pivots), `hud` (FR-22's stats). Test: PC
  screenshots `HIERARCHY_DEMO=showcase` (the yard; a mirrored walker) read back. Files: `examples/hierarchy/*`,
  `examples/CMakeLists.txt`. Covers AC-5, FR-4, FR-13, FR-15.
- [ ] **T7.2** Picking and reparenting: bodies from bounds after the pass (`each<Node, Body>`), a touch on the bottom
  screen (zoom 0.5) picks through `collision::boxes_at` and logs `picked: …`; X attaches to the hand / drops (`attached:
  …`, `dropped: …` at the same position); B destroys the ship (`destroyed: ship + n children`); D-Pad up detaches its
  children (`detached: n children`); D-Pad down toggles the debug draw; X with nothing picked restarts (`yard: restarted
  nodes n`). Files: `picking.*`, `showcase_scene.*`, `main.cpp`, `demos.hpp`. Test: PC screenshots (carried crate, a
  cascade, the debug draw) read back; the lines in a `--pc` run. Covers AC-5, FR-10, FR-11, FR-14, FR-22.
- [ ] **T7.3** Self-check scene: SELECT runs T5.5's script, 2000 operations per frame, logging `hierarchy: <n> sum
  <checksum>` and `hierarchy: PASS <checksum>` / `hierarchy: FAIL ...`; `HIERARCHY_DEMO=check` starts in it; ctest
  `hierarchy_self_check_pc`. Files: `self_check_scene.*`, `tests/CMakeLists.txt`. Covers AC-2, AC-6.
- [ ] **T7.4** [P] Bench (AC-7): `bench.{hpp,cpp}` (the five workloads as plan.md: prefab-built 1024-node world for the
  passes, a shuffled hand-built one for `world_now`, two 256-entity worlds for the spawn on alternate frames, the
  32-parent/64-item reparent set) and `bench_scene.{hpp,cpp}` (300 frames each, profile scopes around the measured call
  only, one line per workload, the spawn line's hand time and ratio, the verdict; Debug: `not judged (Debug)`); Y runs
  it, `HIERARCHY_DEMO=bench` starts in it. Test: a pc-dev run logs every line and PASS. Covers AC-7.
- [ ] **T7.5** The script `examples/hierarchy/tests/check.nnhs`: SELECT → every checkpoint and the PASS line; the
  restart line; fixed touches and buttons with their expected `picked:`/`attached:`/`dropped:`/`detached:`/`destroyed:`
  lines (a `wait 3` after every `expect` an input follows); screenshots (showcase, mirrored walker, carried crate,
  cascade, debug draw); Y → the bench lines and `hierarchy bench: PASS`; `fail-on "hierarchy: FAIL"`, `fail-on
  "hierarchy bench: FAIL"`, `fail-on "] E "`; ctest `hierarchy_check_script_pc` (`RESOURCE_LOCK nnh_remote_port`).
  Test: `nnh run hierarchy --pc --no-build --script examples/hierarchy/tests/check.nnhs` → PASS, twice. Covers AC-5,
  AC-6, AC-7.
- [ ] **T7.6** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects of
  `examples/hierarchy/bench.o`, `src/hierarchy/*`, `src/transform/*`, `src/core/det_sine.o`, `src/prefabs/prefab_tree.o`,
  `prefab_spawn.o`, `src/entities/world_reserve_many.o`, `world_spawn.o`, `world_components.o`; per AC-7 hot path record
  instructions, calls, stack reloads, `pld`, `vmrs`, `vdiv`, `__aeabi_*`, `memcpy`/`memset`, shift-add chains, and compare
  with plan.md's table (pass ≈ 80 per node with no call and two `pld`; the sine inline; `world_now` ≈ 270 for depth 3;
  keep-world ≈ 200 + 3 `vdiv`; tree spawn ≈ 320 per entity; `World::spawn` 109 and `world_add` 143 unchanged;
  `prefab_spawn` + the dispatch's load and branch only; `examples/entities` `bench.o` identical). Decide `sin_quarter`'s
  clamp (plan item 5). Fix what differs without changing a result (every golden holds). Files: plan.md ("Stage 7 ARM
  pre-check": the table and the expected console numbers). Covers AC-7, II.

## Stage 8: mutation testing, opt-in checks

- [ ] **T8.1** Mutation pass as in plans 019–021: hand mutants (`build/ref-022/mutants.py`, applied one at a time by
  `mutate.py`, the whole `nnh_tests` each) of `src/transform/*`, `src/core/det_sine.cpp`, `src/hierarchy/*`,
  `src/entities/world_reserve_many.cpp` and `world_module.cpp`, the prefab tree and registration changes, `gfx/
  draw_transform.cpp`, the headers' templates and the platformer's barrels; Release-path mutants in pc-release;
  survivors become tests or are argued equivalent. Files: plan.md ("Mutation testing"). Covers VI.
- [ ] **T8.2** [P] Release strings: `scripts/check_release_strings.sh` on the 3ds-release and pc-release `hierarchy`
  executables passes (add Development-only texts to its list if any). Covers FR-22, FR-23.
- [ ] **T8.3** [P] Opt-in by linking: probes (`build/ref-022/probe_*`, not committed) linked against the 3ds-release
  libraries: a transform-only game (`compose`, `sin_cos`) has no `nnh::World` symbol; a game with prefabs without
  children has no `nnh::Hierarchy`, `node_removed`, `prefab_tree` or `propagate` symbol; `hello` sizes and `nm` equal
  T0.1 on PC and 3DS. Files: plan.md ("Stage 8 results"). Covers FR-24, AC-8.
- [ ] **T8.4** Regression and build times: `compare.sh` (every reference of T0.2 identical, the platformer's new ones
  from T6.3); every example's console script passes with `--pc` (actions, animation, camera, data, entities, foundation
  ×2, input, platformer `gate`, `level`, `zoom_gate`, prefabs, queries, tweens, hierarchy); `.text` of the platformer,
  `prefabs`, `entities`, `hierarchy` recorded; edit `examples/hierarchy/yard.cpp` → simulator and `.3dsx` times against
  T0.1's and constitution III. Files: plan.md ("Stage 8 results"). Covers AC-8, III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; AC-2's golden
  and checkpoints, the twin-world tests (`test_world_reserve_many`, `test_tree_spawn`), the accuracy and exactness tests,
  spec 020's `C8FB9A41`, spec 021's `7BB7901E`, the replay ctest with T6.3's lines and `tweens` identical on all three
  (they are assertions); ASan/UBSan clean over the walk cursor, cascades from hooks, the tree spawn's stack lists and
  the misuse Release paths; libFuzzer `fuzz_hierarchy_ops` and `fuzz_transform`, 10 minutes each, clean. Files:
  tasks.md (results), plan.md ("Stage 9 results"). Covers I, X, FR-19, AC-1, AC-2.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development
  builds of `hierarchy`, `platformer`, `prefabs` and `entities` without a warning; T7.6's table still true; `hello.elf`
  sizes as in T0.1; plan.md's "Stage 10: console run (prepared)" completed with the recorded goldens, checkpoints and
  showcase lines. Covers AC-8, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Five runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run hierarchy --script examples/hierarchy/tests/check.nnhs` → every `hierarchy: <n>
     sum` line and `hierarchy: PASS` equal the PC's, the showcase lines as on the PC (`dropped:` at the `attached:`
     position), the bench lines and the verdict (numbers recorded); screenshots read back;
  2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → **T6.3's new** replay
     lines, `60 Hz PASS`;
  3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS`;
  4. `build/pc-debug/tools/nnh/nnh run prefabs --script examples/prefabs/tests/check.nnhs` → `prefabs: PASS 7BB7901E`
     and `prefabs bench: PASS` (the tree dispatch);
  5. `build/pc-debug/tools/nnh/nnh run entities --script examples/entities/tests/check.nnhs` → `entities: PASS
     C8FB9A41` and `entities bench: PASS` (the grown cold part).
  Files: plan.md ("Console run 1": verdicts, bench numbers, log paths). Covers AC-4, AC-6, AC-7.
- [ ] **T10.3** (only if a bench target is missed) Console bench refinement: find the cost in the 3ds-dev disassembly,
  apply plan risk 1's list without changing a result (every golden holds; tests first for any behaviour-preserving
  rewrite), then T10.2 run 1 again; if still missed, the budget is set from the measurement with ≈ 25 % margin, written
  in the spec's AC-7, `bench_scene.cpp` and `docs/api/hierarchy.md` with the numbers. Files: plan.md ("Optimisation
  round", "Console run 2 and budgets set"). Covers AC-7.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** (the Old 3DS costs added after T10.2) `docs/api/transform.md` (values, units, the composition order,
  mirroring, determinism and the exactness rules, the snap, edge cases, shear and the drawing approximation, points,
  inverses, `relative`, bounds, the accuracy numbers, the subnormal and signed-zero notes) and `docs/api/hierarchy.md`
  (the node kind and its registration, attach/detach and modes, `can_attach`, queries, `each_descendant`'s rules, the
  pass and its place in a frame, `world_now`, when a stored world is computed, keep-world from stored worlds, cascades,
  `detach_children`, removal, drawing, the bounds-to-body pattern, following a node with a camera, prefab children and
  `spawn_under`, costs, memory, misuse, pitfalls: centred pivots for mirrors, node copies without links); header
  comments (allocates? per-frame safe? misuse?). Covers VIII, AC-9.
- [ ] **T11.2** Other pages: `docs/api/math.md` (link to transforms), `entities.md` (the engine's one component kind,
  cascades), `prefabs.md` (children, `child()`, `max_children`, `spawn_under`, `place(&Node::local)`), `gfx.md` (drawing
  a node), `camera.md` (following a child), `docs/guides/levels.md` (map objects spawn hierarchies),
  `docs/architecture/patterns.md` (the row: implemented), `scenes.md` (the naming note), `docs/REFERENCES.md` (Godot,
  Unity, Bevy, flecs, EnTT, Bitsquid, *Game Programming Patterns*' Dirty Flag), `CLAUDE.md` (layout paragraph and the
  hierarchy script command), the `nnh-architecture` skill's pattern list; `python -I scripts/docs_tool.py check` → 0
  broken links. Covers AC-9.
- [ ] **T11.3** `docs/architecture/decisions.md`: D37 change-log note "implemented" with the stage refinements; D35 and
  D36 notes confirmed. Covers AC-9.
- [ ] **T11.4** Final measurements (header costs, build times, `hello` sizes, `.text` growth) in plan.md and
  `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T11.5** `docs/ROADMAP.md`: the "Scene tree and transforms" checkbox (the title kept, a note: hierarchy, nodes and
  transforms), with the implemented summary.
- [ ] **T11.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
