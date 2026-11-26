# Tasks 017: Input actions

Status: implemented: T7.2 passed on the Old 3DS XL and the Linux VM / libFuzzer parts ran on VM 120 (2026-11-22);
only the timings of T0.1/T8.4 were not measured
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

VM 120 results (2026-11-22, on the branch before the merge): GCC 13 and 14 failed to compile
`test_action_declare.cpp`'s one-line callers (`add(Act::Jump, {.name = ...})` → "cannot convert brace-enclosed
initializer list"; the same braces inside test bodies compile); fixed by naming `nnh::ActionDesc{...}` there (5804d1e).
After that GCC 13/14 pass ctest (apart from spec 014's compile-message regexes, fixed in main since, and
`nnh_script_pc_smoke`, which needs a `pc-dev` build folder). clang 18 ASan/UBSan: `nnh_tests` 1540/1540 clean; the
example ctests pass with `detect_leaks=0` (SDL's offscreen EGL driver leaks). `fuzz_bindings` 2 minutes: cov 436, no
crash. After the merge (main): GCC 13 ctest 256/256 (one `-Wformat-truncation` in `examples/actions/main.cpp`, fixed),
clang ASan/UBSan ctest 256/256 and `nnh_tests` 1701/1701 with leak checks on, `fuzz_bindings` cov 471, no crash.

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug`, a screenshot, a `--pc`
script) and, at stages 2, 5 and 8, the same tests on the Linux build VM 120 (GCC, clang + sanitizers). The only console
step is T7.2: the maintainer opens the Homebrew Launcher, presses Y, and `nnh run actions --script
examples/actions/tests/check.nnhs` logs `actions: PASS` or `actions: FAIL …`. Deviations found while building go
into plan.md as "Stage N refinements", as in plans 012 and 015.

## Stage 0: baseline

- [x] **T0.1** Baseline numbers (Windows, 3 runs each): `hello.exe` and `hello.elf`/`.3dsx` Release sizes (`size`
  text/data/bss); edit `examples/hello/main.cpp` → simulator and `.3dsx` build times; clean engine build; preprocessed
  line count and compile time of a one-line TU including `<nnh/input.hpp>` (the future `actions.hpp` comparison).
  Files: plan.md ("Stage 0 results"). Covers III, FR-25, AC-10. Done: sizes recorded (plan "Stage 0 results"); build times and the header line count not measured yet.

## Stage 1: sources and combining (pure)

- [x] **T1.1** `InputView` (held mask, shaped sticks, 8 direction pushes, stick lengths, touch) and its builder from
  the public queries. Files: `engine/core/src/actions/input_view.{hpp,cpp}`, `engine/core/include/nnh/actions.hpp`
  (enums, `Source` and its factories, `Binding`). Test: `tests/actions/test_input_view.cpp` (pushes per direction,
  lengths, C-Stick zero without the capability). Covers FR-6, FR-10.
- [x] **T1.2** One slot's evaluation for every source kind: button, stick direction (press/release hysteresis, value =
  push), whole stick and stick axis (extra dead zone re-scaled), 2- and 4-button composites (opposites cancel, diagonal
  0.70710678f), touch area (slide in/out, edges), invert x/y. Files: `actions/source_eval.cpp`,
  `actions/actions_internal.hpp`, `engine/core/CMakeLists.txt` (sources, `-ffp-contract=off` on `src/actions/*`).
  Test: `tests/actions/test_source_eval.cpp`. Covers FR-3, FR-6, FR-22.
- [x] **T1.3** [P] Combining (any-held/max for buttons, largest `|value|` for axes, longest vector, ties → lowest
  slot) and the axis/vector held hysteresis. Files: `actions/action_combine.cpp`. Test:
  `tests/actions/test_action_combine.cpp`. Covers FR-3, FR-7.
- [x] **T1.4** [P] Display names: `control_name`, `describe(Source)` (composites, "D-Pad", "L/R"), truncation.
  Files: `actions/source_names.cpp`. Test: `tests/actions/test_source_names.cpp`. Covers FR-19.

## Stage 2: the set: declarations, contexts, queries (pure; driven by `evaluate(view)`)

- [x] **T2.1** Block layout and the set's life: `ActionBlock`/`ActionRec`/`SlotRec` with `static_assert` bounds
  (`ActionRec` ≤ 160 B on 32-bit), one `mem::alloc` (tag `actions`), move, destroy, `valid()`, set ids, log channel
  `actions`. Files: `actions/action_block.hpp`, `actions/action_set.cpp`. Test: `tests/actions/test_action_set.cpp`
  (tag stats, one allocation, move keeps state, destroy frees, empty set). Covers FR-1, FR-23.
- [x] **T2.2** Declaring: `add_context` (context 0 "default"), `add` with id/name/kind/context/bindings/repeat/
  thresholds; name rules, duplicates, FNV-1a collision (a colliding pair found offline and pinned), capacity overflow,
  source invalid for the kind, touch forced locked; Debug asserts, Release refusal + error log. Files:
  `actions/action_declare.cpp`. Test: `tests/actions/test_action_declare.cpp`. Covers FR-1, FR-2, FR-6, FR-8.
- [x] **T2.3** Evaluation core: per enabled action, slots → combine → edges; `pressed/held/released/value/vector`,
  `held_frames`, `frames_since_pressed` (saturation, "never"), held at creation not pressed. Files:
  `actions/action_eval.cpp`, `actions/action_queries.cpp`. Test: `tests/actions/test_action_queries.cpp`,
  `test_action_kinds.cpp`. Covers FR-3, FR-11, FR-23.
- [x] **T2.4** [P] Repeat: frame table for delay/interval, interval 0, `repeated` false while blocked/idle. Files:
  `action_eval.cpp`. Test: `tests/actions/test_action_repeat.cpp`. Covers FR-12.
- [x] **T2.5** Contexts by hand and blocking: enable/disable (from next evaluation), idle and silent when disabled, two
  contexts sharing a control, per-slot block on enable until that source is released (composites, sticks below
  release), another slot still works. Files: `actions/action_contexts.cpp`, `action_eval.cpp`. Test:
  `tests/actions/test_action_contexts.cpp`, `test_action_blocking.cpp`. Covers FR-4, FR-13.
- [x] **T2.6** [P] Old 3DS: capabilities read at creation; fallback slots, inactive ZL/ZR/C-Stick/touch sources,
  `usable()`, one Development warning when a context with an unusable action is enabled. Files:
  `actions/action_contexts.cpp`, `action_declare.cpp`. Test: `tests/actions/test_action_fallback.cpp` (fake
  `set_capabilities`, log capture). Covers FR-9.
- [ ] **T2.7** Stage end: `ctest --preset pc-debug`; the same on VM 120 (GCC 13/14, clang 18). Covers VI. Windows done; VM 120 (GCC 13/14, clang 18) not run yet.

## Stage 3: the frame hook (D32), events, scenes, input paths (fake platform)

- [x] **T3.1** `InputHook` list: inline `run_input_hooks()`, `add/remove_input_hook` (append order, no-op remove,
  double-add assert), head defined in `app.cpp`, call placed right after `input_publish_events()`. Files:
  `engine/core/src/core/input_hooks.{hpp,cpp}`, `engine/core/src/app/app.cpp`, `engine/core/CMakeLists.txt`. Test:
  `tests/app/test_input_hooks.cpp` (order, a hook sees this frame's `input::` values, runs before dispatch, no hooks →
  frame unchanged). Covers FR-15, D32.
- [x] **T3.2** Sets register/unregister their node; several sets in creation order; the `InputView` built once per pass
  and shared. Files: `action_set.cpp`, `action_eval.cpp`, `input_view.cpp`. Test: `tests/actions/test_action_hook.cpp`
  (two sets, destroy one mid-run, move a set). Covers FR-1, FR-15.
- [x] **T3.3** Events: `ActionPressed`/`ActionReleased`, presses then releases in declaration order, after the frame's
  button and touch events, axis/vector on held changes, none while disabled/blocked/listening, `set_events_enabled`,
  queries agree inside handlers, `is(set, id)`. Files: `actions/action_events.cpp`, `actions.hpp`. Test:
  `tests/actions/test_action_events.cpp`. Covers FR-14, FR-15.
- [x] **T3.4** Scene-bound contexts (needs spec 015's D30 merged): `bind_context`/`unbind_context`, enabled iff
  `scenes::top() == scene`, push menu → gameplay off/menu on from the next frame, pop restores, the confirm press that
  popped the menu doesn't reach gameplay, `AfterExit` ends the binding, rebind after re-push, set destroyed while bound
  asserts (Release: detaches), binding a scene not on the stack asserts. Files: `actions/action_contexts.cpp`. Test:
  `tests/actions/test_action_scenes.cpp`. Covers FR-5, FR-13, AC-3.
- [x] **T3.5** Opt-in check: `hello.exe` and `hello.elf` Release sizes against T0.1 (budget ≤ 16 B of text; no
  `actions` symbol: `nm`); `examples/input`, `scenes`, `platformer` build and their tests pass; spec 003 tests
  unchanged. Files: plan.md ("Stage 3 results"). Covers FR-25, AC-10.
- [x] **T3.6** Same sample, every input path: fake buttons/sticks/touch vs. spec 013 remote messages
  (`tests/remote/remote_fixture.hpp`) vs. D-Pad mirror on/off give the same action results; determinism: a recorded
  120-frame sequence (with a listen in the middle) run twice → identical per-frame log of queries, events and listen
  results. Files: tests only. Test: `tests/actions/test_action_input_paths.cpp`, `test_action_determinism.cpp`.
  Covers FR-10, FR-22, AC-2.

## Stage 4: remapping

- [x] **T4.1** Listen state machine: held-at-start ignored until released, capture order (buttons in bit order, then
  Circle Pad, then C-Stick directions), per-kind capture rules (button action: button or direction; axis whole: stick
  axis with invert on the negative side; vector whole: whole stick; parts: buttons, turning a non-composite slot into
  an empty composite), cancel mask, exclude mask, `sticks = false`, timeout, ZL/C-Stick not captured on fake Old 3DS,
  every action idle and silent while listening, blocking when it ends, `stop_listening`, result kept until the next
  listen/clear, listening a locked slot asserts. Files: `actions/action_listen.cpp`, `action_eval.cpp`. Test:
  `tests/actions/test_action_listen.cpp`. Covers FR-16, AC-4.
- [x] **T4.2** Conflicts: footprints (buttons + 8 stick-direction bits), same context only, swap (fits → swapped, else
  cleared), replace, allow, reject (names the holder), locked → rejected except allow, several conflicts, `conflicts()`.
  Files: `actions/action_conflicts.cpp`. Test: `tests/actions/test_action_conflicts.cpp`. Covers FR-17, AC-4.
- [x] **T4.3** [P] Editing: `set_source`, `set_part` (with the conflict policy), `clear`, `set_invert`, `reset` of slot,
  action, context and all; locked slots untouched; effect from the next evaluation; `source()`, `describe(action)` of
  active slots. Files: `actions/action_edit.cpp`, `source_names.cpp`. Test: `tests/actions/test_action_edit.cpp`.
  Covers FR-8, FR-18, FR-19.

## Stage 5: saving and loading

- [x] **T5.1** `NNHB` format: header/entry encode, structural validation (size, magic, format, reserved, count ≤ 256,
  CRC), decode into entries; writer diff (only changed remappable slots, part entry for a single changed composite
  part, whole entry otherwise, sorted, ≤ 3088 B). Files: `actions/bindings_format.{hpp,cpp}`. Test:
  `tests/actions/test_bindings_format.cpp` (byte-exact golden file of a known set). Covers FR-20.
- [x] **T5.2** `save_to`/`load_from`/`save`/`load`: atomic write through `nnh::save` with the set's file buffer;
  game version ignored; reset-then-apply; every skip reason counted (unknown hash, slot, part, kind, data range,
  flags, touch, locked, repeated key); one warning with counts; damaged (fake `corrupt_file`, truncated) and too-new
  files change nothing; missing file → `found = false`; enum reordered and actions added keep choices and get new
  defaults; ZL loaded on fake Old 3DS kept inactive and saved again. Files: `actions/bindings_io.cpp`. Test:
  `tests/actions/test_bindings_io.cpp`. Covers FR-20, FR-21, AC-5.
- [ ] **T5.3** Fuzz target `bindings_target`: validate as given, then with count/CRC fixed so mutations reach the
  entries; apply into a fixed set declared with every kind; must never assert or read out of bounds; seed = the golden
  file. Files: `tests/fuzz/fuzz_bindings.cpp`, `tests/fuzz/libfuzzer_bindings.cpp`, `tests/fuzz/fuzz_targets.hpp`,
  `tests/fuzz/test_fuzz_smoke.cpp`, `tests/CMakeLists.txt`. Test: mini-fuzz in `nnh_tests`; libFuzzer with
  ASan/UBSan on VM 120 (`NNH_BUILD_FUZZERS`, 10 minutes, no findings). Covers FR-21, X, AC-5. Mini-fuzz in `nnh_tests` passes; libFuzzer `fuzz_bindings` wired, not run yet (VM 120).
- [x] **T5.4** No allocation: after creation and declarations, evaluating, listening, remapping, resetting, saving
  and loading make no `operator new` call and no new `mem::alloc`; the leak report is clean after destruction. Files:
  tests only. Test: `tests/actions/test_action_alloc.cpp` (`tests/framework/alloc_counter`). Covers FR-1, FR-23, AC-6.
- [ ] **T5.5** Stage end: `ctest --preset pc-debug` on Windows and VM 120 (GCC, clang + ASan/UBSan). Windows done; VM 120 (GCC, clang + ASan/UBSan) not run yet.

## Stage 6: debug view and the example (PC)

- [x] **T6.1** `debug_draw(screen, x, y)`: enabled contexts and held actions by name with debug text in
  Debug/Development; nothing in Release. Files: `actions/action_debug.cpp`. Test: `tests/actions/test_action_debug.cpp`
  (debug list grows inside a frame, nothing outside). Covers FR-24.
- [x] **T6.2** Example core: `controls.{hpp,cpp}` (enums `Act`/`Ctx`; `move` vector = Circle Pad + D-Pad composite,
  `jump` A, `attack` B + locked touch button, `aim` axis = C-Stick X with L/R composite fallback, `pause` Start locked;
  menu context: up/down with repeat 24/6, confirm A, back B, reset X, reset all Y), `action_log.{hpp,cpp}` (fixed
  lines: `actions: jump pressed`, `actions: rebind jump -> B (swapped with attack)`, `actions: aim uses C-Stick` or
  `actions: aim uses L/R (no C-Stick)`), `main.cpp` (load bindings at start, log `actions: loaded …`). Files:
  `examples/actions/*`, `examples/CMakeLists.txt`. Test: builds; log lines checked by T6.5. Covers AC-7, AC-8.
- [x] **T6.3** `play_scene` (shapes only: character moved by `move`, jump, `attack` charge bar from `held_frames`,
  `aim` line, on-screen touch button on the bottom screen, `debug_draw` panel) and `controls_scene` (pushed by `pause`,
  context bound in `on_enter`; list of actions with `describe`, rebind selected with listen + swap shown, reset one /
  reset all, "Reload bindings" and "Bench" items, saves on leaving with `actions: saved N bytes`). Files:
  `examples/actions/play_scene.{hpp,cpp}`, `controls_scene.{hpp,cpp}`. Test: PC screenshots
  (`ACTIONS_DEMO=play|controls`, `NNH_SIM_INPUT`, `NNH_EXIT_AFTER_FRAMES`, `NNH_SCREENSHOT`), read back. Covers AC-7.
- [x] **T6.4** Self-check and bench: `self_check.{hpp,cpp}` (pure: the expected sequence of remap results and action
  edges, checksum of the saved bytes before/after reload, verdict line `actions: PASS …` or `actions: FAIL <reason>`
  as a warning); `bench_scene` (a second set of 64 actions × 4 mixed slots, 600 frames, `profile::slowest` scope
  `actions`, logs `actions: bench avg X ms worst Y ms`, fails above 0.1 ms on the console). Files:
  `examples/actions/self_check.{hpp,cpp}`, `bench_scene.{hpp,cpp}`. Test:
  `tests/actions_example/test_self_check.cpp`. Covers AC-8, AC-9.
- [x] **T6.5** The script `examples/actions/tests/check.nnhs` (only controls every 3DS has): open the controls screen,
  reset all, rebind `jump` to B and expect the swap line, reset `jump`, rebind it to Y, leave (save), play: Y →
  `jump pressed`, A → `attack pressed`, reopen, "Reload bindings" → checksum line, "Bench" → bench line, then
  `actions: PASS`; `fail-on "actions: FAIL"` and `fail-on "] E "`. Test: `nnh run actions --pc --script
  examples/actions/tests/check.nnhs` → PASS, twice in a row (repeatable with a saved file present). Covers AC-8, FR-22.

## Stage 7: console check (the only step on the 3DS)

- [x] **T7.1** Build and size check: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development build of
  `examples/actions`; `hello.elf` size against T0.1 recorded; `scripts/check_release_strings.sh` on the Release
  `actions.elf` (no debug-view strings). Covers AC-10, FR-24.
- [x] **T7.2** On the Old 3DS: the maintainer opens the Homebrew Launcher and presses Y; then `nnh run actions
  --script examples/actions/tests/check.nnhs` runs with nobody touching the console. Pass = the saved log ends with
  `actions: PASS` (with the `aim` fallback line saying which path the console took and the bench number < 0.1 ms).
  Files: plan.md ("Stage 7 results": verdict, bench avg/worst, log path). Covers AC-8, AC-9. Prepared: command and expected lines in plan.md "Stage 7: console run (prepared)".

  **Result (2026-11-22, Old 3DS XL, Development):** every functional line as on the PC (`aim uses L/R (no C-Stick)`,
  rebind/swap/reset, `saved 40 bytes sum AAB4AA36`, the same sum after the reload). Bench 0.224 ms on the first run,
  over the 0.1 ms target; after the evaluation was optimized (plan "Console bench refinement") **0.142 ms** avg,
  0.230 ms worst, the budget became < 0.2 ms (maintainer's choice), and the run ends `actions: PASS 7/7`, `nnh: PASS`.

## Stage 8: docs, roadmap, status

- [x] **T8.1** `docs/api/actions.md` (kinds, declaring, sources and bindings, combining, contexts and scenes, the
  no-leak rule, queries and events with their order, repeat, remapping and conflicts, display names, saving and
  versioning with the `NNHB` layout, Old 3DS fallbacks, the simulator and scripts, capacities, memory and the measured
  cost) and header comments (allocates? per-frame safe? misuse?). Covers VIII, AC-11.
- [x] **T8.2** Architecture and other pages: `docs/architecture/events.md` (dispatch order: action events after input
  events), `docs/architecture/scenes.md` (second user of attachments: contexts bound for one stay), the frame flow in
  `scenes.md`/`README.md` if it lists the frame steps, `docs/api/input.md` ("Remapping … is a later spec" → link),
  `docs/api/scene.md` (binding contexts), `docs/REFERENCES.md` (Godot InputMap, Unity Input System), `CLAUDE.md`
  (layout paragraph and module list), `docs/architecture/project-structure.md` if a rule changed;
  `python -I scripts/docs_tool.py check` → 0 broken links. Covers AC-11.
- [x] **T8.3** `docs/architecture/decisions.md`: D32 change-log note "implemented" with any stage refinements. Covers
  AC-11.
- [ ] **T8.4** Final measurements (header cost of `actions.hpp`, build times, `hello` sizes, set sizes on PC and 3DS)
  in plan.md and `docs/ROADMAP.md`'s baseline table if it changed. Covers III. Partly: `hello` sizes and the PC bench in plan.md; build times and set sizes on 3DS not measured yet.
- [x] **T8.5** `docs/ROADMAP.md`: "Input actions" checkbox.
- [x] **T8.6** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T7.2 pending").
