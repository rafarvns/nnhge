# Spec 013: Console remote — input, screenshots and test scripts from the PC

Status: approved (2026-11-21, with every proposed answer below)
Author: NNHGE maintainers · Date: 2026-11-21
Depends on: [spec 010](../010-nnh-cli-run/spec.md) (`nnh run`, the game's log on TCP 17491), [spec 003](../003-full-input/spec.md)
(input), [spec 002](../002-core-foundation/spec.md) (logs, debug tools). Comes from the spec 012 console gate: checking
a benchmark by hand on the console was too much work for the maintainer.

## Problem and goal

Testing on the console today means a person holds the 3DS, presses buttons, reads numbers off the screen and reports
them back. It is slow, error-prone, and it can't be done by a contributor without a console at hand or by an automated
agent. Yet a Development build already keeps a connection to the PC for its log. After this spec the PC drives a
Development game over that connection: it **presses buttons and touches the screen**, **takes screenshots of both
screens**, and runs **test scripts** that do both and check the log — so the person only starts the game (Homebrew
Launcher → Y) and the PC does the rest. The same scripts run in the simulator, so a test written once checks PC and
3DS.

## User stories

- As the maintainer, I want to press Y on the console once and let the PC (or an agent) run the whole console test,
  so that I don't have to read and report numbers.
- As a game developer, I want to send button presses and touches from the PC to my game on the console, so that I can
  reproduce a bug step by step.
- As a game developer, I want screenshots of both console screens saved on the PC, so that I can see what the
  console shows without a camera.
- As a game developer, I want a small test script (wait, press, hold, touch, screenshot, expect a log line) that runs
  the same on the console and in the simulator, and tells me pass or fail.
- As a player, I want Release builds to have none of this.

## Functional requirements

- **FR-1 Remote input.** While a Development (or Debug) game is connected to `nnh run`, the PC can send input: press,
  release, hold for N frames and tap any button; set the Circle Pad and C-Stick; touch, drag and release on the bottom
  screen. Remote input is merged with the console's own input before the frame's input is read, so the game sees it
  exactly like real input (queries and events, spec 003) and needs no change. Inputs apply on a given frame, so a
  script is repeatable.
- **FR-2 Screenshots.** The PC can ask for a screenshot; the console sends both screens of the next finished frame
  (top 400×240, bottom 320×240, as the player sees them), saved on the PC as PNG next to the log in `build/logs/`, with
  the frame number in the name. The game keeps running; the frame taking the screenshot may be slower, and that frame
  is marked in the log so benchmarks can ignore it.
- **FR-3 Test scripts.** A plain text script file, one command per line: wait N frames, press/release/hold/tap a
  button, move a stick, touch/drag/release, take a screenshot, expect a log line (text or pattern) within N frames,
  fail if a log line appears, and end. `nnh run <game> --script <file>` runs it after the game connects and exits with
  0 on pass and non-zero on fail, printing which line failed. Comments and blank lines are allowed. Errors in the
  script are reported before anything is sent.
- **FR-4 Same scripts in the simulator.** `nnh run <game> --pc --script <file>` runs the same script against the
  simulator with the same results (same frames, same inputs, screenshots of the same size), so console tests can be
  written and debugged on the PC first.
- **FR-5 Interactive use.** Without a script, `nnh run` can take commands typed in its terminal (the script commands,
  one per line), for exploring a bug by hand.
- **FR-6 Game results in the log.** A game (or example) can report a test result as a log line the script expects
  (e.g. `bench: PASS 4 layers + 300 sprites, worst 16.9 ms`); `examples/platformer`'s benchmark gains an automatic mode
  that runs its gate and logs PASS or FAIL. (The engine needs nothing new for this beyond logs.)
- **FR-7 Development only, PC that started it.** Remote input and screenshots exist only in Debug and Development
  builds; a Release build has no code for them (checked by the release strings test). Only the PC the game connected
  to for its log can drive it (the console opens the connection), and a game without that connection behaves as today.
- **FR-8 Bounded and safe.** Everything the console reads from the PC is bounded and validated (fuzz-tested); a
  malformed or unknown message is ignored and logged, never a crash. Remote input never blocks the frame; the PC
  disconnecting ends remote input (every remote button released).

## Acceptance criteria

- **AC-1** On an Old 3DS, the maintainer starts `examples/platformer` once (HBL → Y); `nnh run platformer --script
  examples/platformer/tests/gate.nnhs` drives it through the bonus level, the benchmark and the flip test, saves
  screenshots of each, and ends with PASS or FAIL — with nobody touching the console after Y. The spec 012 gate (T3.9)
  is then run this way.
- **AC-2** The same script passes with `--pc`; screenshots from PC and console are the same size and show the same
  screens.
- **AC-3** Unit tests: the script parser (every command, errors with line numbers), the input merge (remote and real
  input together, hold/tap timing by frame, release on disconnect), the message codec; the console-side reader is a
  fuzz target, run under ASan/UBSan on Linux.
- **AC-4** `scripts/check_release_strings.sh` shows no remote-control code in a Release build.
- **AC-5** Docs: the script format and commands in the nnh tool guide, a "testing on the console" guide, the
  debugging guide updated, CLAUDE.md commands.

## Non-goals

- Driving Release builds, or games not started by `nnh run` (NNH Link-started games may come later).
- Starting a game from the PC without anyone pressing Y in the Homebrew Launcher (possible later through NNH Link).
- Video streaming or a remote display at full frame rate (screenshots only).
- Recording a player's session for replay (a later feature can reuse the input path).
- Remote control over the internet; a remote debugger (breakpoints, memory).

## Constraints

- **I** — the game's code doesn't change; remote input enters through the platform input path.
- **II** — no frame-time cost when nobody sends anything; screenshot buffers allocated only when asked (or at
  connect), within the Development memory budget.
- **V** — no per-frame heap; nothing blocks the frame.
- **VI** — parser, merge and codec test-first; console behaviour checked on the console by the script itself.
- **X** — the console reads untrusted bytes: bounded, fuzzed; Development only.
- **XI** — games don't have to do anything to be driven; using scripts is optional.

## Open questions

- [x] Script file extension and syntax style — **answer:** `.nnhs`, one command per line like `wait 60`, `press A`,
      `hold SELECT 10`, `touch 160 120`, `screenshot bench`, `expect "bench: PASS" 900`.
- [x] Where screenshots go — **answer:** `build/logs/<game>-<date>/frame-<n>-<name>-{top,bottom}.png`, next to the log.
- [x] Does the simulator take part through the same connection (it already can send its log to `nnh run --pc`?) or
      through its own path — **answer:** whatever the plan finds simplest, as long as FR-4's results match.
