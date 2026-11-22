# Debugging tools

> Status: **implemented** (spec 002): overlay, profiler, simulator keys, crash reports; driving a game from the PC
> (spec 013, [below](#driving-a-game-from-the-pc)); step-by-step
> guide: [debugging-on-3ds.md](../guides/debugging-on-3ds.md). APIs: [debug.md](../api/debug.md), [profile.md](../api/profile.md),
> [simulator.md](../api/simulator.md).

## In-game tools (Debug and Development flavors)

| Tool | How | Shows |
|------|-----|-------|
| Debug overlay | PC: `F1` · 3DS: hold SELECT + press L | FPS, frame time graph (last 120 frames), memory per region |
| Profiler | `NNH_PROFILE_SCOPE("update");` in a scope | slowest scopes of recent frames, in the overlay |
| Debug text | `nnh::debug::print/printf` | anything, drawn on top of the frame |
| Logs | `NNH_LOG_*` | see [logging.md](logging.md) |

All of them compile out in Release.

## Simulator controls (PC only)

| Key | Action |
|-----|--------|
| `F1` | toggle overlay |
| `Pause` | pause / resume the game loop |
| `F10` | advance one frame while paused |
| `F11` | slow motion (¼ speed) toggle |
| `F12` | screenshot to `<data folder>/screenshots/` |

Plus the environment variables `NNH_EXIT_AFTER_FRAMES` / `NNH_SCREENSHOT` for automation, and the test scripts
below.

## Debuggers

**PC**: build `pc-debug` and run the game under any debugger (VS Code with the CMake Tools extension,
Visual Studio, CLion, gdb). On Windows, asserts break into an attached debugger.

**3DS**: Luma3DS includes a GDB stub.
1. Build `3ds-debug` (keeps symbols in the `.elf`).
2. On the console: Rosalina menu (L + Down + Select) → Debugger options → Enable debugger; start the game.
3. On the PC: `arm-none-eabi-gdb build/3ds-debug/examples/<ex>/<ex>.elf`, then
   `target remote <console-ip>:<port>` (the port is shown in the Rosalina debugger menu).

Full step-by-step guide: [debugging-on-3ds.md](../guides/debugging-on-3ds.md).

## Driving a game from the PC

> Status: **implemented** — [spec 013](../../specs/013-console-remote/spec.md), [plan 013](../../specs/013-console-remote/plan.md),
> decision D28; verified on an Old 3DS XL (2026-11-21). Guides: [testing on the console](../guides/testing-on-the-console.md),
> script reference in [the nnh tool](../guides/nnh-tool.md#driving-the-game-from-the-pc).

`nnh run <game> --script <file.nnhs>` presses buttons, moves the sticks, touches the screen, takes screenshots of both
screens and checks the log of a Debug/Development game, on the console or in the simulator (`--pc`), and ends with
PASS or FAIL (exit 0/1/2). Without a script, commands typed in `nnh`'s terminal do the same by hand.

How it fits together:

| Part | Where | What it does |
|------|-------|--------------|
| Channel | the game's log connection (3DS: TCP 17491 to the PC that ran `nnh run`; simulator: loopback, `NNH_REMOTE`) | the game opens it, so only that PC can drive it |
| PC → game | 16-byte messages (`Hello`, `Buttons`, `Stick`, `Touch`, `Screenshot`, `Bye`), each stamped with the frame it applies to | validated field by field; 8 bad ones in a row turn the remote off |
| Game → PC | records inside the log stream (`0x1E 'N' 'R'`, type, length): the `Hello` answer, a `Tick` every 4th frame, `Late`, `Rejected`, `Dropped`, `Screen`, `ScreenFailed` | one ordered stream, so a log line and the frame it belongs to are matched exactly ([logging](logging.md#records-in-the-log-stream)) |
| Core | `engine/core/src/remote/` (wire format, reader, 64-message schedule, input merge, session, screenshots) | each frame, between `platform::read_input()` and the input update: reads what arrived (never waits), applies the messages due, ORs remote buttons into the sample, the further-pushed stick wins, a remote touch wins; after `end_frame()`, sends the screenshots asked for |
| Platforms | contract `nnh/platform/remote.hpp`: `remote_receive`, `remote_send`, `remote_capture`, `remote_capture_release` ([platforms](platforms.md)) | move bytes and read the screens back; nothing else |
| `nnh` | `tools/nnh/src/remote/` (stream splitter, script parser, timeline, runner, log matching, PNG) | turns a script into frame-stamped messages, matches log lines, writes PNG files, gives the verdict |

Costs (Development only; Release has none of this code, checked by `scripts/check_release_strings.sh`): ≈ 3.2 KB of
BSS; per frame, one non-blocking socket read while a PC is connected (on an Old 3DS XL the tile gate ran attached
with no missed frame); a screenshot pair costs a 288 000-byte linear buffer on the 3DS while screenshots are used and
0.6–0.8 s on the console over Wi-Fi, a frame the log marks as not representative. When the PC leaves, every remote
control is released.

## Choosing a tool

| Symptom | First tool |
|---------|------------|
| Wrong value, logic bug | logs, then a debugger on PC |
| Works on PC, fails on 3DS | logs over 3dslink, crash report, then GDB on the console |
| Slow on 3DS | overlay frame time → profiler scopes |
| Running out of memory | overlay memory per tag → leak report at exit |
| Visual glitch | simulator pause + frame step, screenshot |
| Needs someone at the console to check | a test script with `nnh run --script` (screenshots, log checks) |
