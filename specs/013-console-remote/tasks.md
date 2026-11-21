# Tasks 013: Console remote — input, screenshots and test scripts from the PC

Status: approved (2026-11-21); next: stage 1
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task. Every stage ends with
something verifiable on PC (`ctest --preset pc-debug`, a script run in the simulator). Console steps are marked
**3DS**: the agent runs them with `nnh run`; the maintainer's only step is to open the Homebrew Launcher and press
**Y** when asked.

## Stage 1 — wire format and the console-side logic (pure)

- [ ] **T1.1** wire constants, the 16-byte message (encode/decode, every validation rule: magic, type, ranges, button
  mask, disjoint masks, reserved bytes), the record header (encode/decode, length caps), `static_assert` that the
  button mask matches `nnh::Button` (in a core file, not in the pure one) — files: `engine/core/src/remote/remote_wire.{hpp,cpp}`,
  `engine/core/CMakeLists.txt` — test: `tests/remote/test_remote_wire.cpp` — covers FR-8, FR-1
- [ ] **T1.2** reader: bytes → messages across partial reads, `Hello` first, frame order, malformed counting, 8 in a
  row → off, reads bounded by the room given — files: `remote/remote_reader.{hpp,cpp}` — test: `test_remote_reader.cpp`
  — covers FR-8
- [ ] **T1.3** [P] schedule: 64-entry FIFO, due messages for a frame, frame 0 = next frame, late (wanted, applied),
  full → refused — files: `remote/remote_schedule.{hpp,cpp}` — test: `test_remote_schedule.cpp` — covers FR-1, FR-8
- [ ] **T1.4** [P] remote input: apply `Buttons`/`Stick`/`Touch`, merge into an `InputSample` (OR, further-pushed stick,
  remote touch wins), release all — files: `remote/remote_input.{hpp,cpp}` — test: `test_remote_input.cpp` — covers FR-1
- [ ] **T1.5** fuzz target `remote_target` (reader → schedule → input over simulated frames; a decoded message must
  encode back to the same 16 bytes) — files: `tests/fuzz/fuzz_targets.{hpp,cpp}`, `test_fuzz_smoke.cpp`,
  `libfuzzer_remote.cpp`, `tests/CMakeLists.txt` — test: mini-fuzz in `nnh_tests` — covers FR-8, AC-3

Verifiable: `nnh_tests remote` and `nnh_tests fuzz` green on PC.

## Stage 2 — the session in core, over the fake platform

- [ ] **T2.1** contract `nnh/platform/remote.hpp`; fake implementation (connect, push bytes, disconnect, recorded
  sends, synthetic captures, capture failure); conformance: `remote_receive` is −1 without a connection — files:
  `engine/core/contract/nnh/platform/remote.hpp`, `engine/platforms/fake/src/remote_fake.cpp`, `fake_internal.hpp`,
  `include/nnh/platform_fake.hpp`, `CMakeLists.txt`, `README.md`, `tests/platform_conformance/test_contract.cpp` —
  covers FR-7, FR-8
- [ ] **T2.2** session hooks: attach on `Hello` (reply with frame, capabilities, flavor), ticks (after `Hello`, then
  every 4th frame), apply due messages, `Late`/`Rejected`/`Dropped` records, warnings rate-limited (channel `remote`),
  disconnect → release all and stop polling, `Bye` (and `quit`) — files: `remote/remote_session.cpp`,
  `remote/remote_internal.hpp`, `log/engine_channels.*` — test: `test_remote_session.cpp` (incl. no `Hello` → nothing
  sent) — covers FR-1, FR-7, FR-8
- [ ] **T2.3** wire into the frame loop (`if constexpr (build::kDevToolsEnabled)`): `remote_frame_start` between
  `read_input` and `input_update`, `remote_frame_ended` after `end_frame`, quit makes `frame()` return false, reset and
  shutdown — files: `engine/core/src/app/app.cpp` — test: `test_remote_frames.cpp` (press stamped for frame 10 seen by
  `pressed()` and `ButtonPressed` on frame 10 only; hold; real + remote; D-Pad mirror from a remote stick; touch
  events; disconnect publishes `ButtonReleased`) — covers FR-1, FR-8
- [ ] **T2.4** screenshots: due at the end of their frame, only the screens asked, `Screen` records (layout, size,
  frame), `ScreenFailed`, the "time is not representative" log line, buffer released at `Bye`/disconnect/shutdown —
  files: `remote/remote_screenshot.cpp` — test: `test_remote_screenshot.cpp` — covers FR-2
- [ ] **T2.5** Release check, first pass: `pc-release` builds; add the remote strings and symbol names to
  `scripts/check_release_strings.sh` and run it on `build/pc-release/examples/hello/hello.exe` — covers FR-7, AC-4

Verifiable: `nnh_tests` green; the release strings check passes on `pc-release`.

## Stage 3 — the tool's logic (pure)

- [ ] **T3.1** stream splitter: log lines and records from arbitrary chunks, `\r` stripped, text fallback on bad
  headers, 1 MiB cap, a partial line before a record — files: `tools/nnh/src/remote/game_stream.{hpp,cpp}` (+ core's
  `remote_wire.cpp` in `tools/nnh/CMakeLists.txt`) — test: `tools/nnh/tests/test_game_stream.cpp` — covers FR-2, FR-8
- [ ] **T3.2** [P] log matching: patterns (`*`, `?`, escapes, contains), frame and level from the `[f<n>` prefix, lines
  without a prefix — files: `remote/log_match.{hpp,cpp}` — test: `test_log_match.cpp` — covers FR-3
- [ ] **T3.3** script parser: every command and button, case, comments, blank lines, quotes and escapes, every error
  as `file:line: what` (all of them, limits, after `end`), the warning for input right after `expect` — files:
  `remote/script.hpp`, `remote/script_parser.cpp` — test: `test_script_parser.cpp` — covers FR-3
- [ ] **T3.4** timeline: steps → frame-stamped messages (clock from frame 1; hold, tap = +2, drag one step per frame,
  release all, screenshots), split at `expect` barriers — files: `remote/script_timeline.{hpp,cpp}` — test:
  `test_script_timeline.cpp` — covers FR-1, FR-3
- [ ] **T3.5** runner against a fake console: ≤ 48 unapplied, barrier, expect window and timeout (decided at the tick),
  clock = line frame + 1, `fail-on`, level F, late/rejected/dropped → fail with the line, game closes early, missing
  capability before any input, `end` waits for ticks and screenshots, file names, verdict and exit codes; interactive
  mode (lazy `Hello`, now + 6, expect never ends the run, `end` detaches) — files: `remote/script_runner.{hpp,cpp}` —
  test: `test_script_runner.cpp` — covers FR-3, FR-5
- [ ] **T3.6** [P] screen images and PNG: both layouts → RGB rows (known pattern), wrong sizes refused; RGB → PNG with
  stored zlib and core's CRC-32, decoded back in the test — files: `remote/screen_image.{hpp,cpp}`,
  `remote/png_writer.{hpp,cpp}`, `tools/nnh/CMakeLists.txt` (`crc32.cpp`) — test: `test_screen_image.cpp`,
  `test_png_writer.cpp` — covers FR-2
- [ ] **T3.7** [P] tool mini-fuzz: stream splitter and parser on mutated inputs — files: `tools/nnh/tests/test_fuzz_tool.cpp`
  — covers FR-8, AC-3

Verifiable: `nnh_tool_tests` green.

## Stage 4 — the simulator end to end

- [ ] **T4.1** PC transport: `NNH_REMOTE` (loopback only, ignored otherwise and in Release), connect with a 1 s timeout,
  non-blocking receive, send; the log goes to the socket while connected — files:
  `engine/platforms/pc/src/remote_pc.cpp`, `log_pc.cpp`, `app_pc.cpp`, `context_pc.hpp`, `CMakeLists.txt` (`ws2_32`)
  — test: conformance on pc (non-loopback ignored, −1 without a connection) — covers FR-4, FR-7
- [ ] **T4.2** [P] PC read-back of the screen targets (RGBA rows) — files: `engine/platforms/pc/src/remote_capture_pc.cpp`
  — test: a pc conformance test reads back a cleared colour — covers FR-2, FR-4
- [ ] **T4.3** the session loop: socket + stream + runner + stdin thread, lines printed and saved, `nnh:` progress
  lines in the log file, PNGs in `build/logs/<game>-<stamp>/`; `show_game_log` (also used by `nnh serve`) on top of it,
  without stdin for serve — files: `tools/nnh/src/remote/game_session.{hpp,cpp}`, `stdin_lines.{hpp,cpp}`,
  `game_log.*`, `log_listener.*`, `repo.*` — test: `test_repo.cpp` (screenshot folder), the loop by T4.5 — covers
  FR-2, FR-3, FR-5
- [ ] **T4.4** CLI and commands: `--script`, `--no-build`, `--script` + `--release` refused, parse before building,
  console path (`Hello`, 3 s answer, `Bye`), `--pc` path (listen on loopback, `NNH_REMOTE`, simulator on a thread,
  `Bye{quit}`, join), help text — files: `tools/nnh/src/cli.{hpp,cpp}`, `commands/run.cpp`, `commands/run_pc.cpp` —
  test: `test_cli.cpp` — covers FR-3, FR-4
- [ ] **T4.5** `examples/input` logs presses, releases and touches; `examples/input/tests/remote.nnhs`; ctest
  `nnh_script_pc_smoke` (`SDL_VIDEO_DRIVER=offscreen`, `--no-build`, exit 0, four PNGs of the right sizes; read them) —
  files: `examples/input/*.cpp`, `examples/input/tests/remote.nnhs`, `tools/nnh/CMakeLists.txt` — covers FR-1, FR-2,
  FR-3, FR-4
- [ ] **T4.6** [P] interactive by hand on PC: `nnh run input --pc`, type `tap A`, `screenshot x`, `expect "pressed" 30`,
  `end`; the result written in this file — covers FR-5

Verifiable: `nnh run input --pc --script examples/input/tests/remote.nnhs` → `nnh: PASS`, exit 0; ctest green.

## Stage 5 — the 3DS

- [ ] **T5.1** 3DS transport: `ctr::link_socket()`, `recv(MSG_DONTWAIT)`, flushed sends in 16 KB pieces; profile
  scope `remote` around the poll — files: `engine/platforms/3ds/src/remote_3ds.cpp`, `app_3ds.cpp`, `context_3ds.hpp`,
  `CMakeLists.txt` — test: `3ds-dev` builds clean — covers FR-1, FR-7, FR-8
- [ ] **T5.2** 3DS read-back: `linearAlloc` on the first capture, `C3D_SyncDisplayTransfer` from each target with its
  `transferFlags`, `gfxGetScreenFormat` checked, release — files: `engine/platforms/3ds/src/remote_capture_3ds.cpp` —
  test: `3ds-dev` builds clean — covers FR-2
- [ ] **T5.3** **3DS** first contact (maintainer: press Y): `nnh run input --script examples/input/tests/remote.nnhs`.
  Record: PASS/FAIL; the screenshots against the PC's (orientation, colours — the layout and BGR order of the plan;
  fix `screen_image.cpp` or the transfer if not); time per screenshot pair; the `remote` scope (poll cost) in the
  overlay with and without `Hello`. Result written in this file; apply the plan's fallbacks if the read-back or the
  timings fail — covers FR-1, FR-2, AC-2 (input), II
- [ ] **T5.4** **3DS** disconnect: Ctrl+C `nnh` while a button is held by a script → the game sees the release (log
  line from the input example); a game started without `nnh` (Homebrew Launcher only) behaves as before — covers FR-7,
  FR-8

Verifiable on PC: everything above still green; the console results recorded here.

## Stage 6 — the platformer's automatic gate (spec 012 T3.9)

- [ ] **T6.1** gate mode: A in the benchmark or `PLATFORMER_DEMO=gate`; trials (skip 30, measure 600 / 300 frames,
  pass = no missed frame), the limit searches, the log lines of the plan (≤ 160 bytes), back to the manual bench at
  the end — files: `examples/platformer/gate.{hpp,cpp}`, `bench.{hpp,cpp}`, `main.cpp`, `CMakeLists.txt` — test:
  `PLATFORMER_DEMO=gate` in the simulator logs trials, limits and `bench: PASS ...` — covers FR-6
- [ ] **T6.2** `examples/platformer/tests/gate.nnhs` (bonus → flips → bench → gate → result, screenshots of each,
  `fail-on "bench: FAIL"`, `expect "bench: PASS" 12000`); passes with `--pc` — covers AC-1, AC-2, FR-4, FR-6
- [ ] **T6.3** **3DS** (maintainer: press Y on an Old 3DS): `nnh run platformer --script
  examples/platformer/tests/gate.nnhs`; record PASS/FAIL, the trial and limit lines, the flips screenshot against the
  PC's; write the result into spec 012's T3.9 and replace its "How to run it" with this command — covers AC-1, AC-2

Verifiable: the gate script passes in the simulator; the console result recorded here and in spec 012.

## Stage 7 — hardening

- [ ] **T7.1** Linux on VM 120: gcc build + ctest (incl. `nnh_script_pc_smoke` with the offscreen driver); clang
  ASan/UBSan for `nnh_tests` and `nnh_tool_tests`; `fuzz_remote` 60 s without a crash (record `cov:`) — covers AC-3,
  FR-4, FR-8
- [ ] **T7.2** [P] mutation check by hand on the reader, schedule, merge and runner (record killed/survived here) —
  covers VI

## Stage 8 — verification and docs

- [ ] **T8.1** `nnh help` and the header comments of the contract and internal hooks — covers AC-5
- [ ] **T8.2** [P] `docs/api/simulator.md` (`NNH_REMOTE`, the log through `nnh`), `engine/platforms/pc/README.md`,
  `docs/tech/devkitpro-3ds.md` (read-back, socket reads, measured costs), `engine/platforms/3ds` notes — covers AC-5
- [ ] **T8.3** release check, final: `scripts/check_release_strings.sh` on `build/3ds-release/examples/platformer/platformer.elf`
  and `build/pc-release/examples/platformer/platformer.exe` — covers FR-7, AC-4
- [ ] **T8.4** guide `docs/guides/nnh-tool.md`: `--script`, `--no-build`, interactive mode, the script format and
  every command, exit codes, where screenshots go — covers AC-5, FR-3, FR-5
- [ ] **T8.5** [P] new guide `docs/guides/testing-on-the-console.md`: write a script, run it in the simulator, then on
  the console with one press of Y; timing rules (frame 1, `expect` + wait, late = fail), what is repeatable, reporting
  results from a game (FR-6), screenshots; linked from the nnh tool guide, `getting-started.md` and the README's docs
  list (and the wiki sidebar through `scripts/docs_tool.py` if guides need registering) — covers AC-5
- [ ] **T8.6** [P] build budgets (edit one game `.cpp` → simulator and `.3dsx`; clean engine), before/after; ROADMAP
  baseline table — covers III
- [ ] **T8.7** [P] `docs/guides/debugging-on-3ds.md` and `docs/architecture/debugging.md` (remote section from design
  to implemented), `security.md` (the console's new reader, fuzz numbers), `platforms.md` (contract area `remote`),
  `logging.md` (records in the stream), `project-structure.md` (`examples/<name>/tests/*.nnhs`,
  `tools/nnh/src/remote/`), `decisions.md` (D28 approved), `CLAUDE.md` (commands: `nnh run --script`, `--pc
  --script`; layout) — covers AC-5
- [ ] **T8.8** `docs/ROADMAP.md` checkbox; spec, plan, tasks status → implemented; `specs/README.md`; README status;
  `python -I scripts/docs_tool.py check` → 0 broken links — covers AC-5
