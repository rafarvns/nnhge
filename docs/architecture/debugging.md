# Debugging tools

> Status: **design**, specified in [spec 002](../../specs/002-core-foundation/spec.md). Debug text
> (`nnh::debug::print`) already exists.

## In-game tools (Debug and Development flavors)

| Tool | How | Shows |
|------|-----|-------|
| Debug overlay | PC: `F1` · 3DS: hold SELECT + press L (proposal) | FPS, frame time graph (last 120 frames), memory per region and per tag |
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
| `F12` | screenshot to `build/screenshots/` |

Plus the existing environment variables `NNH_EXIT_AFTER_FRAMES` / `NNH_SCREENSHOT` for automation.

## Debuggers

**PC**: build `pc-debug` and run the game under any debugger (VS Code with the CMake Tools extension,
Visual Studio, CLion, gdb). Asserts break into the debugger when one is attached.

**3DS**: Luma3DS includes a GDB stub.
1. Build `3ds-debug` (keeps symbols in the `.elf`).
2. On the console: Rosalina menu (L + Down + Select) → Debugger options → Enable debugger; start the game.
3. On the PC: `arm-none-eabi-gdb build/3ds-debug/examples/<ex>/<ex>.elf`, then
   `target remote <console-ip>:<port>` (the port is shown in the Rosalina debugger menu).

Full step-by-step guide: `docs/guides/debugging-on-3ds.md` (written with spec 002).

## Choosing a tool

| Symptom | First tool |
|---------|------------|
| Wrong value, logic bug | logs, then a debugger on PC |
| Works on PC, fails on 3DS | logs over 3dslink, crash report, then GDB on the console |
| Slow on 3DS | overlay frame time → profiler scopes |
| Running out of memory | overlay memory per tag → leak report at exit |
| Visual glitch | simulator pause + frame step, screenshot |
