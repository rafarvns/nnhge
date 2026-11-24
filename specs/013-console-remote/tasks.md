# Tasks 013: Console remote — input, screenshots and test scripts from the PC

Status: implemented (2026-11-21): stages 1–5 done on the Old 3DS XL; stages 6–7 (the gate passed on the Old 3DS XL;
Linux, sanitizers, fuzzing); stage 8 (verification and docs)
Spec: [spec.md](spec.md) · Plan: [plan.md](plan.md)

Each task: one or a few TDD loops. `[P]` = can be done in parallel with the previous task. Every stage ends with
something verifiable on PC (`ctest --preset pc-debug`, a script run in the simulator). Console steps are marked
**3DS**: the agent runs them with `nnh run`; the maintainer's only step is to open the Homebrew Launcher and press
**Y** when asked.

## Stage 1 — wire format and the console-side logic (pure)

- [x] **T1.1** wire constants, the 16-byte message (encode/decode, every validation rule: magic, type, ranges, button
  mask, disjoint masks, reserved bytes), the record header (encode/decode, length caps), `static_assert` that the
  button mask matches `nnh::Button` (in a core file, not in the pure one) — files: `engine/core/src/remote/remote_wire.{hpp,cpp}`,
  `engine/core/CMakeLists.txt` — test: `tests/remote/test_remote_wire.cpp` — covers FR-8, FR-1
  - Done (22 tests). A non-zero reserved byte and a `Hello` of another version or with flags are `Rejected` with
    reason `range` (no new reason). `resolve_frame()` ("frame 0 = the frame now starting") lives here, shared by the
    reader and the schedule. The `static_assert` is in `remote_input.cpp` (T1.4), the core file that merges into
    `nnh::Button` bits. Payload encoders for the records come with the session (stage 2); only the header and the
    payload sizes are here.
- [x] **T1.2** reader: bytes → messages across partial reads, `Hello` first, frame order, malformed counting, 8 in a
  row → off, reads bounded by the room given — files: `remote/remote_reader.{hpp,cpp}` — test: `test_remote_reader.cpp`
  — covers FR-8
  - Done (18 tests). `bytes_wanted(room)` = room × 16 minus a partly read message; `feed()` writes at most
    `capacity` items and stops before the byte that would complete one more. Rules: off → `too many` for anything;
    `Hello` is accepted on arrival (attaches, its frame isn't ordered); data before `Hello` → `not attached`; order is
    checked on the resolved frame and only accepted messages move it; `Bye` detaches the reader on arrival (when it
    takes effect is the session's choice, stage 2). Every rejection, including before `Hello`, counts toward the 8.
- [x] **T1.3** [P] schedule: 64-entry FIFO, due messages for a frame, frame 0 = next frame, late (wanted, applied),
  full → refused — files: `remote/remote_schedule.{hpp,cpp}` — test: `test_remote_schedule.cpp` — covers FR-1, FR-8
  - Done (10 tests). Frame 0 is due on the frame that queued it (the frame whose start reads it: it can still be
    applied before that frame's input). `is_late(message, applied)` = wanted < applied; the message keeps its
    wanted frame for the `Late` record.
- [x] **T1.4** [P] remote input: apply `Buttons`/`Stick`/`Touch`, merge into an `InputSample` (OR, further-pushed stick,
  remote touch wins), release all — files: `remote/remote_input.{hpp,cpp}` — test: `test_remote_input.cpp` — covers FR-1
  - Done (12 tests). Sticks compare squared lengths; a tie keeps the real stick. A remote release never hides a real
    press (OR after the remote state is updated).
- [x] **T1.5** fuzz target `remote_target` (reader → schedule → input over simulated frames; a decoded message must
  encode back to the same 16 bytes) — files: `tests/fuzz/fuzz_targets.{hpp,cpp}`, `test_fuzz_smoke.cpp`,
  `libfuzzer_remote.cpp`, `tests/CMakeLists.txt` — test: mini-fuzz in `nnh_tests` — covers FR-8, AC-3
  - Done (`remote_target_survives_mutated_inputs`, seeded with a valid session). Input format: a control byte per
    chunk (bits 0–5 bytes that arrive, capped by the queue's room; bits 6–7 frames that pass), then the bytes. Aborts
    on: a round trip that changes bytes, a bounded read that doesn't fit, a message applied early or out of order, a
    merged sample outside the contract. libFuzzer run on VM 120: T7.1.
  - Mutation check (by hand, 2026-11-21; stage 7's T7.2 repeats it for the session and runner): 44 mutants of
    `remote_wire`, `remote_reader`, `remote_schedule` and `remote_input` (validation rules, record caps, bad count and
    its reset, order, frame 0, detach, bounded reads, due `>`/`>=`/`+1`, late `<`/`<=`, full, ring advance, stick
    `>`/`>=`/`<`, OR, release, touch, C-Stick, scale) — 43 killed; the survivor (`down = a[0] == 1` → `!= 0`) is
    equivalent, since the byte is validated to be 0 or 1.

Verifiable: `nnh_tests remote` and `nnh_tests fuzz` green on PC. Done: 63 new tests, all green (`nnh_tests remote_`
runs them all, the mini-fuzz included; the fuzz tests' names end in `_target_survives_mutated_inputs`, so the filter
is `target`, not `fuzz`).

## Stage 2 — the session in core, over the fake platform

- [x] **T2.1** contract `nnh/platform/remote.hpp`; fake implementation (connect, push bytes, disconnect, recorded
  sends, synthetic captures, capture failure); conformance: `remote_receive` is −1 without a connection — files:
  `engine/core/contract/nnh/platform/remote.hpp`, `engine/platforms/fake/src/remote_fake.cpp`, `fake_internal.hpp`,
  `include/nnh/platform_fake.hpp`, `CMakeLists.txt`, `README.md`, `tests/platform_conformance/test_contract.cpp` —
  covers FR-7, FR-8
  - Done (2 conformance tests, fake and pc). Fake controls: `remote_connect()`, `remote_push()`, `remote_disconnect()`
    (pushed bytes are still read, then −1), `remote_sent()`/`remote_clear_sent()`, `set_remote_capture_fails()`,
    `remote_capture_pixel()` (the synthetic pattern); `state()` counts receive calls, the last read size and the held
    capture buffer. The fake refuses a capture while a frame is open (the contract reads "the frame that just ended"),
    checked by a second conformance test. pc and 3ds got "not available" stubs (`remote_pc.cpp`, `remote_3ds.cpp`:
    receive −1, send false, capture fails) until stages 4 and 5.
- [x] **T2.2** session hooks: attach on `Hello` (reply with frame, capabilities, flavor), ticks (after `Hello`, then
  every 4th frame), apply due messages, `Late`/`Rejected`/`Dropped` records, warnings rate-limited (channel `remote`),
  disconnect → release all and stop polling, `Bye` (and `quit`) — files: `remote/remote_session.cpp`,
  `remote/remote_internal.hpp`, `log/engine_channels.*` — test: `test_remote_session.cpp` (incl. no `Hello` → nothing
  sent) — covers FR-1, FR-7, FR-8
  - Done (14 session tests + 9 record tests). The record payload encoders went into the pure `remote_wire.*` (the tool
    decodes the same layouts in stage 3), tested byte by byte in `test_remote_records.cpp`. Rules settled here (plan
    updated): records go out only while attached (Hello → Bye, off or disconnect), so a message before `Hello` is only
    a warning on the console, and after 8 bad messages in a row the console sends that 8th `Rejected`, releases every
    remote control and goes quiet (`too many` appears in the log, not on the wire); each frame reads at least one
    message even with a full queue, so `Dropped` is reachable and a closed connection is still noticed. Warnings: one
    line per 60 frames with "(N more since the last warning)"; "remote input off" always.
- [x] **T2.3** wire into the frame loop (`if constexpr (build::kDevToolsEnabled)`): `remote_frame_start` between
  `read_input` and `input_update`, `remote_frame_ended` after `end_frame`, quit makes `frame()` return false, reset and
  shutdown — files: `engine/core/src/app/app.cpp` — test: `test_remote_frames.cpp` (press stamped for frame 10 seen by
  `pressed()` and `ButtonPressed` on frame 10 only; hold; real + remote; D-Pad mirror from a remote stick; touch
  events; disconnect publishes `ButtonReleased`) — covers FR-1, FR-8
  - Done (9 tests). `remote_frame_start(frame_count() + 1, …)` right after `read_input`; `remote_frame_ended` inside
    `close_frame()` right after `platform::end_frame()` (so the last frame's screenshots also go out at shutdown);
    `remote_reset()` in `init()`, `remote_shutdown()` before `platform::shutdown()`. A quit returns false before
    `input_update`, like `poll_events()`.
- [x] **T2.4** screenshots: due at the end of their frame, only the screens asked, `Screen` records (layout, size,
  frame), `ScreenFailed`, the "time is not representative" log line, buffer released at `Bye`/disconnect/shutdown —
  files: `remote/remote_screenshot.cpp` — test: `test_remote_screenshot.cpp` — covers FR-2
  - Done (9 tests). The platform's answer is checked (`size == width × height × bpp`, a known layout, fits a record)
    before sending, else `ScreenFailed` and a warning. A late `Screenshot` is taken at the end of the frame it arrives
    on, and its `Screen` record carries that frame. The log line is logged in the screenshot's own frame (`[f<n>`).
- [x] **T2.5** Release check, first pass: `pc-release` builds; add the remote strings and symbol names to
  `scripts/check_release_strings.sh` and run it on `build/pc-release/examples/hello/hello.exe` — covers FR-7, AC-4
  - Done. Forbidden now: the remote's log texts and `remote_frame_start|remote_frame_ended|remote_receive|remote_send|
    remote_capture|screenshots_send`. `ok` on `pc-release` `hello.exe` (276 599 bytes; it keeps symbols, so the names
    count) and on `3ds-release` `hello.elf`; the same check on `pc-debug` `hello.exe` fails listing every one (control).
    The Release build first warned "unused variable `ms`" (the Info log compiles out): fixed with `[[maybe_unused]]`.

Mutation check (by hand, 2026-11-21): 38 mutants of `remote_session.cpp`, `remote_screenshot.cpp`, the hooks in
`app.cpp` and the record encoders (hook order both ways, frame number ±1, no reset/shutdown, quit ignored, tick
cadence `4→3`/`>=→>`/no Hello tick, no `Late`/wanted = applied, no `Rejected`, warning limit `<→<=`/never limited/no
count, off without release, no `Dropped`, reads not bounded / none when full, disconnect without release or still
polling, `Bye` without quit/release, no merge, records before `Hello`, capabilities 0, screenshot frame +1 / no log
line / always top / screens swapped / no release / pending `>=` / pixels never ok / no `ScreenFailed` / no pixels,
`Late` fields swapped, `Screen` length without its fixed part) — 38 killed. The one that first survived (`remote_frame_ended`
before `end_frame`) led to the fake's "no capture while a frame is open" rule above.

Verifiable: `nnh_tests` green; the release strings check passes on `pc-release`. Done: 41 new tests (816 in
`nnh_tests`, 16 in each conformance run), ctest 102/102, `pc-release`, `3ds-release` and `3ds-dev` build without
warnings, `examples/hello` in the simulator unchanged.

## Stage 3 — the tool's logic (pure)

- [x] **T3.1** stream splitter: log lines and records from arbitrary chunks, `\r` stripped, text fallback on bad
  headers, 1 MiB cap, a partial line before a record — files: `tools/nnh/src/remote/game_stream.{hpp,cpp}` (+ core's
  `remote_wire.cpp` in `tools/nnh/CMakeLists.txt`) — test: `tools/nnh/tests/test_game_stream.cpp` — covers FR-2, FR-8
  - Done (13 tests). Record payloads are decoded by a file of their own, `remote/game_records.{hpp,cpp}` (the tests
    build every record with core's encoders, so both ends agree). A false start (`0x1E` without a valid header) is
    text and the 7 bytes after it are looked at again, so a real record right behind it is still found; an unfinished
    record at the end of the stream is shown as text. `nnh_tool_lib` has `engine/core/src` on its include path.
- [x] **T3.2** [P] log matching: patterns (`*`, `?`, escapes, contains), frame and level from the `[f<n>` prefix, lines
  without a prefix — files: `remote/log_match.{hpp,cpp}` — test: `test_log_match.cpp` — covers FR-3
  - Done (7 tests). Frame and level come from engine lines only (log_view's `parse_log_line`); a frame over u32 or
    any other text has none. `\\` is a literal backslash in a pattern too.
- [x] **T3.3** script parser: every command and button, case, comments, blank lines, quotes and escapes, every error
  as `file:line: what` (all of them, limits, after `end`), the warning for input right after `expect` — files:
  `remote/script.hpp`, `remote/script_parser.cpp` — test: `test_script_parser.cpp` — covers FR-3
  - Done (19 tests). Words and values are in `remote/script_tokens.{hpp,cpp}` (the parser stays at 212 lines).
    `parse_script_line()` with a `ScriptParser` state is also what interactive mode uses. Details settled in the plan
    ("Settled in stage 3").
- [x] **T3.4** timeline: steps → frame-stamped messages (clock from frame 1; hold, tap = +2, drag one step per frame,
  release all, screenshots), split at `expect` barriers — files: `remote/script_timeline.{hpp,cpp}` — test:
  `test_script_timeline.cpp` — covers FR-1, FR-3
  - Done (11 tests). Segments hold offsets from their first frame (the runner adds the base: 1, then the matching
    line's frame + 1). `hold B N`: press at +0, release at +N; `tap`: +0/+1; `drag X Y N`: steps at +1…+N (the frame
    of the touch itself keeps the starting point); `release all`: buttons, both sticks centred, touch up. `Timeline`
    remembers the touch point across segments and typed lines (`take_open()` for interactive mode). Changed in
    stage 5 (maintainer, 2026-11-21): `screenshot` moves the clock +1 and `drag` +N+1 (plan, "Settled in stage 5").
- [x] **T3.5** runner against a fake console: ≤ 48 unapplied, barrier, expect window and timeout (decided at the tick),
  clock = line frame + 1, `fail-on`, level F, late/rejected/dropped → fail with the line, game closes early, missing
  capability before any input, `end` waits for ticks and screenshots, file names, verdict and exit codes; interactive
  mode (lazy `Hello`, now + 6, expect never ends the run, `end` detaches) — files: `remote/script_runner.{hpp,cpp}` —
  test: `test_script_runner.cpp` — covers FR-3, FR-5
  - Done (21 script-mode tests + 12 interactive in `test_script_interactive.cpp`). The interactive half is
    `remote/script_interactive.cpp`. The fake console (`tests/fake_console.hpp`) plays the console frame by frame —
    Hello answer, ticks every 4th frame, due messages with `Late`, screenshots at the end of the frame, a game that logs
    "pressed <mask>" — and its records go through `GameStream`, as in a real run. Changed from the plan: the `expect`
    window starts at the segment's first frame (the plan's own example needs it); see the plan, "Settled in stage 3".
    `no_answer()` is the session's 3 s timeout (exit 2); a protocol version mismatch is exit 2 too.
- [x] **T3.6** [P] screen images and PNG: both layouts → RGB rows (known pattern), wrong sizes refused; RGB → PNG with
  stored zlib and core's CRC-32, decoded back in the test — files: `remote/screen_image.{hpp,cpp}`,
  `remote/png_writer.{hpp,cpp}`, `tools/nnh/CMakeLists.txt` (`crc32.cpp`) — test: `test_screen_image.cpp`,
  `test_png_writer.cpp` — covers FR-2
  - Done (4 + 3 tests). A screen must be the console's (top 400×240, bottom 320×240), in layout 1 or 2, with exactly
    w × h × bpp bytes. The PNG test checks IEND against its well-known CRC (`AE426082`), independent of `crc32.cpp`.
    Layout 2's orientation and colour order are confirmed on the console in T5.3.
- [x] **T3.7** [P] tool mini-fuzz: stream splitter and parser on mutated inputs — files: `tools/nnh/tests/test_fuzz_tool.cpp`
  — covers FR-8, AC-3
  - Done (`stream_target_survives_mutated_inputs`: 4000 mutated streams, the result doesn't depend on how the bytes
    are chunked and nothing is invented; `script_target_survives_mutated_inputs`: 6000 mutated scripts, every error is
    `file:line:`, every accepted value within its limits, timeline offsets never go back).

Mutation check (by hand, 2026-11-21): 75 mutants of the stream, records, matcher, tokens, parser, timeline, runner
(both modes), screen image and PNG writer. First pass: 65 killed. Of the 10 survivors, 2 were redundant code, now
removed (a decimal-count check that `parse_whole` already makes; the parser advancing its "since expect" clock on
inputs, which only `wait` can affect); the other 8 got tests (a 12-character flavor, a comment right after a closing
quote, the send window exactly 48 after a tick, an unasked-for `Hello` in interactive mode, a typed `expect`'s window
end, its timeout tick and that a match ends it, a wrong screen height). Final: 73 of 73 killed.

Verifiable: `nnh_tool_tests` green. Done: 92 new tests (138 in `nnh_tool_tests`), `nnh_tests` 816/816, ctest 102/102,
`pc-release` builds without warnings, `3ds-release` unchanged.

## Stage 4 — the simulator end to end

- [x] **T4.1** PC transport: `NNH_REMOTE` (loopback only, ignored otherwise and in Release), connect with a 1 s timeout,
  non-blocking receive, send; the log goes to the socket while connected — files:
  `engine/platforms/pc/src/remote_pc.cpp`, `log_pc.cpp`, `app_pc.cpp`, `context_pc.hpp`, `CMakeLists.txt` (`ws2_32`)
  — test: conformance on pc (non-loopback ignored, −1 without a connection) — covers FR-4, FR-7
  - Done. Extra files: `remote_pc.hpp` (Winsock stays out of SDL's headers) and the pure parser
    `remote_address_pc.{hpp,cpp}` (3 tests in `nnh_tests`, `tests/simulator/test_remote_address.cpp`). PC-only
    conformance in `tests/platform_conformance/test_remote_pc.cpp` (the test plays nnh with `nnh_link` sockets):
    an address off this computer is ignored; connected → receive 0 without data, bounded reads, bytes both ways,
    the log line through the socket in order and uncoloured (and not on stdout), a 1 MiB send arrives whole, −1 and
    no send after nnh closes; the connection closes at shutdown; nobody listening → no connection. In `pc-release` the same test expects no connection.
    `TCP_NODELAY` on both ends (`net::Socket::set_no_delay()` added to `link/`, 1 test). Details in the plan,
    "Settled in stage 4".
- [x] **T4.2** [P] PC read-back of the screen targets (RGBA rows) — files: `engine/platforms/pc/src/remote_capture_pc.cpp`
  — test: a pc conformance test reads back a cleared colour — covers FR-2, FR-4
  - Done (`pc_remote_capture_reads_each_screen_back_as_the_frame_drew_it`: both cleared colours, sizes, layout 1,
    and a 4×3 mark at (10, 20) found where it was drawn, so rows are top-down and not mirrored). The existing
    "no capture while a frame is open" conformance test now runs against a real check (`frame_open`).
- [x] **T4.3** the session loop: socket + stream + runner + stdin thread, lines printed and saved, `nnh:` progress
  lines in the log file, PNGs in `build/logs/<game>-<stamp>/`; `show_game_log` (also used by `nnh serve`) on top of it,
  without stdin for serve — files: `tools/nnh/src/remote/game_session.{hpp,cpp}`, `stdin_lines.{hpp,cpp}`,
  `game_log.*`, `log_listener.*`, `repo.*` — test: `test_repo.cpp` (screenshot folder), the loop by T4.5 — covers
  FR-2, FR-3, FR-5
  - Done. The loop also has tests of its own over loopback, against a fake game that speaks the real bytes
    (`tools/nnh/tests/fake_game.hpp`, `test_game_session.cpp`, 7 tests: plain log shown and saved; a script to
    PASS with its PNGs, notes in the log file, files noted before the verdict and the log after it; no Hello answer
    → 2; the game closing early → 1; typed commands; a typed command nobody answers leaves the log running;
    nothing typed → no Hello and no timeout).
    `test_stdin_lines.cpp` (3), `test_repo.cpp` (+2: screenshot folder, `NNH_PRESET`). `LogListener::read_lines`
    became `take_game()` (its test in `test_console_link.cpp` changed with it).
- [x] **T4.4** CLI and commands: `--script`, `--no-build`, `--script` + `--release` refused, parse before building,
  console path (`Hello`, 3 s answer, `Bye`), `--pc` path (listen on loopback, `NNH_REMOTE`, simulator on a thread,
  `Bye{quit}`, join), help text — files: `tools/nnh/src/cli.{hpp,cpp}`, `commands/run.cpp`, `commands/run_pc.cpp` —
  test: `test_cli.cpp` — covers FR-3, FR-4
  - Done (+3 tests: options, where scripts are refused, `closes_game_at_end()`). Only a script with `--pc` sends
    `Bye{quit}`; first written as "any `--pc` run", which closed the simulator at a typed `end` — found by T4.6.
    `NNH_PRESET` is new (plan, "Settled in stage 4"). Not run on a console yet: the console path is stage 5's T5.3.
- [x] **T4.5** `examples/input` logs presses, releases and touches; `examples/input/tests/remote.nnhs`; ctest
  `nnh_script_pc_smoke` (`SDL_VIDEO_DRIVER=offscreen`, `--no-build`, exit 0, four PNGs of the right sizes; read them) —
  files: `examples/input/*.cpp`, `examples/input/tests/remote.nnhs`, `tools/nnh/CMakeLists.txt` — covers FR-1, FR-2,
  FR-3, FR-4
  - Done. New `examples/input/press_log.{hpp,cpp}` ("pressed A", "released A"; also the button names and the `demo`
    channel), `touch_log.cpp` logs "touch started at x, y" / "touch ended at x, y". The smoke test is
    `tools/nnh/tests/script_pc_smoke.cmake` (exit 0, `PASS`, four PNG headers 400×240 / 320×240), ~3 s.
  - Result: `nnh: PASS (24 lines, frames 1-110)`, exit 0. Screenshots read: `stick-right` (frame 77) top shows the
    Circle Pad at x +1.00, bottom "not touching"; `dragged` (frame 108) bottom shows "touching at 280, 200" with the
    cursor there, top the sticks centred. Same colours, orientation and text as an `NNH_SCREENSHOT` of the example
    with `NNH_SIM_INPUT="circle=1,0"`.
  - **Open question (timeline semantics)**: the plan's example script, run as written, also passes, but its
    screenshots were wrong: `stick-right` showed the stick centred and `dragged` showed "not touching", and the touch
    ended at 272, 194. Commands on the same clock all apply at the start of that frame, and the screenshot shows its
    end, so the commands *after* a screenshot on the same clock are already in it; the last `drag` step shares its
    frame with what follows. The script now has `wait 1` after each screenshot (and expects "touch ended at 280,
    200"). Changing the timeline instead (stage 3's behaviour) is the maintainer's call.
  - **Resolved in stage 5** (maintainer, 2026-11-21): the timeline changed (a `screenshot` takes a frame, a `drag`
    holds its target one frame) and the `wait 1` lines are gone. The script as in the plan: `nnh: PASS (19 lines,
    frames 1-111)`; `stick-right` (frame 77) top shows x +1.00, `dragged` (frame 109) bottom "touching at 280, 200";
    "touch ended at 280, 200" on frame 110. Tests: `timeline_screenshot_takes_a_frame_so_what_follows_is_not_in_it`,
    `timeline_drag_holds_its_target_one_frame_before_what_follows`, `runner_sends_what_follows_a_screenshot_for_the_next_frame`
    (+ four timeline/runner tests adjusted to the new offsets). Mutation check by hand: 7 mutants of the change
    (no advance, +2, capture on the next frame, drag +N, drag +N+2, drag steps a frame late, `wait` +1) — 7 killed.
- [x] **T4.6** [P] interactive by hand on PC: `nnh run input --pc`, type `tap A`, `screenshot x`, `expect "pressed" 30`,
  `end`; the result written in this file — covers FR-5
  - Done by an agent, so the lines were **piped into stdin, not typed** (`NNH_PRESET=pc-debug`, `--no-build`,
    headless, `NNH_EXIT_AFTER_FRAMES` so the simulator closes by itself):
    - Paced like typing (`sleep 5`, the first three lines, `sleep 2`, `end`): `Hello` on the first command (frame
      168), `pressed A` on frame 175, `nnh: line 3: expect "pressed" matched at frame 175`, both PNGs of `x` saved
      (frame 176, A released), `nnh: detached: …` at `end`, then the log until the game closed at frame 600, exit 0.
    - All four lines at once (or before the game is up, which takes ~2 s here): every line waits for the `Hello`
      answer, then `end` drops the tap, screenshot and expect not sent yet (stage 3's rule), so only the detach
      happens. Expected for typed use; a person types after the game shows up.
    - The first run found a bug: `end` closed the simulator (`Bye{quit}`). Fixed (T4.4).
    - Still to do by hand on a real terminal (and with a gamepad: `SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT` if the
      stick drifts): typing while the log scrolls.

Mutation check (by hand, 2026-11-21): 69 mutants of the session loop (`game_session.cpp`: loop and linger ends,
exit code, log file, note prefix, Hello timeout and its condition, close, stream end, records, lines and their colour,
sends, screenshot/notes order, folder, path, write result, first flush), `stdin_lines.cpp` (`\r`, last line, partial
reads, end, pop, EOF), the CLI and repo helpers (`--script`/`--no-build` placement and values, Release refusal,
`closes_game_at_end`, `NNH_PRESET` checks, screenshot folder), `net::Socket::set_no_delay`, and the PC transport
(address parser, receive poll/close/bound, log line, connect check, blocking mode, `TCP_NODELAY`, read-back frame
check/layout/size/screen/format/target restore, the `app_pc`/`log_pc`/`video_pc` hooks). First pass: 59 killed. Of the
10 survivors, 6 got tests (a 6-digit port, a 1 MiB send that needs the blocking socket, a log line that must not also
reach stdout, an interactive session with nothing typed must not report "didn't answer", the screenshot notes before
the verdict — the fake game now ticks before sending screens, as the console does — and `set_no_delay`, whose test was
in `nnh_link_tests`, not in the group run). Final: 65 of 69 killed; the 4 left change nothing observable: the
simulator's `TCP_NODELAY` (latency only), restoring the render target after a read-back (the next `begin_frame` sets
it), closing the socket on a failed send (the line still falls back to the terminal), and the flush right after
`start()` (the Hello goes out on the next loop pass, ≤ 20 ms later).

Verifiable: `nnh run input --pc --script examples/input/tests/remote.nnhs` → `nnh: PASS`, exit 0; ctest green.
Done: `nnh_tests` 819 (+3), `nnh_tool_tests` 153 (+15), `nnh_link_tests` 55 (+1), `nnh_conformance_pc` 23 (+6), ctest
103/103 on `pc-debug` (+1, `nnh_script_pc_smoke`); `pc-dev`, `pc-release`, `3ds-release` and `3ds-dev` build without
warnings (a GCC 16 `-O2` false positive in stage 3's `test_game_stream.cpp` fixed); the release strings check (now
also `NNH_REMOTE` and the simulator's remote function names) passes on `pc-release` and `3ds-release` `hello` and
`input`. Found and fixed (from stage 2): in a **Release** build of `nnh_tests` the session, frames and screenshot tests
crashed or failed (they test code Release compiles out); `tests/CMakeLists.txt` now builds them only outside
Release, and `ctest --preset pc-release` passes 102/102.

## Stage 5 — the 3DS

- [x] **T5.1** 3DS transport: `ctr::link_socket()`, `recv(MSG_DONTWAIT)`, flushed sends in 16 KB pieces; profile
  scope `remote` around the poll — files: `engine/platforms/3ds/src/remote_3ds.cpp`, `app_3ds.cpp`, `context_3ds.hpp`,
  `CMakeLists.txt` — test: `3ds-dev` builds clean — covers FR-1, FR-7, FR-8
  - Done (built, not yet run on a console). `recv` 0 bytes or an error → −1 (closed, remembered in `ctr::Context`);
    `EAGAIN`/`EWOULDBLOCK`/`EINTR` → 0. `TCP_NODELAY` at the first remote send (after `Hello` only). The profile
    scope is in core, `remote_session.cpp` (platforms never call core), with a test:
    `remote_frames_time_the_poll_as_remote_in_the_profiler` (seen failing without the scope). Every function returns
    at once unless `kDevToolsEnabled`; Release has no log socket anyway.
- [x] **T5.2** 3DS read-back: `linearAlloc` on the first capture, `C3D_SyncDisplayTransfer` from each target with its
  `transferFlags`, `gfxGetScreenFormat` checked, release — files: `engine/platforms/3ds/src/remote_capture_3ds.cpp` —
  test: `3ds-dev` builds clean — covers FR-2
  - Done (built, not yet run on a console). Also `video_3ds.cpp`: `ctr::Context::frame_open` (the read-back refuses
    during a frame, as on PC). Checks before the transfer: the target is 240 × 400/320, its `transferFlags` give
    linear RGB8, the screen is `GSP_BGR8_OES`; `GSPGPU_InvalidateDataCache` after it. Sends layout 2, 400/320 × 240,
    pixel (x, y) at `(x × 240 + 239 − y) × 3` as B, G, R (what `screen_image.cpp` reads; plan, "Settled in stage 5").
  - Builds: `3ds-dev` and `3ds-release` without warnings; `check_release_strings.sh` ok on `3ds-release` `hello.elf`
    and `input.elf`; the `3ds-dev` `input.elf` links `remote_receive`, `remote_send`, `remote_capture` and
    `C3D_SyncDisplayTransfer`. `pc-debug`/`pc-release` build; `nnh_tests` 820, `nnh_tool_tests` 156, ctest 103/103.
- [x] **T5.3** **3DS** first contact (maintainer: press Y): `nnh run input --script examples/input/tests/remote.nnhs`.
  Record: PASS/FAIL; the screenshots against the PC's (orientation, colours — the layout and BGR order of the plan;
  fix `screen_image.cpp` or the transfer if not); time per screenshot pair; the `remote` scope (poll cost) in the
  overlay with and without `Hello`. Result written in this file; apply the plan's fallbacks if the read-back or the
  timings fail — covers FR-1, FR-2, AC-2 (input), II
  - Done on the Old 3DS XL (2026-11-21): `nnh: PASS (19 lines, frames 1-111)`, exit 0, nobody touching the console
    after Y. `pressed A` on frame 31, L+R held 42–62, drag ended at 280, 200 on frame 110 — the same frames as the
    simulator. The screenshots match the PC's: upright, not mirrored, same colours (layout 2 and BGR as planned, no
    fix needed); the C-Stick panel reads "not on this system". Screenshot pair: 659 ms and 622 ms (plan: 0.5–2 s, no
    RLE needed). The `remote` poll cost was not read off the overlay (console tests stay yes/no); it is measured by the
    stage 6 gate, which runs attached and fails on any missed frame.
- [x] **T5.4** **3DS** disconnect: Ctrl+C `nnh` while a button is held by a script → the game sees the release (log
  line from the input example); a game started without `nnh` (Homebrew Launcher only) behaves as before — covers FR-7,
  FR-8
  - Done (2026-11-21): a script pressed L on frame 31 with `hold L 1200`; `nnh` was killed 8 s in (`timeout`, so the
    connection dropped without `Bye`). The maintainer confirmed L was no longer lit on the top screen — released on
    disconnect. Games without `nnh` were not re-run here; NNH Link launches (spec 011) start games with no remote PC,
    and the session sends nothing before `Hello` (stage 2 tests).

Verifiable on PC: everything above still green; the console results recorded here.

## Stage 6 — the platformer's automatic gate (spec 012 T3.9)

- [x] **T6.1** gate mode: A in the benchmark or `PLATFORMER_DEMO=gate`; trials (skip 30, measure 600 / 300 frames,
  pass = no missed frame), the limit searches, the log lines of the plan (≤ 160 bytes), back to the manual bench at
  the end — files: `examples/platformer/gate.{hpp,cpp}`, `bench.{hpp,cpp}`, `main.cpp`, `CMakeLists.txt` — test:
  `PLATFORMER_DEMO=gate` in the simulator logs trials, limits and `bench: PASS ...` — covers FR-6
  - Done. The trials and searches are pure (`examples/platformer/gate_run.{hpp,cpp}`), tested first in `nnh_tests`
    (`tests/platformer/test_gate_run.cpp`, 16 tests: target length and skip, average/worst/missed, the threshold is
    "slower than", searches up to the first failure or the maximum, down to the first pass when the target failed,
    no limits, verdict = the target only, frames ignored once done). Hand mutants (`>` → `>=` on the threshold, the
    sprite maximum off by one): both killed. `gate.{hpp,cpp}` logs and draws, `bench.cpp` feeds it. Choices beyond
    the plan (downward searches on a FAIL, extra log lines, the on-screen verdict, SELECT stops a run) are in the
    plan, "Settled in stage 6".
  - `PLATFORMER_DEMO=gate NNH_EXIT_AFTER_FRAMES=9100` (pc-debug, a window at 60 Hz): target `ok, avg 16.7 ms, worst
    17.2 ms, missed 0/600`; layers up to `limit 12 layers (the most it has) with 300 sprites`; sprites 400, 500 ok,
    600 `MISSED ... worst 73.2 ms, missed 19/300` (a Windows hiccup, not the load); `bench: PASS 4 layers + 300
    sprites, avg 16.7 ms, worst 17.2 ms, missed 0 of 600 frames` at frame 4260 (71 s). Screenshot read: the top shows
    a large green `GATE: PASS` over the benchmark, the bottom the verdict, the target's numbers and both limits.
  - **The PC is not a stable 60 Hz**: one script run failed with a single 29 ms frame in the target trial (the FAIL
    path then worked as designed: down to `limit 3 layers`, `limit 200 sprites`, `W ... bench: FAIL ...`, `nnh: FAIL`
    at the `fail-on` line, exit 1). The criterion stays "no missed frame" (the console is what it is for); on PC a
    rerun with nothing else running passes.
- [x] **T6.2** `examples/platformer/tests/gate.nnhs` (bonus → flips → bench → gate → result, screenshots of each,
  `fail-on "bench: FAIL"`, `expect "bench: PASS" 12000`); passes with `--pc` — covers AC-1, AC-2, FR-4, FR-6
  - Done. `NNH_PRESET=pc-debug nnh run platformer --pc --no-build --script examples/platformer/tests/gate.nnhs` →
    `nnh: PASS (41 lines, frames 1-9149)`, exit 0, 152 s (every PC trial passes, so both searches run to the
    benchmark's maximum: 26 trials). Gate started on frame 252, `bench: PASS 4 layers + 300 sprites, avg 16.7 ms,
    worst 17.0 ms, missed 0 of 600 frames` on frame 9132. Screenshots read: `bonus` (frame 101) the player in mid-jump
    under the brick row; `flips` (146) the 8 map orientations, the 4 sprite flips and the wall; `bench` (221) the
    4 layers + 300 sprites, bottom `missed 0`, `tiles 1566 / 4992`; `gate-result` (9148) the green `GATE: PASS`
    banner on top and the verdict with `limit 12 layers (the most it has)` and `limit 2000 sprites (the most it
    has)` below. The bonus screenshot is 8 frames after the jump (at 20 the player had already landed).
  - On the Old 3DS the searches stop at the first failing count, so the run is shorter (estimate below 1 min of
    gate when the limits are near the target; the most is ~9000 frames, inside the 12 000 of the `expect`).
- [x] **T6.3** **3DS** (maintainer: press Y on an Old 3DS): `nnh run platformer --script
  examples/platformer/tests/gate.nnhs`; record PASS/FAIL, the trial and limit lines, the flips screenshot against the
  PC's; write the result into spec 012's T3.9 and replace its "How to run it" with this command — covers AC-1, AC-2
  - Done on the Old 3DS XL (2026-11-21, `3ds-dev`, nobody touching the console after Y): `nnh: PASS (41 lines,
    frames 1-3209)`, exit 0, about 1 minute. Target `ok, avg 16.7 ms, worst 17.4 ms, missed 0/600`; limits 6 layers
    with 300 sprites (7: 31 missed) and 600 sprites with 4 layers (700: 9 missed). Screenshot pairs 659–775 ms. The
    flips, bonus, bench and `GATE: PASS` screenshots are right; flips match the PC's. Recorded in spec 012's T3.9.
    AC-1 met. The remote's poll cost is inside these trials: no missed frame at the target while attached.

Verifiable: the gate script passes in the simulator; the console result recorded here and in spec 012.

## Stage 7 — hardening

- [x] **T7.1** Linux on VM 120: gcc build + ctest (incl. `nnh_script_pc_smoke` with the offscreen driver); clang
  ASan/UBSan for `nnh_tests` and `nnh_tool_tests`; `fuzz_remote` 60 s without a crash (record `cov:`) — covers AC-3,
  FR-4, FR-8
  - Done on VM 120 (2026-11-21). gcc `pc-debug`: builds (the only warnings are SDL's own Wayland XML), ctest 103/103
    with `SDL_VIDEO_DRIVER=offscreen`, including `nnh_script_pc_smoke`. clang 18 with
    `-fsanitize=address,undefined -fno-sanitize-recover=undefined`: `nnh_tool_tests` 156/156 clean; `nnh_tests` found
    one **stack-use-after-scope** in two stage-2 tests (`sent().find(...)` kept a pointer into a temporary `Sent`),
    fixed in `tests/remote/test_remote_session.cpp` (the engine code was right), then 836/836 clean.
    `fuzz_remote` (libFuzzer + ASan, empty seed corpus) 60 s: 922 651 runs, `cov: 132`, `ft: 782`, no crash.
- [x] **T7.2** [P] mutation check by hand on the reader, schedule, merge and runner (record killed/survived here) —
  covers VI
  - Done stage by stage rather than once at the end (2026-11-21), every result recorded above: reader, schedule,
    merge and wire 43/44 (the survivor equivalent); session, screenshots and hooks 38/38; stream, parser, timeline,
    runner and PNG 73/73, plus the screenshot/drag timing change 7/7; session loop, typed lines, CLI and PC transport
    65/69 (the 4 change only latency or cleanup, nothing observable); gate logic 2/2.

## Stage 8 — verification and docs

- [x] **T8.1** `nnh help` and the header comments of the contract and internal hooks — covers AC-5
  - Done. `nnh help`: typed commands (`help`, `end`), `--script` is Debug/Development only, the log and screenshot
    paths, a pointer to the new guide (`tools/nnh/src/cli.cpp`; `test_cli.cpp`'s help checks still pass);
    `tools/nnh/src/main.cpp`'s header names specs 010, 011 and 013. `nnh/platform/remote.hpp` lists the
    implementations, their costs and the guide; `remote_internal.hpp` points to the architecture overview.
- [x] **T8.2** [P] `docs/api/simulator.md` (`NNH_REMOTE`, the log through `nnh`), `engine/platforms/pc/README.md`,
  `docs/tech/devkitpro-3ds.md` (read-back, socket reads, measured costs), `engine/platforms/3ds` notes — covers AC-5
  - Done. `devkitpro-3ds.md` has the socket reads, sends, read-back (confirmed on hardware), the measured costs
    (screenshot pair 0.62–0.78 s; the gate attached with no missed frame) and the disconnect. Also fixed stale
    lines found on the way: the PC README's capabilities (the simulator has touch, both sticks and ZL/ZR), the 3DS
    README's capabilities note and `.cia` "planned".
- [x] **T8.3** release check, final: `scripts/check_release_strings.sh` on `build/3ds-release/examples/platformer/platformer.elf`
  and `build/pc-release/examples/platformer/platformer.exe` — covers FR-7, AC-4
  - Done (2026-11-21, from this tree): `ok` on `3ds-release` `platformer.elf` (3 199 176 bytes) and `pc-release`
    `platformer.exe` (503 592 bytes), and again on `hello` and `input` of both; the `pc-debug` `platformer.exe`
    fails it, listing the remote's names (control).
- [x] **T8.4** guide `docs/guides/nnh-tool.md`: `--script`, `--no-build`, interactive mode, the script format and
  every command, exit codes, where screenshots go — covers AC-5, FR-3, FR-5
  - Done: options (with `NNH_PRESET`), the format, every command with its clock, limits and parser errors, timing
    rules (frame 1, same-frame commands, tap/hold, screenshot +1, drag +N+1, the `expect` window and barrier,
    late = fail), matching, output with a real run's lines, exit codes 0/1/2 in detail, screenshots, typed commands.
- [x] **T8.5** [P] new guide `docs/guides/testing-on-the-console.md`: write a script, run it in the simulator, then on
  the console with one press of Y; timing rules (frame 1, `expect` + wait, late = fail), what is repeatable, reporting
  results from a game (FR-6), screenshots; linked from the nnh tool guide, `getting-started.md` and the README's docs
  list (and the wiki sidebar through `scripts/docs_tool.py` if guides need registering) — covers AC-5
  - Done. Also linked from `debugging-on-3ds.md` (new section 2), the simulator page, the architecture pages and
    CLAUDE.md. The wiki needs nothing: `docs/guides` is already a section of `scripts/docs_tool.py`.
- [x] **T8.6** [P] build budgets (edit one game `.cpp` → simulator and `.3dsx`; clean engine), before/after; ROADMAP
  baseline table — covers III
  - Done (before = `82f6e91`, after = this tree, separate build folders, three edits of `examples/hello/main.cpp`):
    simulator 1.66–2.07 → 1.77–2.04 s, `.3dsx` 2.97–3.05 → 2.93–2.97 s, clean 3DS engine 10.2 → 11.7 s, the rest of
    the 3DS tree 12.7 → 13.1 s; Release `hello.3dsx` +148 bytes. Recorded in the ROADMAP.
- [x] **T8.7** [P] `docs/guides/debugging-on-3ds.md` and `docs/architecture/debugging.md` (remote section from design
  to implemented), `security.md` (the console's new reader, fuzz numbers), `platforms.md` (contract area `remote`),
  `logging.md` (records in the stream), `project-structure.md` (`examples/<name>/tests/*.nnhs`,
  `tools/nnh/src/remote/`), `decisions.md` (D28 approved), `CLAUDE.md` (commands: `nnh run --script`, `--pc
  --script`; layout) — covers AC-5
  - Done. Fixed on the way in `platforms.md`: the contract folder (`engine/core/contract/`, not `include/`) and the
    log contract header's name (`log.hpp`).
- [x] **T8.8** `docs/ROADMAP.md` checkbox; spec, plan, tasks status → implemented; `specs/README.md`; README status;
  `python -I scripts/docs_tool.py check` → 0 broken links — covers AC-5
  - Done (the plan's stale bits too: D28 "proposed", `--no-build` "proposed", the read-back "not yet confirmed",
    2.2 → 3.2 KB in the closing check).

Verifiable: `ctest --preset pc-debug` 103/103 (after the help change); `nnh run input --pc --script
examples/input/tests/remote.nnhs` → `nnh: PASS (19 lines, frames 1-111)`, exit 0; release check ok; 0 broken links.

## Amendment 2026-11-24: a script's input is exclusive (maintainer-approved)

Spec FR-1 amendment and plan "Merge in core" amendment. Cause: an Old 3DS Circle Pad resting at raw (10, 14) of 156
is past the 0.10 dead zone and leaked into `examples/hierarchy` `check.nnhs:49` through the merge.

- [x] **A1** Wire: `Hello` flag bit 0 `kHelloExclusive` (`Message::exclusive`), other flag bits still `Range`;
  round trip — `test_remote_wire.cpp`
- [x] **A2** `RemoteInput::replace_into` and `rest_report` (stick past `kStickDeadZone`, buttons, touch) —
  `test_remote_input.cpp`
- [x] **A3** Session: exclusive from a flagged `Hello` until `Bye`/disconnect/off/plain `Hello`; one
  `W remote <control> not at rest (...): ignored while the script runs` per busy control at attach —
  `test_remote_exclusive.cpp`
- [x] **A4** `nnh`: scripts send the flag (typed commands don't); the console's warning becomes an `nnh:` note —
  `test_script_runner.cpp`, `test_script_interactive.cpp`
- [x] **A5** ctest `hierarchy_check_script_resting_pad_pc` (`NNH_SIM_INPUT` resting pad): FAIL at line 49 without the
  flag, PASS with it; 19 mutants on the new logic, all killed
- [x] **A6** Docs: spec, plan, `testing-on-the-console.md`, `nnh-tool.md`, `debugging.md`, ROADMAP
- [ ] **A7** Console: the hierarchy script on the Old 3DS XL passes with the resting pad, and the warning shows
