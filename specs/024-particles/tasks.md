# Tasks 024: Particles

Status: approved 2026-11-24 (overnight pre-approval)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script). The work happens in a **separate git worktree** (branch `spec-024`) created from `main` **after spec 022 is
merged complete**; specs 023 and 025 are built in parallel worktrees, so follow plan.md's "Merge notes" for every
shared file (insertions at the stated anchors only; `examples/platformer/tests/level.nnhs` is **never** edited). The
shared helpers move first with their bits unchanged; the simulation is pure and tested on the fake platform; drawing is
proven against `DrawList::build()`; the golden script, presets (with the D29 amendment) and fuzz follow; the platformer
gains cosmetic effects with its replay lines untouched; the example and the ARM pre-check (T7.6) come before the first
console run. Stage 9 is run on the Linux build VM by the lead; the only console stage is stage 10: five scripted runs
the maintainer only starts (Homebrew Launcher → Y). Deviations found while building go into plan.md as "Stage N
refinements", as in plans 012–025.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release without a warning;
`hello` sizes and `nm` equal T0.1's (until stage 4; then within T4.6's budget, no particle symbol); ctest
`platformer_replay_matches_script` with the lines in the tree and `git diff --exit-code examples/platformer/tests`;
`camera: PASS 5201E6CF`, `queries: PASS 4ACAA177`, `entities: PASS C8FB9A41`, `prefabs: PASS 7BB7901E`, spec 022's
hierarchy golden, `tweens: PASS B44CA3F6`, `data check: PASS checksum 0xD5BD3020` (their ctests).

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-024/`, not committed): `hello.exe`
  (pc-release, pc-debug) and `hello.elf`/`.3dsx` (3ds-release, 3ds-dev) sizes (`size`: text/data/bss, file bytes) and
  `nm -C` lists; `.text` of the 3ds-release `sprites`, `platformer`, `data`, `camera` ELFs; edit
  `examples/prefabs/room.cpp` → simulator and `.3dsx` times (the reference for the particles example's edit); clean
  `nnh_core`; preprocessed lines and `-fsyntax-only` time of one-line TUs including `<nnh/gfx.hpp>` + `<nnh/transform.hpp>`
  + `<nnh/time.hpp>` + `<nnh/result.hpp>`; a data-only rebuild of `examples/data` (one asset edited) timed; 3ds-dev
  disassembly (`fcount.py`) of `DrawList::build`, `gfx_frame.o`, `draw2d.o`, `camera_shake.o`, `draw2d_3ds.o` saved in
  `build/ref-024/dis0/`. Files: plan.md ("Stage 0 results"). Covers III, FR-26, AC-8.
- [x] **T0.2** [P] Reference screenshots for AC-8 in `build/ref-024/ref/`: `hello`, `sprites` (world and
  `SPRITES_DEMO=bench`), `scenes`, `camera` (`CAMERA_DEMO=zoom1`), `queries` (`QUERIES_DEMO=all`), `entities`, `prefabs`,
  `hierarchy` (showcase), platformer `PLATFORMER_DEMO=bench` (top), level 1 at 90, `PLATFORMER_DEMO=replay` at 64 and
  312; `build/ref-024/compare.sh` retakes them into `now/`. (Level 1 and the replay are expected to change in stage 6:
  dust and puffs; new references are taken there.) Files: plan.md. Covers AC-8.
- [x] **T0.3** [P] Goldens and counts today: the replay ctest's last line as in the tree (spec 022's), `5201E6CF`,
  `4ACAA177`, `C8FB9A41`, `7BB7901E`, the hierarchy golden, `B44CA3F6`, `0xD5BD3020`; `nnh_tests` count against
  `kMaxTests` (this spec adds ≈ 300 tests: raise only if fewer than 400 are left, and note it for the merge). Files:
  plan.md. Covers VI.

## Stage 1: shared helpers and the emitter skeleton (pure)

- [x] **T1.1** Move the camera shake's finalizer to `engine/core/src/core/mix32.hpp` (inline `detail::mix32`, the same
  body); `camera_shake.cpp` uses it. Test: `tests/camera/test_mix32.cpp` (known values of the finalizer); spec 018's
  tests and `camera: PASS 5201E6CF`; 3ds-dev `camera_shake.o` identical to T0.1's. Covers FR-15.
- [x] **T1.2** [P] `engine/core/src/gfx/sprite_place.hpp`: `snap`, `place_sprite`, `place_rect` (the code of
  `draw_list.cpp`'s `sprite_item`/`shape_item` arithmetic, inline); `draw_list.cpp` uses them. Test:
  `tests/gfx/test_sprite_place.cpp` (items for zoom 0.5/1/2, flips by negative scale, pivots, rounding of halves);
  `tests/gfx/test_draw_list.cpp` unchanged and passing; 3ds-dev `draw_list.o` byte-identical to T0.1's. Covers FR-19.
- [x] **T1.3** The header and the description: `<nnh/particles.hpp>` (limits, enums, `ParticleRange`, `ParticleTimeRange`,
  `ParticleCount`, `ParticleBurst`, keys, `ParticleFrame`, `ParticleDesc`, `ParticleDrawOptions`, `ParticleStats`,
  `ParticleView`, `ParticleEmitter` declarations), `particle_desc_check.{hpp,cpp}` (every check of plan.md, the first bad
  field named), `ParticleEmitter::check`. Test: `tests/particles/test_desc_check.cpp` (the default valid; each limit both
  ways; unused-then-used keys and frames; unsorted keys; NaN anywhere; messages). Files: those, `engine/core/CMakeLists.txt`
  (the `_nnh_particles_sources` block after the hierarchy block, `-ffp-contract=off`), `tests/CMakeLists.txt` (024's block
  after the `entities_example` line). Covers FR-2, FR-3, FR-25.
- [x] **T1.4** Blocks: `particle_block.hpp` (header, hot 32 B and cold 16 B records `static_assert`ed, 32-byte
  alignment), `particle_derived.{hpp,cpp}` (per-frame units, segments, bucket tables, frame constants, variant flags),
  `emitter_create.cpp` (create, move, destroy, the `particles` tag, `capacity()`, `valid()`, `desc()`). Test:
  `tests/particles/test_create.cpp` (capacities 1/8192 accepted, 0/8193 refused; `OutOfMemory`; tag bytes = header +
  48 × capacity; a game tag; move leaves the source empty; empty emitters answer zero), `particles_fixture.hpp`. Covers
  FR-1, FR-24.
- [x] **T1.5** [P] The generator: `particle_random.hpp` (`base = mix(seed ^ mix(n))`, values by property index, floats
  in [0, 1), integer ranges by `umull`). Test: `tests/particles/test_random.cpp` (part 1: known values, uniformity of
  10 000 values in 16 bins within a fixed bound, properties independent, integer ranges hit both ends). Covers FR-15.
- [ ] **T1.6** Stage end: the stage-end guards; include check (`<nnh/particles.hpp>` pulls only its six includes and
  what they include; preprocessed lines and `-fsyntax-only` within 0.05 s of T0.1's set; no inline float arithmetic).
  Files: plan.md ("Stage 1 results"). Covers FR-26, III.

## Stage 2: emission (pure)

- [x] **T2.1** Shapes and directions: `particle_shapes.{hpp,cpp}` (point; box area and edge by perimeter; disc by
  `max(u₁, u₂)`; ring by the fixed mixture; arc; line; cone and outward directions; the Newton reciprocal square root;
  the per-emitter sampler pointer). Test: `tests/particles/test_shapes.cpp` (points inside / on the edge / in the ring
  / on the arc for 10 000 births; area uniformity by equal-area annuli and box quadrants, perimeter uniformity of edges,
  within fixed bounds), `tests/particles/test_directions.cpp` (cone limits, 2π spread, outward unit vectors within
  2⁻²⁰, the centre and point fallbacks, negative speeds). Covers FR-8, FR-9.
- [x] **T2.2** Births and on-demand emission: `emitter_emit.cpp` (`birth()` in property order, the birth map and its
  pure-translation skip, inherit velocity, rotation by the mirror rule, lifetime ticks, `1/lifetime`, start colour and
  size, the Random frame byte; `emit`, `emit_at(point)`, `emit_at(transform)`; the emission number; the full case:
  dropped and counted, one Debug/Development warning per emitter). Test: `tests/particles/test_order_full.cpp` (part 1:
  as many as fit, the rest dropped and counted, no assert, the warning once and not in Release),
  `tests/particles/test_random.cpp` (part 2: the same particles whatever the capacity — drops never shift later values;
  another emitter's calls change nothing). Covers FR-6, FR-15, FR-18.
- [x] **T2.3** The clock: `emitter_clock.cpp` (delay, `clock += 4096`, duration, looping, the 16.16 rate accumulator,
  bursts with cycles and intervals, hashed counts, `start`/`stop`/`emitting`, emission over, `finished()`). Test:
  `tests/particles/test_rate.cpp` (2.5/s → 25 in 600 frames and 150 in 3600 exactly; 0.5/s, 59.9/s, 491 520/s; delay;
  duration; looping), `tests/particles/test_bursts.cpp` (times, 1/4/endless cycles, intervals, count ranges, 8 bursts, a
  one-shot finishing). Covers FR-4, FR-5, FR-7.
- [x] **T2.4** Sub-frame spreading: rate births at their frame fraction on the interpolated transform (origin and
  drawing values, one sine only when the rotation's bits changed, `f = 1` the current map), pre-aged by `1 − f`;
  `set_transform`/`set_position` with `jump`; `emitter_transform.cpp`. Test: `tests/particles/test_subframe.cpp` (a fast
  straight emitter's births evenly spaced, a turning one along the arc, pre-ages, `jump` breaks it, local space not
  spread, bursts not spread). Covers FR-10, FR-14.
- [ ] **T2.5** Stage end: the stage-end guards. Files: plan.md ("Stage 2 results"). Covers AC-8.

## Stage 3: life, appearance, lifecycle (pure)

- [x] **T3.1** Motion: `emitter_update.cpp` (`update()`'s order: clock, motion, rate births, bursts, previous transform;
  the `<spin, drag>` variants; `age += 4096` and death; stable compaction with cold moves; the velocity flush; bounds by
  integer keys; gravity mapped once per update). Test: `tests/particles/test_motion.cpp` (the order against a hand loop;
  px/s and px/s²; exact lifetimes in updates for `Frames{24}` and `Duration`s; the flush; no allocation),
  `tests/particles/test_order_full.cpp` (part 2: emission order through random deaths). Covers FR-3, FR-11, FR-17, FR-18.
- [x] **T3.2** Appearance: `particle_appearance.hpp` (`t`, segments through the bucket table, rounded channels, `mul8`,
  size; the branch-free frame rule with its reciprocal). Test: `tests/particles/test_appearance.cpp` (keys exact, between
  keys within 1/256 of a `double` reference for flat and steep gradients, 0/1 key constant, `mul8` exhaustive over 256²),
  `tests/particles/test_frames.cpp` (first, random uniform, over life with repeats, at a rate; the reciprocal rule
  exhaustive over m < 2²⁴ for n = 1..16). Covers FR-12, FR-13.
- [x] **T3.3** Spaces: world and local storage, gravity world/local, the local drawing values (equal to
  `gfx::transformed`). Test: `tests/particles/test_space.cpp` (world trail stays behind; local particles follow a moving,
  rotated and mirrored transform; `world_gravity` both ways; the drawing values equal `gfx::transformed`'s for 1000
  seeded transforms). Covers FR-14.
- [x] **T3.4** Lifecycle and prewarm: `clear`, `restart` (clock, accumulator, bursts, emission number, previous transform,
  prewarm), prewarm at creation, `set_desc` (tables rebuilt, live particles kept, a bad one asserts and changes nothing),
  `set_seed`. Test: `tests/particles/test_lifecycle.cpp` (stop lets particles finish; clear; restart reproduces the first
  run bit for bit; `emit`/`emit_at` while stopped), `tests/particles/test_prewarm.cpp` (prewarm = the same updates by
  hand; 600 accepted, 601 refused), `tests/particles/test_set_desc.cpp`. Covers FR-2, FR-6, FR-7.
- [x] **T3.5** [P] Queries and binding: `emitter_queries.cpp` (`particle()` views, `stats()`, `alive()`, `bounds()`),
  `emitter_scene.cpp` (D30). Test: `tests/particles/test_queries.cpp`, `tests/particles/test_scene_binding.cpp` (updated
  before the scene's update, frozen under a pause scene, cleared after exit, moved while bound, destroyed while bound,
  `bind` twice ignored). Covers FR-17, FR-22.
- [x] **T3.6** Misuse and cost: every FR-25 case of plan.md asserts and is harmless on the Release path (pc-release);
  allocation counter unchanged over 100 000 operations after creation; no log in the normal path; `events::stats()`
  unchanged. Test: `tests/particles/test_particle_misuse.cpp`, `tests/particles/test_particle_cost.cpp`. Covers FR-23,
  FR-24, FR-25.
- [ ] **T3.7** Stage end: the stage-end guards. Files: plan.md ("Stage 3 results"). Covers AC-8.

## Stage 4: drawing

- [x] **T4.1** gfx's particle batches: `gfx/particle_batches.{hpp,cpp}` (records, the 32-batch table, the 128-item
  scratch, the `max_particles` budget, `gfx_record_particles`, `gfx_forget_particles`, the submit loop with chunks, the
  ops and their registrar); `GfxConfig::max_particles` (`app.hpp`), `gfx::Stats`' three fields (`gfx.hpp`), the splits
  array to 96 (`gfx_state.hpp`), the dispatch and null-checked calls in `gfx_frame.cpp` and `draw2d.cpp`, the
  `tile_batch` comment (`draw_list.hpp`). Test: `tests/particles/test_particle_budget.cpp` (part 1: no memory when 0;
  ≈ 8.2 KB when > 0; a fake expand function's items reach `draw_2d` in chunks between the pass's other calls; 33
  batches; budget drops identical however the batches are cut). Covers FR-19, FR-20.
- [x] **T4.2** The expansion: `particle_expand.cpp` (`<textured, local>` variants; the view; frames resolved against the
  texture; reach; the emitter and per-particle culling by integer keys; draw order; appearance; items through
  `sprite_place.hpp`; drawn/culled stats), `emitter_draw.cpp` (`draw()` overloads, outside a frame, `max_particles == 0`).
  Test: `tests/particles/test_expand.cpp` (items equal `DrawList::build`'s for the equivalent `gfx::draw` /
  `gfx::fill_rect` commands at zoom 0.5/1/2, `ignore_camera`, rotation, scale, flips, tint × opacity, every frame mode,
  whole texture and pivots, local mirrored emitters; whole-emitter and per-particle culling at exact boundaries; newest
  first). Covers FR-13, FR-14, FR-19.
- [x] **T4.3** Through the frame: Test: `tests/particles/test_draw.cpp` (one command per draw; sorted with sprites and
  tile batches by layer and call order, read from the fake's calls; read at submit (an update after the draw call
  shows); destroyed after its draw call (forgotten), moved (still drawn); destroyed texture (assert, skipped); both
  screens; drawn twice), `tests/particles/test_particle_budget.cpp` (part 2: `max_particles` 0 asserts with the fix,
  Release warns once; over the cap asserts, Release drops and warns once per frame; the stats; the overlay line only
  when the cap > 0). Files: `debug/overlay.cpp`. Covers FR-19, FR-20.
- [x] **T4.4** [P] 3DS: `app_3ds.cpp` sizes `C2D_Init` for `max_particles` more objects. Test: 3ds-release and 3ds-dev
  build without a warning; `examples/sprites` and the platformer's bench draw the same screenshots on PC (the gfx path
  with `max_particles` 0 is byte-identical: `compare.sh`). Covers FR-20, AC-8.
- [ ] **T4.5** Stage end: the stage-end guards; `compare.sh` (every reference identical: with `max_particles` 0 the
  draw lists are the same bytes). Files: plan.md ("Stage 4 results"). Covers AC-8.
- [ ] **T4.6** `hello` budget: sizes against T0.1 (`.text` ≤ +128 B on 3ds-release, ≤ +256 B on pc-release; `.data`
  unchanged; `.bss` ≈ +260 B), `nm` shows only `g_gfx_particle_ops` as new; the numbers in plan.md. If over budget, move
  the dispatch out of line behind the null check. Covers FR-26, AC-8.

## Stage 5: the golden script, presets, fuzz

- [ ] **T5.1** The self-check (AC-2): `examples/particles/self_check.{hpp,cpp}` (the 8 emitters of plan.md, 3600
  frames, the table and FNV-1a choices, checkpoints every 600 frames over every live particle's view and the counters);
  goldens from pc-debug in `kParticlesCheckGolden`/`kParticlesCheckpoints`, equal in pc-release. Test:
  `tests/particles_example/test_self_check.cpp` (on the fake platform; a one-value change in the script fails it —
  checked once by hand). Files: those, `tests/CMakeLists.txt` (024's block). Covers AC-2, FR-16.
- [ ] **T5.2** D29 amendment, part 1 (core): `FieldOptions::ascending`/`up_to` (`data.hpp`), `FieldInfo`'s two bools,
  `data_tables.hpp`, `schema_write.cpp` (tokens only when set), `schema_check.cpp` (valid places), `data_validate.cpp`
  (ascending → `Corrupted`), `data_apply.cpp` (`up_to` arrays: written items only). Test:
  `tests/data_assets/test_ascending.cpp`, `tests/data_assets/test_up_to.cpp`; every existing data test and fixture
  unchanged; the description text of `examples/data`'s schema byte-identical to T0.1's (fingerprints unchanged). Covers
  FR-21, X.
- [ ] **T5.3** D29 amendment, part 2 (converter): `tools/assetc/src/schema_read.cpp` (the tokens),
  `data_values.cpp`/`data_writer.cpp` (`file:line` messages; short `up_to` arrays with their count). Test:
  `tools/assetc/tests/` (schema read, conversion, the fuzz target still clean), fixtures
  `tests/data/data_assets/bad/not_ascending.enemy` and `too_many_up_to.enemy` with their messages (ctests
  `data_convert_rejects_*`); `data check: PASS checksum 0xD5BD3020 (… index 595 bytes)`. Covers FR-21, AC-3.
- [ ] **T5.4** The preset schema: `engine/core/schemas/particles_schema.cpp`, `NNH_PARTICLES_SCHEMA` in
  `cmake/nnh_data.cmake`; a test schema program writes `tests/data/particles/schema.nnhschema` (ctest compares it to the
  committed copy); fixtures `tests/data/particles/good/sparks.particles` (E6's description) and `bad/{unsorted_keys,
  too_many_keys,out_of_range,unknown_shape}.particles` with messages; ctests `particles_preset_accepts_good`,
  `particles_preset_rejects_<rule>`, `particles_preset_explains_<rule>`. Test: `tests/particles/test_presets.cpp` (the
  converted good preset loads equal, field for field, to the C++ description; AC-2's script with E6 from the preset
  gives the same checksum; `check()` refuses a hand-damaged loaded description). Covers FR-21, AC-3.
- [ ] **T5.5** [P] Fuzz: `tests/fuzz/fuzz_particle_ops.cpp` (bytes → a description (often invalid: `create` errors) and
  a stream of updates, emissions with any floats, transforms, `set_desc`, restarts, stops, clears and expansions into a
  scratch with any camera and zoom; checks: alive ≤ capacity, bounds contain positions, no UB), `libfuzzer_particle_ops.cpp`,
  `tests/fuzz/test_fuzz_particle_ops.cpp` (mini-fuzz in `nnh_tests`). Files: `tests/CMakeLists.txt` (024's block).
  Covers X, FR-25.
- [ ] **T5.6** Stage end: the stage-end guards; `compare.sh`. Files: plan.md ("Stage 5 results": golden, checkpoints,
  the schema text, the preset messages). Covers AC-2, AC-3, AC-8.

## Stage 6: the platformer's effects (replay lines untouched)

- [ ] **T6.1** `examples/platformer/level_effects.{hpp,cpp}`: dust (landing, starting to run), puff (a bullet stopped
  on a wall, a hit), flash (a turret firing, at spec 022's muzzle point along the barrel), texture-less squares on
  `kPlayer`, fixed seeds; `update(const EffectsInput&)` reads only; `draw()`; `clear()`. Test:
  `tests/platformer/test_level_effects.cpp` (each trigger emits the planned count at the planned place; a frame without
  a trigger emits nothing; the bullet pool, turrets, world and hierarchy are byte-identical before and after
  `update()`). Files: those, `tests/CMakeLists.txt` (024's block). Covers AC-5.
- [ ] **T6.2** The hooks: `level_play.hpp` (include after `sparkles.hpp`, `LevelEffects effects_;` after `sparkles_`),
  `level_play.cpp` (after `camera_.apply();` in `update()`, after `turrets_.restart();`, after `draw_sparkles();`),
  `main.cpp` (`.max_particles = 256`), `CMakeLists.txt` (`level_effects.cpp` after `level_camera.cpp`). Test: ctest
  `platformer_replay_matches_script` passes with the lines in the tree; `nnh run platformer --pc --no-build --script
  examples/platformer/tests/level.nnhs` → PASS; `git diff --exit-code examples/platformer/tests`; new references for
  level 1 at 90 and the replay at 64 and 312 (dust and puffs visible: read back), the bench screenshot identical.
  Files: plan.md ("Stage 6 results"). Covers AC-5.
- [ ] **T6.3** Stage end: the stage-end guards; `gate.nnhs --pc`, `zoom_gate.nnhs --pc` PASS. Covers AC-5, AC-8.

## Stage 7: `examples/particles` (PC), the bench and the ARM pre-check

- [ ] **T7.1** Art, presets and the gallery: `art/make_art.py` → `assets/art/particles.png`, `assets/fx/embers.particles`
  and `sparks.particles`, `effects.{hpp,cpp}` (the gallery's descriptions), `gallery.{hpp,cpp}` (torch, smoke, rain and
  the code-built tile band, explosion, squares, embers), `ship.{hpp,cpp}`, `hud.{hpp,cpp}`, `showcase_scene.{hpp,cpp}`,
  `main.cpp`, `demos.hpp`, `CMakeLists.txt` (`DATA_SCHEMA ${NNH_PARTICLES_SCHEMA}`, `ASSETS assets`),
  `examples/CMakeLists.txt` (after `prefabs`). Test: PC screenshots `PARTICLES_SHOT=gallery` and `trail` read back.
  Covers AC-4, FR-21.
- [ ] **T7.2** The bottom screen and controls: `touch_panel.{hpp,cpp}` (touch bursts with `emit_at` on the selected
  effect's bottom emitter, L/R selection, the stats and `gfx::stats()` lines), A/X/B/D-Pad Down, the log lines of
  plan.md (`burst:`, `done:`, `effect:`, `explosion:`, `space:`, `layer:`, `smoke:`); `PARTICLES_SHOT` modes. Test: PC
  screenshots `explosion`, `touch`, `local`, `world`, `full` read back; the lines in a `--pc` run. Covers AC-4, FR-6,
  FR-14, FR-18, FR-22.
- [ ] **T7.3** Check scene: SELECT (or `PARTICLES_DEMO=check`) runs T5.1's script at 10 script frames per display
  frame, logs `particles: check started`, `particles: <frame> sum <checksum>` every 600 frames, `particles: PASS
  <checksum>` or `particles: FAIL frame <f> expected <x> got <y>`, draws the last frame and logs `particles: stats drawn
  <n> of <max> dropped <d>`; SELECT back to the showcase; ctest `particles_self_check_pc` (after the
  `entities_self_check_pc` block). Files: `check_scene.{hpp,cpp}`, `tests/CMakeLists.txt`. Covers AC-2, AC-6.
- [ ] **T7.4** [P] Bench: `bench.{hpp,cpp}` (`update1024`, `update16x64`, `emit512`, `draw512`, `sprite512`, `frame`,
  `offscreen`, a base run) and `bench_scene.{hpp,cpp}` (300 frames each (`frame` 600), the scene's scope + `gfx submit`
  − base, the lines and the verdict; Debug `not judged (Debug)`); Y and `PARTICLES_DEMO=bench`. Uses plan 023's `gfx
  submit` scope: if 023 isn't merged yet, copy its `gfx_frame.cpp` hunk byte for byte (plan.md "Merge notes"). Test: a
  pc-dev run logs every line. Covers AC-7.
- [ ] **T7.5** The script `examples/particles/tests/check.nnhs`: SELECT → every checkpoint, the PASS line and the stats
  line; SELECT → `particles: showcase`; a touch at (120, 64) → `burst: sparks 16 at 120,64 live 16`, then `done: sparks
  live 0 finished`; R → `effect: smoke` and a touch; A → `explosion:`; X twice → `space: local`, `space: world`; B →
  `layer: front`; D-Pad Down → `smoke: stopped`; screenshots at fixed waits; a `wait 3` after every `expect` that an input
  follows; Y → the bench lines and `particles bench: PASS`; `fail-on "particles: FAIL"`, `fail-on "particles bench:
  FAIL"`, `fail-on "] E "`; ctest `particles_check_script_pc` (`RESOURCE_LOCK nnh_remote_port`). Test: `nnh run
  particles --pc --no-build --script examples/particles/tests/check.nnhs` → PASS, twice. Files: plan.md ("Stage 7
  results": every expected line). Covers AC-4, AC-6.
- [ ] **T7.6** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects
  `src/particles/*`, `src/gfx/particle_batches.o`, `draw_list.o`, `gfx_frame.o`, `draw2d.o`, `src/camera/camera_shake.o`,
  `draw2d_3ds.o` and the example's `bench.o`; per AC-7 hot path record instructions, calls, stack reloads, `pld`, `vmrs`,
  `vdiv`, `__aeabi_*`, `memcpy`/`memset`, `floorf`, constant-multiply chains: the update step per variant (≈ 45
  instructions, one `pld`, no call), the birth (≈ 300, `sin_cos` inline, one indirect call), the expansion per particle
  (≈ 180–220, two `floorf`), the submit loop per chunk, `DrawList::build` and `camera_shake.o` unchanged; the miss model
  per workload from the working sets; compare with plan.md's table and fix what differs without changing a result
  (every golden holds). Files: plan.md ("Stage 7 ARM pre-check": the table and the expected console numbers). Covers
  AC-7, II.

## Stage 8: mutation testing, opt-in checks

- [ ] **T8.1** Mutation pass as in plans 019–025: hand mutants (`build/ref-024/mutants.py`, applied one at a time by
  `mutate.py`, the whole `nnh_tests` each) of `src/particles/*` (the update order, the flush, compaction, the accumulator,
  bursts, `finished()`, the sampler densities, the outward normalisation, the mirror rule, segments and `mul8`, the frame
  rule, culling, the budget), `src/gfx/particle_batches.cpp`, `sprite_place.hpp`, the D29 rules and the platformer's
  effects; Release-path mutants in pc-release; survivors become tests or are argued equivalent. Files: plan.md
  ("Mutation testing"). Covers VI.
- [ ] **T8.2** [P] Release strings: `scripts/check_release_strings.sh` on the 3ds-release and pc-release `particles`
  executables (the drop warning and misuse texts are Debug/Development only). Covers FR-18, FR-25.
- [ ] **T8.3** [P] Opt-in by linking: probes (`build/ref-024/probe_*`, not committed) linked against the 3ds-release
  libraries: a game that only updates emitters links no data code and no `particle_expand` code; a game that draws
  them has `g_gfx_particle_ops` set at `init()` (the probe logs it); a particles game without the preset schema has no
  `data::` symbol (the 3DS map file); `hello` has no particle symbol and sizes within T4.6's budget. Files: plan.md
  ("Stage 8 results"). Covers FR-26, AC-8.
- [ ] **T8.4** Regression and build times: `compare.sh` (every T0.2 reference identical, the platformer's new ones from
  T6.2); every example's console script passes with `--pc`; `.text` of `sprites`, `platformer`, `data`, `camera`,
  `particles` recorded; edit `examples/particles/gallery.cpp` → simulator and `.3dsx` times against T0.1's and constitution
  III; edit `assets/fx/embers.particles` → a data-only rebuild (no C++ compiled; time recorded). Files: plan.md ("Stage 8
  results"). Covers AC-8, III.

## Stage 9: Linux checks (VM 120, run by the lead)

- [ ] **T9.1** On `nnhge-build`: GCC 13, GCC 14 and clang 18 (ASan/UBSan): `nnh_tests` and ctest pass; AC-2's golden
  and checkpoints, the expansion-equals-`build` tests, the preset tests, the D29 fixtures, `0xD5BD3020`, `5201E6CF`, the
  replay ctest with the tree's lines identical on all three; ASan/UBSan clean over compaction, the scratch chunks,
  forgotten batches, moved and destroyed emitters, scene binding and the misuse Release paths; libFuzzer
  `fuzz_particle_ops` 10 minutes and the data fuzz targets 5 minutes each, clean. Files: tasks.md (results), plan.md
  ("Stage 9 results"). Covers I, X, FR-16, AC-1, AC-2.

## Stage 10: console check (the only steps on the 3DS)

- [ ] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh
  cmake --build --preset 3ds-dev --target particles_3dsx platformer_3dsx data_3dsx camera_3dsx` without a warning;
  T7.6's table still true; `hello.elf` within T4.6's budget; plan.md's "Stage 10: console run (prepared)" completed with
  the recorded golden, checkpoints, showcase lines and the platformer's lines in the tree. Covers AC-8, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Five runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run particles --script examples/particles/tests/check.nnhs` → every `particles:
     <frame> sum` line and `particles: PASS` equal the PC's, the stats line, every showcase line, the bench lines and the
     verdict (numbers recorded); screenshots read back;
  2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the tree's replay
     lines, `60 Hz PASS`;
  3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` → `bench: PASS`;
  4. `build/pc-debug/tools/nnh/nnh run data --script examples/data/tests/check.nnhs` → `data check: PASS checksum
     0xD5BD3020 (4 enemies, 3 items, 1 settings, index 595 bytes)`;
  5. `build/pc-debug/tools/nnh/nnh run camera --script examples/camera/tests/check.nnhs` → `camera: PASS 5201E6CF`.
  Files: plan.md ("Console run 1": verdicts, bench numbers, log paths). Covers AC-5, AC-6, AC-7.
- [ ] **T10.3** (only if a bench target is missed) Console bench refinement: find the cost in the 3ds-dev disassembly,
  apply plan risk 1's list (and risk 2's tint-mode tracking if `draw512` shows flushes) without changing a result
  (every golden holds; tests first for any behaviour-preserving rewrite); then T10.2 run 1 again; if still missed, the
  budget is set from the measurement with ≈ 25 % margin, written in the spec's AC-7, `bench_scene.cpp` and
  `docs/api/particles.md` with the numbers. Files: plan.md ("Optimisation round", "Console run 2 and budgets set").
  Covers AC-7.

## Stage 11: docs, roadmap, status

- [ ] **T11.1** (the Old 3DS costs added after T10.2) `docs/api/particles.md`: emitters and capacities, the description
  and its units, emission (rate, bursts, one-shots, on demand), shapes and directions, motion and damping, colour, size
  and frames over life, local and world space, following an entity or a node (copy its world transform after the pass),
  sub-frame spreading and `jump`, determinism and seeds, update order and scene binding, drawing, layers, culling,
  `max_particles`, the full case, presets (the schema line in `DATA_SCHEMA`, the ticks table, the key rules), stats,
  costs, memory, misuse, pitfalls (squares don't rotate; switching space needs a restart; bound emitters follow the
  previous transform); header comments (allocates? per-frame safe? misuse?). Covers VIII, AC-9.
- [ ] **T11.2** Other pages: `docs/api/gfx.md` (capacities, stats, particle batches; after the tile section), `app.md`
  (`max_particles`), `docs/api/data.md` (`.ascending`, `.up_to`, the engine's preset schema), `transform.md`/`hierarchy.md`
  (the follow-a-node pattern), `animation.md` (frames from a sprite library), `docs/architecture/rendering.md`
  ("Particle batches (D39)" after "Zoom (D33)"), `project-structure.md` (`engine/core/schemas/`), `patterns.md` (the row:
  implemented), `docs/REFERENCES.md` (Godot, Unity, LÖVE, Phaser, Nystrom), `CLAUDE.md` (layout paragraph after
  "Sprite animation", commands after the `nnh run data` line, `particles/` in the layout); `python -I
  scripts/docs_tool.py check` → 0 broken links. Covers AC-9.
- [ ] **T11.3** `docs/architecture/decisions.md`: D39 change-log note "implemented" with the stage refinements; the D29
  amendment note confirmed. Covers AC-9.
- [ ] **T11.4** Final measurements (header costs, build times, data-only rebuild, `hello` sizes, `.text` growth) in
  plan.md and `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [ ] **T11.5** `docs/ROADMAP.md`: the "Particles" checkbox, with the implemented summary.
- [ ] **T11.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
- [ ] **T11.7** (the lead, after 022 and whichever of 023/025 are done) Merge `spec-024` into `main` per plan.md's
  "Merge notes"; then the full ctest (pc-debug, pc-release), every example's `--pc` script, AC-2's golden, 022's, 023's
  and 025's goldens and the replay ctest with the tree's lines, one 3ds-release build; T10.2 run 2 is repeated if a merge
  happened after it.
