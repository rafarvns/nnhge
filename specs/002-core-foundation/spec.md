# Spec 002: Core foundation

Status: draft
Author: NNHGE maintainers · Date: 2026-11-17
Implementation order: **before** [spec 001](../001-event-bus-and-scenes/spec.md), which depends on it
(logging, asserts, overflow handling).

## Problem and goal

Every feature from now on needs the same base services: telling the developer what happened (logs),
catching mistakes early (asserts, errors), understanding a crash on a console with no debugger attached,
measuring time and memory on weak hardware, and not damaging the user's SD card. Today the engine has
none of this: a failure on the 3DS shows a frozen screen and nothing else.

It also splits the engine into a **portable core** and **one library per system** that plugs into it
(the maintainer's direction: "a core, and for each system its own libraries that plug into the core").

This spec defines the **core foundation**: the core/platform split, time, build flavors, logging, errors and crash reports,
memory tracking, debug tools and file safety. Design pages:
[platforms](../../docs/architecture/platforms.md), [foundation](../../docs/architecture/foundation.md), [logging](../../docs/architecture/logging.md),
[errors](../../docs/architecture/errors.md), [debugging](../../docs/architecture/debugging.md),
[security](../../docs/architecture/security.md).

## User stories

- As a game developer, I want leveled, per-module logs that reach my PC terminal even when the game runs
  on the 3DS, so that I can follow what my game does.
- As a game developer, I want a crash on the console to show me what happened (message, file:line, last
  logs, registers) and save it to the SD, so that I can fix bugs I can't reproduce on PC.
- As a game developer, I want asserts that catch my mistakes in development and cost nothing in release.
- As a game developer, I want to see FPS, frame time and memory per region on screen, and profile scopes,
  so that I know when I'm exceeding the 3DS budget.
- As a game developer, I want to pause and step frames in the simulator, so that I can inspect fast events.
- As a player, I want a game made with NNHGE to never corrupt my save or touch files outside its folder.

## Functional requirements

### Core and platform libraries (PLAT) — design in [platforms.md](../../docs/architecture/platforms.md)
- **FR-P1** The engine is split into `nnh::core` (portable, no system SDK headers) and one static library
  per system (`nnh::platform_pc`, `nnh::platform_3ds`, `nnh::platform_fake`) that implements the platform
  contract; games link `nnh::engine` = core + the platform chosen by the preset.
- **FR-P2** The platform contract is a set of documented headers (`nnh/platform/<area>.hpp`); core calls
  only those, resolved at link time (no virtual dispatch).
- **FR-P3** Conformance tests for the contract run against the fake and PC platforms in CI and as a test
  program on 3DS.
- **FR-P4** `nnh::system::has(Capability)` and `nnh::system::info()` report what the running system offers.
- **FR-P5** A platform library may ship extension headers `<nnh/<system>/...>` for system-only features;
  using them on another platform fails at build time with a clear message.

### Time (TIME)
- **FR-T1** `nnh::time` gives a monotonic clock (`now()`), the last frame's delta and the frame count.
- **FR-T2** A fixed-timestep helper runs game logic at a fixed rate independent of rendering hiccups.

### Build flavors (BUILD)
- **FR-B1** Three flavors — Debug, Development, Release — selectable by preset, controlling asserts,
  minimum log level, debug overlay/profiler and optimization as in `docs/architecture/foundation.md`.
- **FR-B2** Code disabled in a flavor compiles to nothing (no runtime check, no string data).

### Logging (LOG)
- **FR-L1** Levels trace, debug, info, warn, error, fatal; channels per module and user-defined channels.
- **FR-L2** printf-style macros (`NNH_LOG_INFO(channel, fmt, ...)`) with compile-time format checking;
  levels below the flavor's minimum compile out.
- **FR-L3** Runtime filter per channel and level.
- **FR-L4** Pluggable sinks. Built-in: PC console (colored), file, in-memory ring buffer (last N lines);
  3DS: 3dslink stdio to the PC, debugger output (`svcOutputDebugString`), file on SD.
- **FR-L5** Each line carries frame number, time since start, level and channel.
- **FR-L6** No heap allocation when logging; long messages are truncated, not dropped.

### Errors and crashes (ERR)
- **FR-E1** `NNH_ASSERT(cond, fmt, ...)` (Debug + Development), `NNH_VERIFY` (all flavors),
  `NNH_PANIC(fmt, ...)` (never returns).
- **FR-E2** `nnh::Result<T>`: a value or an error (code + short message), no exceptions.
- **FR-E3** A failed assert/verify/panic shows a **crash screen** on both screens (message, file:line,
  last log lines) and writes a crash report file; the app then waits for the user to exit.
- **FR-E4** On 3DS, CPU exceptions (data abort, prefetch abort, undefined instruction) are caught with
  `threadOnException` and reported the same way, including registers. On PC, fatal signals / unhandled
  exceptions produce the report in the console and a file.

### Memory (MEM)
- **FR-M1** Engine allocations go through tagged allocators per region (heap, linear, VRAM on 3DS) with
  usage counters and optional budgets.
- **FR-M2** Exceeding a budget logs a warning (Debug/Development); the simulator enforces Old 3DS budgets.
- **FR-M3** At shutdown, Debug/Development builds report allocations that were never freed, per tag.

### Debug tools (DBG)
- **FR-D1** A debug overlay toggled by a key (PC) or button combo (3DS) shows FPS, frame time (graph),
  memory per region and per tag.
- **FR-D2** `NNH_PROFILE_SCOPE("name")` measures scopes; the overlay shows the slowest scopes of recent
  frames. Compiled out in Release.
- **FR-D3** Simulator only: pause, single-frame step, slow motion, screenshot hotkey.
- **FR-D4** Documented procedure to debug on 3DS with GDB through Luma3DS's gdbstub and on PC with an IDE.

### File safety (SEC)
- **FR-S1** All engine file writes go through `nnh::fs`, confined to the game's data folder
  (`sdmc:/3ds/<game>/` on 3DS, a per-game folder on PC). Absolute paths, `..`, and device prefixes are
  rejected with an error.
- **FR-S2** Save files are written atomically (temp file + rename, keeping a backup) with a header
  (magic, format version, size, checksum). Loading falls back to the backup when the main file is invalid.
- **FR-S3** Every parser of external data (saves, assets, config) checks bounds and has a fuzz test
  running in CI for a fixed time budget.
- **FR-S4** CI workflows use least-privilege `permissions` and third-party actions pinned by commit SHA.

## Acceptance criteria

- **AC-0** The repository follows the layout in `platforms.md`; `nnh::core` compiles without any system SDK
  on its include path; conformance tests pass on fake and PC (CI) and on a 3DS.
- **AC-1** Unit tests (PC) cover: clock/delta and fixed-step math with an injected fake clock; log
  formatting, filtering, truncation and the ring buffer; `Result`; allocator counters, budgets and leak
  report; path validation (accepted and rejected paths); save format round trip, corruption detection and
  backup fallback.
- **AC-2** Fuzz targets for the save-file parser and path validator run in CI (short budget) with no crash.
- **AC-3** Example `examples/foundation/`: logs on several channels, toggles the overlay, profiles a scope,
  saves and loads a small save file; pressing a combo triggers a deliberate assert. Simulator screenshots
  show the overlay and the crash screen; on a 3DS the logs appear in the `3dslink` terminal, the crash
  screen appears and `crash.txt` is written.
- **AC-4** A deliberate null-pointer write on 3DS shows the crash screen with registers (exception handler).
- **AC-5** In a Release build, asserts, trace/debug/info logs, overlay and profiler are absent from the
  binary (checked by searching the binary for their format strings) and the `.3dsx` grows by less than
  the agreed budget (measured in the plan).
- **AC-6** `hello` still runs; build budgets still met (constitution III).
- **AC-7** Docs: `docs/api/{log,assert,result,time,memory,fs}.md` and the debugging guide exist;
  architecture pages no longer say "design".

## Non-goals

- Network security of NNH Link (pairing, authentication) — its own spec.
- Remote logging to a server, telemetry (never).
- A full in-game console with commands.
- Thread-safe logging from multiple threads (single thread for now; audio thread later).
- Asset formats themselves (only the rule that their parsers are bounded and fuzzed).

## Constraints

- Constitution II/V: no exceptions, RTTI or per-frame heap; fixed-size buffers sized for Old 3DS.
- Constitution III: public headers stay light (logging macros must not pull heavy headers).
- Constitution X (new): safe by default — see `docs/process/constitution.md`.

## Open questions

- [ ] Ring buffer size and max line length (proposal: 64 lines × 160 chars ≈ 10 KB).
- [ ] Overlay combo on 3DS (proposal: hold SELECT + press L).
- [ ] Release flavor: keep `warn`/`error` logs to SD by default, or console only?
- [ ] Fuzzing tool: libFuzzer (clang) in CI on Linux, plus a plain random-input runner for MinGW/MSVC?
- [ ] Crash screen in Release: full details or a friendly message + "report saved" (details in file)?
