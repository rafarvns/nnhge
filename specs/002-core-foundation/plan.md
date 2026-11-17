# Plan 002: Core foundation

Status: implemented (2026-11-17)
Spec: [spec.md](spec.md)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | This spec *creates* the core/platform boundary; extension headers are opt-in and explicit |
| II 3DS constraint | ✅ | Fixed buffers (log ring 10 KB, profiler, overlay); budgets default to Old 3DS |
| III Build speed | ✅ | Log/assert macros in light headers; measure after stage 1 and at the end |
| IV Event-driven | ✅ | Foundation sits below the event bus; no events needed here |
| V No hidden costs | ✅ | No exceptions/RTTI; logging formats into stack buffers; disabled code compiles out |
| VI Spec + test first | ✅ | Every core module unit-tested with the fake platform; backends verified by example + 3DS |
| VII Small files | ✅ | One concept per file; layout below |
| VIII Documented API | ✅ | docs/api page per module, debugging guide, platform READMEs |
| IX Clean and legal | ✅ | CRC-32 and fuzzing written in-house or via toolchain (libFuzzer ships with clang) |
| X Safe by default | ✅ | This spec implements it |

## Delivery in stages

The spec is large, so it ships in **9 stages**. Each stage is a commit series that ends green (PC + 3DS
build, tests, CI) with its docs updated. Stages 1–2 restructure; 3–8 add modules; 9 closes.

| Stage | Content | Covers |
|-------|---------|--------|
| 1 | Core / platform split, contract headers, `platform_fake`, conformance skeleton | FR-P1–P3 |
| 2 | Build flavors | FR-B1–B2 |
| 3 | Time | FR-T1–T2 |
| 4 | Logging (+ console, ring, 3dslink, debugger sinks) | FR-L1–L6 |
| 5 | Errors: asserts, `Result`, crash screen + report, exception/signal hooks | FR-E1–E4 |
| 6 | Memory accounting | FR-M1–M3 |
| 7 | `nnh::fs` (confined), atomic saves, file log sink, fuzzing | FR-S1–S3, FR-L4 (file) |
| 8 | System capabilities, debug overlay, profiler, simulator controls | FR-P4–P5, FR-D1–D3 |
| 9 | CI hardening, foundation example, GDB guide, API docs, final measurements | FR-S4, FR-D4, AC-3–AC-7 |

## Design

### Layout and targets (stage 1)

```
engine/core/include/nnh/<module>.hpp          public API (games)
engine/core/contract/nnh/platform/<area>.hpp  platform contract (platform libraries only, not games)
engine/core/src/<module>/...                  core implementation
engine/platforms/pc/   {CMakeLists.txt, README.md, src/<area>_pc.cpp, src/context_pc.hpp}
engine/platforms/3ds/  {CMakeLists.txt, README.md, src/<area>_3ds.cpp, src/context_3ds.hpp, include/nnh/3ds/}
engine/platforms/fake/ {CMakeLists.txt, README.md, src/<area>_fake.cpp, include/nnh/platform_fake.hpp}
tests/<module>/...     tests/platform_conformance/...     tests/fuzz/...
```

| CMake target | Kind | Links |
|--------------|------|-------|
| `nnh::core_contract` | INTERFACE | include dir `engine/core/contract` |
| `nnh::core` | STATIC | `core_contract` (PUBLIC to platform libs only via the contract target) |
| `nnh::platform_<system>` | STATIC | `core_contract`, system SDK (PRIVATE) |
| `nnh::engine` | INTERFACE | `nnh::core` + the preset's platform (what games link) |

Core calls contract functions implemented by the platform; platforms may call core (logging), so the
two static libraries depend on each other. `nnh::engine` links them as `core platform core` (CMake
`LINK_INTERFACE_MULTIPLICITY 2` on `nnh_core`) so single-pass linkers (GNU ld) resolve both directions.
Tests link `nnh::core` + `nnh::platform_fake`.

`NNH_PLATFORM` (pc/3ds) stays the preset switch; a future `switch` adds a folder and presets only.

### Platform contract (stage 1 creates it from today's `platform.hpp`; later stages add areas)

| Header | Functions (initial) | Stage |
|--------|---------------------|-------|
| `app.hpp` | `init`, `shutdown`, `poll_events` | 1 |
| `video.hpp` | `begin_frame`, `end_frame`, `debug_text`, `debug_line_height` | 1 |
| `input.hpp` | `read_buttons` | 1 |
| `time.hpp` | `ticks_ns()` monotonic | 3 |
| `log.hpp` | `log_write_console(level, line)`, optional extra sinks registered at init | 4 |
| `crash.hpp` | `crash_install_hooks()`, `crash_show(const CrashReport&)` (never returns) | 5 |
| `memory.hpp` | region allocators: `alloc(region, size, align)`, `free`, `region_capacity` | 6 |
| `fs.hpp` | `data_root()`, `open/read/write/close/rename/remove/exists` on validated paths | 7 |
| `system.hpp` | `capabilities()`, `info()`, `overlay_combo_pressed()` | 8 |

Each function documents when it's called, failure behaviour and whether it may allocate.

### Build flavors (stage 2)

Presets: `pc-debug`, `pc-dev`, `pc-release`, `3ds-debug`, `3ds-dev`, `3ds-release`.
Mapping: Debug → `CMAKE_BUILD_TYPE=Debug`, Development → `RelWithDebInfo`, Release → `Release`; CMake
defines exactly one of `NNH_FLAVOR_DEBUG|DEVELOPMENT|RELEASE` on `nnh::core` (PUBLIC, so macros in public
headers see it). `-dev` replaces nothing existing; `*-debug`/`*-release` keep their names.

### Time (stage 3)

`<nnh/time.hpp>`: `struct Duration { std::int64_t ns; }` with helpers (`seconds()`, `ms()`), `now()`,
`delta()`, `FixedStep{hz, max_catch_up=4}` with `bool next()`. Clock read through the contract
(`svcGetSystemTick` × 1e9 / 268111856 on 3DS; `SDL_GetTicksNS` on PC; manual in fake).

### Logging (stage 4)

`<nnh/log.hpp>`: `enum class LogLevel`, `struct LogChannel { const char* name; LogLevel min; }`,
`NNH_LOG_CHANNEL(name)` / `NNH_LOG_CHANNEL_EXTERN(name)`, `NNH_LOG_<LEVEL>(channel, fmt, ...)` which
expand to nothing below the flavor minimum. Core: `log/format.cpp` (prefix + vsnprintf into 160-byte
buffer, `…` on truncation), `log/ring.cpp` (64×160), `log/sinks.cpp` (fixed array of 8 sinks),
`log/log.cpp`. Engine channels: `app`, `gfx`, `input`, `fs`, `memory`, `crash`, `debug`.
Platform sinks: PC console with ANSI colors (Windows: enable VT mode); 3DS: stdout (reaches 3dslink when
`link3dsStdio()` succeeded at init — tried only if launched by 3dslink, `envGetHandle`/argv check) and
`svcOutputDebugString` in Debug.

### Errors (stage 5)

`<nnh/assert.hpp>`: `NNH_ASSERT`, `NNH_VERIFY`, `NNH_PANIC`, `NNH_DEBUG_BREAK`; failure path is a cold,
`[[noreturn]]` function `detail::fail(kind, file, line, func, fmt, ...)`.
`<nnh/result.hpp>`: `ErrorCode` enum, `Error { code; char message[64]; }`, `Result<T>` (union storage,
no exceptions, `[[nodiscard]]`), `Result<void>`.
Crash flow (`crash/report.cpp`): build `CrashReport` (kind, message, location, registers optional,
ring-buffer snapshot) → write `crash.txt` (stage 7 enables the file; before that, console only) →
`platform::crash_show(report)`.
- 3DS `crash_show`: stop citro3d if active, re-init `gfx` with two `consoleInit` text consoles (one per
  screen), print, loop on `aptMainLoop` until START, `exit`. `threadOnException(handler, own 16 KB stack,
  WRITE_DATA_TO_HANDLER_STACK)` installed at init; the handler copies registers into the report and calls
  the same path.
- PC `crash_show`: print to stderr; if the renderer works, draw the report with debug text and wait for
  close; `SetUnhandledExceptionFilter` (Windows) / `sigaction` (POSIX) for SIGSEGV/SIGABRT/SIGFPE/SIGILL.
- Release: friendly message + file:line on screen; full report in file (spec decision).

### Memory (stage 6)

`<nnh/memory.hpp>`: `enum class MemRegion { Heap, Linear, Vram }`, `MemTag` (name + id, user-declarable),
`nnh::mem::alloc(region, tag, size, align)` / `free`, `stats(region)`, `stats(tag)`, `set_budget`.
Bookkeeping in core: per-allocation header (size, tag) for Heap/Linear; VRAM via a side table (fixed
1024 entries) because VRAM can't be written by the CPU. Counters: bytes, peak, count. Leak report at
shutdown in Debug/Development. Simulator emulates Linear/VRAM with heap + Old 3DS capacities.

### File system and saves (stage 7)

`<nnh/fs.hpp>`: `fs::Path` validation (`validate_relative_path` pure function, heavily tested and fuzzed),
`read_file`, `write_file_atomic`, `exists`, `remove`; `nnh::save::write/read` with header
`{ magic 'NNHS', u16 version, u16 flags, u32 size, u32 crc32 }` + payload. Atomic write order from
`docs/architecture/security.md`. Roots: 3DS `sdmc:/3ds/<title>/` (title from a CMake define set by
`nnh_add_game`); PC `<exe dir>/data/` in Debug/Development, OS app-data folder in Release.
File log sink (`log.txt`, rotated to `log.prev.txt` at start) and `crash.txt` enabled here.
Fuzzing: `tests/fuzz/fuzz_path.cpp`, `fuzz_save.cpp` with `LLVMFuzzerTestOneInput`; built with libFuzzer
under clang (CI job `fuzz`, 60 s per target) and also as a corpus runner linked into `nnh_tests`
(seed corpus in `tests/fuzz/corpus/<target>/`).

### Capabilities, overlay, profiler (stage 8)

`<nnh/system.hpp>`: `enum class Capability { TouchScreen, SecondScreen, Stereo3D, CStick, ZButtons, ... }`,
`has(Capability)`, `info()` (`name`, `model`, `memory_mb`). 3DS reads `APT_CheckNew3DS`.
`<nnh/profile.hpp>`: `NNH_PROFILE_SCOPE(name)` RAII timer into a fixed per-frame table (64 scopes).
Overlay in `debug/overlay.cpp`, drawn with debug text + rectangles (adds `platform::video::fill_rect`).
Simulator controls in `platforms/pc/src/devtools_pc.cpp`. All compiled out in Release.

### Extension headers (FR-P5, stage 8)

`engine/platforms/3ds/include/nnh/3ds/` starts with one small real extension (`nnh/3ds/console.hpp`:
`is_new_3ds()`, `slider_3d()`), each header beginning with
`#if !defined(NNH_PLATFORM_3DS) #error "<nnh/3ds/...> is only available when building for 3DS" #endif`.

## Test strategy

| Requirement | Verified by |
|-------------|-------------|
| FR-P1, P2 | builds of all presets; `nnh::core` built with no SDK include path (CI check) |
| FR-P3 | `tests/platform_conformance/` run with fake (CI) and pc (CI headless); 3DS: `examples/conformance` on console |
| FR-T1, T2 | `tests/time/` with fake clock: delta, FixedStep catch-up limit |
| FR-L1–L6 | `tests/log/`: format, truncation, level filter, channel filter, ring wrap, sink fan-out |
| FR-E1–E2 | `tests/result/`; assert macros compile-out test (flavor), fail path tested via fake `crash_show` that records |
| FR-E3–E4 | example deliberate assert + null write; screenshots (PC), 3DS manual check |
| FR-M1–M3 | `tests/memory/`: counters, peak, budgets warning, leak report contents |
| FR-S1 | `tests/fs/test_path.cpp`: accepted/rejected tables; fuzz `fuzz_path` |
| FR-S2 | `tests/fs/test_save.cpp`: round trip, CRC mismatch, truncated file, fallback to `.bak`, interrupted-write simulation in fake fs |
| FR-S3 | fuzz CI job + corpus runner in unit tests |
| FR-S4 | workflow review (`permissions`, SHA pins) in CI diff |
| FR-D1–D3 | example + screenshots with overlay; simulator key tests manual |
| FR-B2, AC-5 | CI step: build `3ds-release`, `strings` the `.elf` for a known trace/assert format string → must be absent |

## Risks and alternatives

- **3DS CPU exceptions may not reach our handler** on retail units unless UNITINFO is non-zero (libctru
  `thread.h` note). Mitigation: document enabling Luma3DS "Set developer UNITINFO" for development; when
  disabled, Luma's own handler takes over and writes a dump to `/luma/dumps/arm11/` — the guide explains
  reading it. Asserts/panics don't depend on this.
- **Crash screen after GPU state corruption** may fail on 3DS: we drop to libctru's text console
  (CPU framebuffer), which only needs `gfxInit`.
- **Static library cycle** core ↔ platform: *resolved in stage 4* — platform libraries never call core;
  they report through return values and `platform::last_error()` and core logs. No link multiplicity needed.
- **libFuzzer unavailable on MinGW/MSVC**: fuzz only in Linux CI; corpus runner everywhere.
- **Build time** grows with more modules: measure after stages 1 and 9; keep headers light.
- Alternative rejected: runtime platform registry with virtual interfaces (D15).

## Constitution Check (after design)

Unchanged: all ✅. Article III will be re-measured after stage 1 (layout change) and at stage 9.
