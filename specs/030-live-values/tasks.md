# Tasks 030: Live values

Status: implemented 2026-11-26; Linux checks passed; console correctness passed, console run 2 (bench budgets) pending; approved 2026-11-26 (with plan.md)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task is one or a few TDD loops (test first, then code). `[P]` means the task can be done in parallel with the
previous one. Every stage ends with something verifiable on PC (`ctest --preset pc-debug` and `pc-release`, a
screenshot, a `--pc` script). The work happens in a **separate git worktree** (branch `spec-030`) created from `main`
**after spec 029 is merged** (plan.md, "Spec 029 overlap"); shared files only get insertions after 029's blocks. The
order: the pure pieces shared with `nnh` first (names, stepping, float text, the wire), then the registry and the public
API, pause and step in core, the overlay pages and the panel, the game side of the remote, the `nnh` side, then the
example, the self-check, the bench and the ARM pre-check before the only console stage (10). Stage 9 is run on the Linux
build VM by the lead. Deviations found while building go into plan.md as "Stage N refinements", as in plans 012–029.
While developing, run the module's tests (`nnh_tests live_`, `debug_`, `app_`, `gfx_replay`, `audio_hold`, `remote_`;
`nnh_tool_tests live script`); the full suite at stage ends.

Guards held at every stage end (the "stage-end guards"): ctest pc-debug and pc-release; 3ds-release and 3ds-dev without a
warning; 3ds-release `hello.elf`/`.3dsx` **SHA-256 identical** to T0.1's and pc-release `hello` size and `nm -C`
**identical** (plan.md, "Release by construction"; when a hash moves, the touched objects' disassembly is compared);
Debug/Development `hello` growth recorded; every earlier golden (`ui: PASS 30400DA6`, `camera: PASS 5201E6CF`,
`queries: PASS 4ACAA177`, `entities: PASS C8FB9A41`, `prefabs: PASS 7BB7901E`, `hierarchy: PASS 23DB5C36`,
`tweens: PASS B44CA3F6`, `pathfinding: PASS 1C0B5992`, `particles: PASS C5EF1A84`, `transitions: PASS BFE1D810`, the
models self-check `A3BF4EDF`, the stereo golden `B3550D40`, `data check: PASS checksum 0xD5BD3020`, the saves goldens,
029's localization golden and fixture checksums, the platformer replay ctest with the tree's lines — values re-read from
the tree at T0.3).

## Stage 0: baseline

- [x] **T0.0** Start: spec 029 merged into `main` (the lead); worktree `spec-030` from that `main`; the message and record
  numbers of plan.md confirmed against the merged `remote_wire.hpp` (029's `Language = 8`; records 1–7). Files: plan.md
  ("Stage 0 results"). Covers D28.
- [x] **T0.1** Baseline numbers (Windows, 3 runs each; scripts in `build/ref-030/`, not committed): `hello` sizes
  (`size`: text/data/bss, file bytes), `nm -C` lists and SHA-256 for 3ds-release, 3ds-dev, pc-release, pc-debug; the
  3ds-release objects `app.o`, `overlay.o`, `debug.o`, `debug_pass.o`, `gfx_frame.o`, `clock.o`, the scene stack's object,
  the audio core's objects, `system_3ds.o` and pc-release `devtools_pc.o`, `app_pc.o` (bytes and disassembly) for later
  diffs; whether the 3DS and PC links drop unreferenced functions (`--gc-sections`?) — decides rule 2 of plan.md; the
  `.text` of the 3ds-release `platformer` ELF; edit `examples/ui/title_scene.cpp` → simulator and `.3dsx` times; clean
  `nnh_core` and `nnh`; preprocessed lines and `-fsyntax-only` time of a one-line TU including `<nnh/math.hpp>`. Files:
  plan.md ("Stage 0 results"). Covers III, FR-20, FR-21, AC-6.
- [x] **T0.2** [P] Reference screenshots in `build/ref-030/ref/`: `hello`, `sprites`, `ui` (`UI_SHOT=title`), the
  platformer (level 1 at 90, `PLATFORMER_DEMO=replay` at 64 and 312, `bench` top), `models` (check), one overlay shot
  (`hello` with `debug::set_overlay_visible(true)` through a tiny probe, the Stats page) — `build/ref-030/compare.sh`
  retakes and compares bytes. Files: plan.md. Covers AC-1, FR-7.
- [x] **T0.3** [P] Goldens and counts today: the stage-end guards' values as in the tree; `nnh_tests` count against
  `kMaxTests` (this spec adds ≈ 400 tests; raise it in `tests/framework/test_main.cpp`, one commit, if fewer than 450 are
  left). Files: plan.md, `tests/framework/test_main.cpp`. Covers VI.

## Stage 1: the pure pieces shared with `nnh`

- [x] **T1.1** Names: `engine/core/src/live/live_names.{hpp,cpp}` (rules, leaf/group split, group hash, run-time check)
  and `live::Name`'s `consteval` constructor in `<nnh/live.hpp>` (only `Name` and the constants at this point). Test:
  `tests/live/test_live_names.cpp` (every rule and edge: 31/32 bytes, each character class, `//`, leading/trailing `/`,
  4 levels; hashes equal `fnv1a32`), compile-fail ctests `live_name_too_long_fails`, `live_name_bad_character_fails`,
  `live_name_empty_part_fails`, `live_name_too_deep_fails` (`try_compile` with the expected diagnostic text). Covers FR-1,
  III.
- [x] **T1.2** [P] Kinds and stepping: `live_kinds.hpp`, `live_steps.{hpp,cpp}` (`-ffp-contract=off`; grids, up/down by 1
  and 10, `max` stop, drag, bar fill, colour channels, enums, bools, refusals). Test: `tests/live/test_live_steps.cpp`
  (hand-computed floats: `0..10/0.25`, `−1..1/0.1`, `0..1/0.3` (max off-grid), off-grid values, n = 2²⁴, negative zero,
  ×10 at both ends; ints with steps 1, 5, 7; drag at every pixel of a 13-pixel bar; bar fill; every refusal). Covers
  FR-8, FR-9, FR-13, FR-22, Q9.
- [x] **T1.3** Ryu: `third_party/ryu/` — `f2s.c` and the headers it includes at one pinned `ulfjack/ryu` commit,
  unmodified, `README.md` with the commit, SHA-256 of each file and the licence choice (BSL-1.0, `LICENSE-Boost`), a C
  object library (warnings off) linked into `nnh_core` and `nnh` (question 3; if the maintainer prefers our own
  formatter, this task writes `live_float_text.cpp` instead and T1.4's tests stay). Test: builds on pc presets, the 3DS
  presets and the host build of `nnh`; `nm` of a 3ds-release game without `live::` shows no `f2s`. Covers IX, III.
- [x] **T1.4** Value text: `live_format.{hpp,cpp}` (the decimal layout around Ryu's digits, ints, colours, Vec2, bools,
  enums, quoted text). Test: `tests/live/test_live_format.cpp` (hand-written expected texts: `5.25`, `0.1`, `0.00001`,
  `1e-06`, `-0`, `16777216`, `123456790`, `1e+09`, `3.4028235e+38`, `1e-45`, the −5..8 exponent boundary on both sides; `#FFC040FF` and
  panel `#FFC040`; `(12.5, 40)`; escapes; round trip of 10⁶ seeded floats + every power of two + subnormal edges through
  `std::from_chars`), ctest `live_float_text_exhaustive` (all 2³² patterns, label `slow`, run on VM 120 at T9.1). Covers
  FR-13, FR-22.
- [x] **T1.5** The wire: `engine/core/src/remote/live_wire.{hpp,cpp}` (the three messages and five records: encode,
  decode, validate, pack into `Message`), `remote_wire.{hpp,cpp}` (type numbers, decoder dispatch, record length rules;
  `Message` stays 32 bytes). Test: `tests/remote/test_live_wire.cpp` (round trips; every refusal: hash 0 where not
  allowed, op/arg/count ranges, step 0 or 3601, non-zero reserved bytes; record lengths at their bounds and one past);
  `test_remote_wire.cpp` unchanged cases pass; the remote fuzz target's corpus gains the new messages. Covers FR-11,
  FR-18, X.
- [x] **T1.6** Stage end: the stage-end guards (nothing links the new files yet: `hello` untouched by construction);
  plan.md ("Stage 1 results": the Ryu commit, the header's cost so far). Covers AC-6.

## Stage 2: the registry and the public API

- [x] **T2.1** The block and registry: `live_block.hpp`, `live_registry.{hpp,cpp}` (create on first use with
  `configure()`'s capacities, tag `live`; add/remove with the sorted index; find by hash; generations; group-clash check;
  shutdown hook; `g_live_ops` set), `engine/core/src/debug/live_hook.hpp` (`LiveOps`). Test:
  `tests/live/test_live_registry.cpp` (block size formula per capacity, tag accounting, sorted order on random
  insert/remove, generations, every assert through the test hook, capacity + 1, `configure()` after use asserts, the
  block freed at shutdown, `g_live_ops` null until the first registration). Covers FR-1, FR-6, FR-18, FR-19.
- [x] **T2.2** Tunables and actions: `<nnh/live.hpp>` (the full API of plan.md, Release inline forms), `live_tune.cpp`
  (`tune` for each kind, enums, `action`, `Handle` move/destroy/`changed`/`reset`, `reset_all`, `reset_group`). Test:
  `tests/live/test_live_tune.cpp` (defaults from the variable, every kind's entry, enum sizes 1/2/4, actions, handle moves,
  stale use asserts, run-time names copied), `tests/live/test_live_release.cpp` (built in pc-release: every call compiles,
  nothing registered, `changed()` false, `sizeof(Handle) == 4`, no allocation). Covers FR-1, FR-2, FR-4, FR-5, FR-6,
  FR-20, Q9.
- [x] **T2.3** The application point: `live_apply.{hpp,cpp}` (pending edits, the remote op queue, `live` frame start:
  resolve, refuse, write, `changed_frame`, callbacks in sorted order, actions), the call in `app.cpp`'s Dev branch after
  `++frame_count`. Test: `tests/live/test_live_apply.cpp` (plan.md's FR-3/4/5 cases on the fake's `nnh::frame()`; a
  callback that registers or destroys handles; a `set` for a tunable registered in that frame's `on_enter()`; queue full
  → refused). Covers FR-3, FR-4, FR-5, Q6, Q9.
- [x] **T2.4** [P] Watches: `live_watch.cpp` (registered watches of every kind, `show()` with its hash table, expiry every
  60 frames after 120 unseen, reading values into bytes at the frame end). Test: `tests/live/test_live_watch.cpp` (every
  kind's bytes; a read counter proves registered watches are read only when shown or followed; latest `show()` wins;
  text cut at 31 bytes with one warning; expiry; the frame-end read point). Covers FR-17, FR-19.
- [x] **T2.5** [P] The dump: `live_dump.cpp`. Test: `tests/live/test_live_dump.cpp` (the exact lines of plan.md:
  ungrouped first, nested tables, colours, enums, bools, `# default`, nothing changed, ordering). Covers FR-14.
- [x] **T2.6** [P] Cost and plain loops: `tests/live/test_live_cost.cpp` (no allocation over 100 000 frames of edits,
  `show()` and follows; a registry-walk counter stays 0 with the overlay hidden and nothing pending),
  `tests/live/test_live_plain_loop.cpp` (tunables and watches in a `while (nnh::frame())` game). Covers FR-19, FR-21.
- [x] **T2.7** [P] Misuse and random operations: `tests/live/test_live_misuse.cpp` (plan.md's misuse list asserts and is
  harmless on the Release path), `tests/live/test_live_random_ops.cpp` (100 000 seeded registrations, removals, edits,
  shows, resets with guard bytes around every variable; invariants). Covers FR-6, FR-18.
- [x] **T2.8** Stage end: the stage-end guards; include check (`<nnh/live.hpp>` within +250 lines and +0.03 s of T0.1's
  `<nnh/math.hpp>` TU); plan.md ("Stage 2 results": bytes per entry measured on 3ds-dev and pc-debug against plan.md's
  table). Covers III, II.

## Stage 3: pause and step in core

- [x] **T3.1** Requests: the contract's three `DevRequest` bits; `devtools_pc.cpp` (`Pause`, `F10` → requests),
  `app_pc.cpp` (`NNH_HOLD=1` once), `system_3ds.cpp` (`--nnh-hold` once), `system_fake.cpp` + `platform_fake.hpp`
  (`request_pause/step/hold`) — all in Dev branches. Test: conformance (`poll_dev_requests` returns each bit once on the
  fake and the PC with the variable set), 3ds-release and pc-release `hello` identical (a stage-end check run early).
  Covers FR-16, I.
- [x] **T3.2** The state machine: `src/app/dev_pause.{hpp,cpp}` (`PauseState`: requests, when each takes effect, steps,
  held start with its 600-tick timeout; pure part), `live::pause()`'s back end. Test: `tests/app/test_dev_pause_state.cpp`
  (every transition, a step while running pauses first, step N counts, hold with and without an attach, timeout). Covers
  FR-16.
- [x] **T3.3** Engine controls: `src/debug/dev_controls.{hpp,cpp}` (held/pressed/released per tick from the post-remote
  sample, repeat 24/6, touch down/held/up). Test: `tests/debug/test_dev_controls.cpp` (edges, repeat timing, exclusive
  mode reads the script's sample only). Covers FR-8, FR-10.
- [x] **T3.4** The held frame: `src/gfx/gfx_replay.cpp`, `gfx_frame.cpp` and `gfx_internal.hpp` (Dev: the per-frame clears
  of draws, tiles, particle batches and 3D records move to the start of the next game frame's recording; replay; textures
  re-resolved; validity), `src/debug/debug_held.cpp` + the debug list's mark (the held frame's game debug text), the
  scene stack's Dev-only "applied something" flag. Test: `tests/gfx/test_gfx_replay.cpp` (a paused tick's fake items
  equal the held frame's for sprites, text, shapes, tiles, particles and a 3D draw; a released texture is skipped; a
  scene change drops the replay; Release-path behaviour unchanged in pc-release), every existing gfx test in pc-debug and
  pc-release. Covers FR-16, FR-22.
- [x] **T3.5** [P] Audio and time: `src/audio/audio_hold.cpp` (hold and release every playing voice and stream at the
  platform, the game's pause flags untouched), the frame-clock restart in `time_internal.hpp`/`clock.cpp` (Dev accessor).
  Test: `tests/audio/test_audio_hold.cpp` (fake voices and streams paused on hold, released for one stepped frame, a
  voice the game paused stays paused), `tests/app/test_dev_pause.cpp` part 1 (`time::delta()` of the first frames after a
  pause and after steps). Covers FR-16, Q7.
- [x] **T3.6** The paused ticks in `nnh::frame()`: `app.cpp`'s Dev order of plan.md ("The frame in Debug/Development
  builds"), `dev_pause.cpp`'s loop. Test: `tests/app/test_dev_pause.cpp` part 2 (no scene update and no input event while
  paused; plain loops wait; `frame_count()` and the frame clock stop; step N runs exactly N; quit while paused; the held
  start draws cleared screens and the message), `tests/app/test_frame_loop.cpp` unchanged cases pass in both flavours.
  Covers FR-16, FR-21.
- [ ] **T3.7** Stage end: the stage-end guards (the first stage that edits Release-linked files: per-object diffs
  recorded); Debug/Development `hello` growth so far; the simulator's `Pause`/`F10` by hand on `examples/platformer`
  (pause, step, resume; audio holds) and a screenshot of a paused frame equal to the frame before it. Files: plan.md
  ("Stage 3 results"). Covers AC-6, FR-16.

## Stage 4: the overlay pages and the panel

- [x] **T4.1** Pages and controls: `src/debug/overlay_pages.{hpp,cpp}` (Stats → Tunables → Watches, SELECT + R, the
  DevPanel blocker, chords SELECT + Y/X, empty pages without the module, `live::show_panel`), `overlay.cpp`'s Dev branch,
  `InputBlocker::DevPanel`. Test: `tests/debug/test_overlay_pages.cpp` (cycling, controls taken and given back, the game's
  queries idle while taken, a held button blocked until released after closing, independence from `input::set_blocked`
  and transitions, chords reach the pause), `tests/debug/test_overlay.cpp` unchanged cases pass. Covers FR-7, FR-10, FR-16.
- [x] **T4.2** The panel model: `live_panel_model.{hpp,cpp}` (rows from the sorted index, groups, selection, scroll,
  repeat, buttons → edits, channels, enums, actions, resets, dump requests, pending marks). Test:
  `tests/live/test_live_panel_model.cpp` (navigation with wrap only on fresh presses, repeat, ×10 with L and with R,
  stopping at max, colour channel cycle, enum ends, bool, actions once per press, X on an entry and on a group, Y, B,
  selection kept when an entry is removed). Covers FR-5, FR-8, Q9.
- [x] **T4.3** [P] Layout and touch: `live_panel_layout.{hpp,cpp}`, `live_panel_touch.cpp`. Test:
  `tests/live/test_live_panel_touch.cpp` (row hit boxes, taps on group rows, drags tick by tick through a bar with the
  rounding rule, `-`/`+` with repeat while held, bool and action hits, a touch starting outside does nothing). Covers
  FR-9.
- [x] **T4.4** Drawing: `live_panel_draw.cpp` (Tunables and Watches pages, header, footer, marks, bars, swatches, channel
  line), the Stats line in `live_ops.cpp`. Test: `tests/live/test_live_panel_draw.cpp` (debug-list items of every row kind
  at exact positions and texts; a full page within 90 entries and 1.5 KB; paused `~` marks). Then PC screenshots by hand
  (a probe or the example skeleton) read back; exact pixels written into plan.md ("Stage 4 refinements"). Covers FR-7,
  FR-8, FR-22.
- [x] **T4.5** Stage end: the stage-end guards; `compare.sh` (every reference identical, the Stats page included); plan.md
  ("Stage 4 results"). Covers AC-1, AC-6.

## Stage 5: the game side of the remote

- [x] **T5.1** Session glue: `src/remote/remote_live.cpp` (dispatch of `LiveSet`/`Live`/`Pause`, the `live_records` flag,
  answers without the module, `LiveState`, refusals counted and warned without the shutoff), `remote_session.cpp`,
  `remote_internal.hpp` (usable on paused ticks: messages due at the next game frame). Test:
  `tests/remote/test_remote_live.cpp` (every message applied at its frame; refusals; older-nnh case: no live record
  before a catalogue request; game without the module), `tests/remote/test_remote_pause.cpp` (pause at a named frame,
  step N, resume, messages and screenshots on paused ticks, `LiveState` on each applying tick). Covers FR-11, FR-16,
  FR-18.
- [x] **T5.2** Live records: `live_remote.cpp` (catalogue in sorted order and its end, `LiveEntry`/`LiveRemoved` while
  attached, Get and Follow → `LiveValue` at the frame end, paused answers, the follow table, dump requests). Test:
  `tests/live/test_live_remote.cpp` (record bytes against hand-built expectations; follows every N frames for count frames;
  16 + 1 follows; values while paused flagged), the session sends records in pieces (no buffer beyond the 64-byte fixed
  part). Covers FR-11, FR-14, FR-17, FR-18.
- [x] **T5.3** [P] Fuzzing: `tests/fuzz/fuzz_live_remote.cpp` (mini-fuzz in `nnh_tests`: random message streams into the
  session with a registry of every kind and pauses; invariants: values in range, guard bytes intact, no crash) and
  `tests/fuzz/libfuzzer_live_remote.cpp` (`NNH_BUILD_FUZZERS`). Test: 60 s clean under the host's ASan where available (the
  VM's clang run at T9.1). Covers FR-18, X, AC-5.
- [x] **T5.4** Stage end: the stage-end guards; plan.md ("Stage 5 results"). Covers AC-6.

## Stage 6: the `nnh` side

- [x] **T6.1** The catalogue and value text: `tools/nnh/src/remote/live_catalog.{hpp,cpp}`, `live_text.{hpp,cpp}` (the
  shared `live_names`, `live_steps`, `live_format` and Ryu compiled into `nnh`), `game_records.*` (records 8–12). Test:
  `tools/nnh/tests/test_live_catalog.cpp` (records in, lookups, groups, removals), `test_live_text.cpp` (every kind:
  `5`, `-2.5`, `1e-3` for floats, ints, `true`/`false`, `#RRGGBB[AA]`, enum names and indexes, Vec2 and text for
  expect-value; every refusal's exact text with did-you-mean), `test_game_stream.cpp` gains the new records. Covers FR-11,
  FR-13.
- [x] **T6.2** Script commands: `script.hpp`, `script_parser.cpp`, `script_live.cpp` (the new lines; paused-section
  rules), `script_timeline.*` (clock rules: zero-frame commands, `step N`, `expect-value` as a segment closer, screenshots
  while paused). Test: `tools/nnh/tests/test_script_live.cpp` (syntax and errors with lines; `tap while paused` and the
  other refusals), `test_script_timeline_live.cpp` (events and offsets of plan.md's examples); every existing parser and
  timeline test unchanged. Covers FR-12, FR-16.
- [x] **T6.3** The runner: `script_runner_live.cpp`, `script_runner.*` (catalogue request when needed, resolution per
  segment, `LiveSet`/`Live`/`Pause` encoding, `expect-value` windows and tolerance through follows, `LiveState` flow
  control, refusals failing with their line, an older game's `Reject::Type` message). Test:
  `tools/nnh/tests/test_script_runner_live.cpp` (a fake console: passes and failures of every new command; a name
  registered after the first segment; paused sections). Covers FR-12, FR-13, FR-16.
- [x] **T6.4** [P] Typed commands: `live_interactive.cpp`, `script_interactive.cpp` (help lines). Test:
  `tools/nnh/tests/test_live_interactive.cpp` (`tunes`, `get`, `set`, `reset`, `action`, `watches`, `watch … every N`,
  `unwatch`, `dump`, `pause`, `step`, `resume`: messages, refusals before sending, printed lines, stamping while paused).
  Covers FR-11, FR-16, FR-17.
- [x] **T6.5** [P] Dumps and `--tunes`: `dump_capture.{hpp,cpp}` (lines → `build/logs/<game>-<date>-<time>-tunes.toml`),
  `tunes_file.{hpp,cpp}` (reader), `cli.cpp` (`--tunes`, `--hold`), `netloader.cpp` (`--nnh-hold` in argv), the
  simulator's `NNH_HOLD=1`, a mini-fuzz of the reader. Test: `tools/nnh/tests/test_dump_capture.cpp`,
  `test_tunes_file.cpp` (format, quoted names, every error with `file:line`, limits), `test_netloader.cpp` (the argv
  block), `test_script_runner_live.cpp` (`--tunes`: unknown names skipped with their line, sends for the held frame,
  resume). Covers FR-14, FR-15, X.
- [x] **T6.6** Stage end: the stage-end guards; every example's `--pc` script still passes; plan.md ("Stage 6 results").
  Covers AC-6.

## Stage 7: `examples/live`, the platformer, the self-check, the bench, the ARM pre-check

- [x] **T7.1** The self-check (AC-2): `examples/live/self_check.{hpp,cpp}` (plan.md's simulation from the current values,
  `kLiveDefaultsGolden`, `kLiveScriptGolden`). Test: `tests/live_example/test_self_check.cpp` (defaults golden; the
  scripted values' golden; a one-bit change of a tunable changes the sum — checked once by hand). Files:
  `tests/CMakeLists.txt`. Covers AC-2, FR-22.
- [x] **T7.2** The showcase: `examples/live/main.cpp`, `demos.hpp`, `tuning.{hpp,cpp}`, `player.{hpp,cpp}`,
  `spawner.{hpp,cpp}`, `live_scene.{hpp,cpp}`, `check_scene.{hpp,cpp}`, `CMakeLists.txt`, `examples/CMakeLists.txt`.
  Test: the log lines of plan.md; ctest `live_self_check_pc`. Covers AC-1, FR-21, Q9, Q10.
- [x] **T7.3** Screenshots: `LIVE_SHOT=stats|tunables|watches|colour|paused` with `NNH_EXIT_AFTER_FRAMES` +
  `NNH_SCREENSHOT`, read back (the colour row open with its channel line, one value changed, the paused header). Covers
  AC-1, FR-7.
- [x] **T7.4** [P] The bench: `examples/live/bench.{hpp,cpp}`, `bench_scene.{hpp,cpp}` (plan.md's phases; each timing only
  its workload; lines and verdict; Debug `not judged (Debug)`). Test: a pc-dev run logs every line; the memory line matches
  plan.md's table within 10 % (else plan.md is corrected). Covers AC-7.
- [x] **T7.5** The scripts: `examples/live/tests/check.nnhs`, `panel.nnhs`, `keep.nnhs`, `keep_second.nnhs` (plan.md's
  outlines; every expect followed by `wait 10` before the next input) and ctests `live_check_script_pc`,
  `live_panel_script_pc`, `live_keep_pc` (two runs through the saved dump; `RESOURCE_LOCK nnh_remote_port`). Test: each
  passes with `nnh run live --pc --no-build` twice in a row. Files: plan.md ("Stage 10: console run (prepared)" with every
  expected line). Covers AC-2, AC-3, AC-8.
- [x] **T7.6** The platformer: `examples/platformer/tuning.{hpp,cpp}` (run speed, gravity, jump speed, max fall, camera
  look-ahead constants as tunables with the same defaults), `player.cpp`, `level_camera.*`, `level_scene.cpp`,
  `CMakeLists.txt`; `tests/platformer/` builds. Test: the replay ctest's lines unchanged (the tree's), `level.nnhs` and
  `gate.nnhs` pass with `--pc`, a by-hand session tuning the jump on the Tunables page. Covers Q10, FR-21.
- [x] **T7.7** ARM11 pre-check (before any console run): `arm-none-eabi-objdump -drC` of the 3ds-dev objects of `src/live/*`,
  `dev_pause.o`, `dev_controls.o`, `overlay_pages.o`, `gfx_replay.o` and the example's `bench.o`; per plan.md's cost table
  record instructions, calls, `__aeabi_*` (none in stepping, drawing or `show()`), VFP use, `-fstack-usage` along the page
  drawing and the paused tick (< 1 KiB); fix what differs without changing a pixel, a value or a golden. Files: plan.md
  ("Stage 7 ARM pre-check": the table and the expected console numbers). Covers AC-7, II.

## Stage 8: mutation testing, release hygiene, opt-in checks, build times

- [x] **T8.1** Mutation pass as in plans 019–029: plan.md's planned hand mutants (`build/ref-030/mutants.py`, applied one
  at a time with the module filters) and Release-path mutants in pc-release; survivors become tests or are argued
  equivalent. Files: plan.md ("Mutation testing"). Covers VI.
- [x] **T8.2** [P] Release hygiene: `scripts/check_release_strings.sh` gains the live texts (`dump: begin`, `refused`,
  `not declared`, `waiting for nnh`, `frame not held`, the panel's hints, the misuse messages) and the example's names
  (`player/jump`, `colors/sky`, `spawn/alive`); run on the 3ds-release and pc-release `live` and `platformer`; `nm` of both
  shows no `live`, `f2s`, `dev_pause`, `overlay_pages` symbol; the Release example prints `live: sum
  <kLiveDefaultsGolden>`. Covers FR-20, AC-6.
- [x] **T8.3** [P] Opt-in by linking: probes (`build/ref-030/probe_*`, not committed) linked against the 3ds-dev and
  3ds-release libraries: a Development game without `live::` (no `live_` symbol; pages show "no live values"), a game with
  one tunable (the module's size recorded); `hello` 3ds-release bytes and pc-release size/`nm` identical to T0.1's;
  Debug/Development `hello` growth recorded against question 7's estimate. Files: plan.md ("Stage 8 results"). Covers
  FR-20, FR-21, XI.
- [x] **T8.4** Build times and regressions: every example's console script passes with `--pc`; `compare.sh`; edit
  `examples/live/live_scene.cpp` → simulator and `.3dsx` times; clean `nnh_core` and `nnh`. Files: plan.md ("Stage 8
  results"). Covers III, AC-6.

## Stage 9: Linux checks (VM 120, run by the lead)

- [x] **T9.1** (done 2026-11-26: plan.md "Stage 9 results") On `nnhge-build` (start it per CLAUDE.md, never with the Bazzite VM; shut it down after): GCC 13, GCC 14 and
  clang 18 (ASan/UBSan): `nnh_tests`, `nnh_tool_tests` and ctest pass in pc-debug and pc-release; AC-2's goldens identical
  on all three and on the 32-bit host build (`-m32 -msse2 -mfpmath=sse`); `live_float_text_exhaustive` (all 2³² floats);
  libFuzzer `libfuzzer_live_remote` and the remote targets 10 minutes each, plus the mini-fuzzers; the `--pc` scripts.
  Files: tasks.md (results), plan.md ("Stage 9 results"). Covers I, X, FR-13, FR-22, AC-2, AC-5.

## Stage 10: console check (the only steps on the 3DS)

- [x] **T10.1** (on the PC) Builds and checks: `scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh
  cmake --build --preset 3ds-dev --target live_3dsx platformer_3dsx` without a warning; T7.7's table still true;
  3ds-release `hello.elf` byte-identical to T0.1's. Covers AC-6, II.
- [ ] **T10.2** Old 3DS, nobody touching it after Y. Runs, each: Homebrew Launcher, press **Y**, then on the PC:
  1. `build/pc-debug/tools/nnh/nnh run live --script examples/live/tests/check.nnhs` → every value, `live: sum
     <kLiveScriptGolden>` equal to the PC's, the pause and 3-frame step, the dump file saved, the bench lines and the
     verdict (numbers recorded); screenshots compared with the `--pc` run's (the panel's pixels identical);
  2. `build/pc-debug/tools/nnh/nnh run live --script examples/live/tests/panel.nnhs` → PASS (buttons and touch);
  3. `build/pc-debug/tools/nnh/nnh run live --script examples/live/tests/keep.nnhs`, then (Y again)
     `build/pc-debug/tools/nnh/nnh run live --tunes <the saved file> --script examples/live/tests/keep_second.nnhs` → PASS
     (values on frame 1 through the held start: `--nnh-hold` in argv);
  4. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` → the tree's replay lines
     and `60 Hz PASS` (the tunables changed nothing);
  5. (yes/no, by hand, ≈ 1 minute) `build/pc-debug/tools/nnh/nnh run platformer` (no script): SELECT + L, SELECT + R,
     raise `player/jump` with the D-Pad, jump — higher? SELECT + Y pauses (the level stays on screen, music holds),
     SELECT + X steps one frame — yes/no.
  Files: plan.md ("Console run 1": verdicts, bench numbers, log paths, the screenshot comparison). Covers AC-1, AC-2, AC-3,
  AC-7, AC-8, FR-16, FR-22.
- [x] **T10.3** (round done 2026-11-26: plan.md "Optimisation round"; budgets from console run 2, pending) (only if a bench target is missed) Optimisation round: find the cost (3ds-dev disassembly, the bench's
  phases), apply risk 1's list without changing a pixel, value or golden (tests first for any rewrite); then T10.2 run 1
  again; if still missed, the budget is set from the measurement with ≈ 25 % margin and written in the spec's AC-7,
  `bench.cpp` and `docs/api/live.md`. Files: plan.md ("Optimisation round", "Console run 2 and budgets set"). Covers AC-7.

## Stage 11: docs, roadmap, status

- [x] **T11.1** `docs/api/live.md` (declaring tunables, kinds, ranges and stepping, enums, actions, watches both ways,
  handles and lifetime, `changed()` and callbacks, the panel and its controls, pause and step, `nnh` commands, the dump and
  `--tunes`, Release behaviour, capacities and memory, costs measured at T10.2, misuse); header comments (allocates?
  per-frame safe? misuse?). Covers VIII, AC-9.
- [x] **T11.2** `docs/guides/tuning-your-game.md`: from constants to tunables, tuning a jump on the console, watching
  state, pausing and stepping, scripts with `set`/`expect-value`, keeping a session (dump, paste into a data asset,
  `--tunes`), what Release does. Covers AC-9.
- [x] **T11.3** Other pages: `docs/api/debug.md` (overlay pages, chords), `docs/api/simulator.md` (`Pause`/`F10` now in
  core, `NNH_HOLD`), `docs/architecture/debugging.md` (pause and step on the console, the panel, the new messages and
  records), `docs/guides/nnh-tool.md` (commands, script commands, `--tunes`, `--hold`), `docs/guides/testing-on-the-console.md`,
  `docs/guides/debugging-on-3ds.md`, `docs/architecture/foundation.md` and `project-structure.md` (`src/live/`,
  `third_party/ryu`), `docs/tech/devkitpro-3ds.md` (`--nnh-hold`, audio hold), `docs/REFERENCES.md` (Godot, Unity,
  Hargreaves' tweakables, the debug menu article, tweakable constants, Corrade, tunable-cpp, AntTweakBar, Ryu),
  `CLAUDE.md` (layout paragraph and the `live` commands); `python -I scripts/docs_tool.py check` → 0 broken links. Covers
  AC-9.
- [x] **T11.4** `docs/architecture/decisions.md`: D51 and D52 change-log notes "approved"/"implemented" with the stage
  refinements and the maintainer's answers; D28's new messages and records, D38's DevPanel blocker and D17's overlay pages
  noted. Covers AC-9.
- [x] **T11.5** Final measurements (header cost, build times, `hello` sizes, the module's `.text`, the block's bytes) in
  plan.md and `docs/ROADMAP.md`'s baseline table if it changed. Covers III.
- [x] **T11.6** `docs/ROADMAP.md`: the "Live values in the simulator" checkbox, with the implemented summary.
- [x] **T11.7** spec, plan, tasks and `specs/README.md` status → implemented (or "implemented, console T10.2 pending").
- [ ] **T11.8** (the lead) Merge `spec-030` into `main` (rebased; insertions after 029's blocks); then the full ctest
  (pc-debug, pc-release), every example's `--pc` script, AC-2's golden and every earlier golden, one 3ds-release build;
  T10.2 runs repeated if the merge happened after them.
