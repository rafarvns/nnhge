# Core foundation

> Status: **design**, specified in [spec 002](../../specs/002-core-foundation/spec.md). Built before
> the event bus (spec 001), which uses it.

The services every other module relies on. Each is its own small module with one public header:

| Module | Header | Purpose | Page |
|--------|--------|---------|------|
| time | `<nnh/time.hpp>` | Monotonic clock, frame delta, fixed timestep | this page |
| log | `<nnh/log.hpp>` | Leveled, per-channel logging to pluggable sinks | [logging.md](logging.md) |
| assert | `<nnh/assert.hpp>` | `NNH_ASSERT`, `NNH_VERIFY`, `NNH_PANIC`, crash screen | [errors.md](errors.md) |
| result | `<nnh/result.hpp>` | `nnh::Result<T>` for recoverable errors | [errors.md](errors.md) |
| memory | `<nnh/memory.hpp>` | Tagged allocators, budgets, leak report | this page |
| debug | `<nnh/debug.hpp>` | Debug text (exists), overlay, profiler | [debugging.md](debugging.md) |
| fs | `<nnh/fs.hpp>` | Confined file access, atomic saves | [security.md](security.md) |
| system | `<nnh/system.hpp>` | Capability queries, system info | [platforms.md](platforms.md) |

Each module's system-specific part (clock source, file APIs, log sinks, crash hooks) is a function of the
platform contract, implemented by each platform library ([platforms.md](platforms.md)).

Dependency order (lower first): time → log → assert/result → memory → fs → debug overlay/profiler.
No foundation module depends on the event bus or scenes.

## Build flavors

> Status: **implemented** (spec 002 stage 2). API: [docs/api/build.md](../api/build.md).

| Flavor | Preset suffix | CMake build type | Asserts | Min log level | Overlay + profiler | Optimization | Use |
|--------|---------------|------------------|---------|---------------|--------------------|--------------|-----|
| **Debug** | `-debug` | `Debug` | `ASSERT` + `VERIFY` | trace | yes | none (`-O0 -g`) | stepping in a debugger |
| **Development** | `-dev` | `RelWithDebInfo` | `ASSERT` + `VERIFY` | debug (info shown by default) | yes | `-O2 -g` | daily work, testing on 3DS |
| **Release** | `-release` | `Release` | `VERIFY` only | warn | no | toolchain release flags (`-O2`/`-O3`) | what players get |

CMake defines one macro on `nnh::core` (PUBLIC), `NNH_FLAVOR_DEBUG`, `NNH_FLAVOR_DEVELOPMENT` or
`NNH_FLAVOR_RELEASE`, and `<nnh/build.hpp>` turns it into `nnh::build::flavor()`,
`kAssertsEnabled` and `kDevToolsEnabled`. Foundation headers use them to compile things out; game code
may display the flavor but shouldn't branch gameplay on it.

## Time

> Status: **implemented** (spec 002 stage 3). API: [docs/api/time.md](../api/time.md).

- `nnh::time::now()` — monotonic `Duration` since `init()` (3DS: `svcGetSystemTick`, 268 MHz ticks;
  PC: `SDL_GetTicksNS`).
- `nnh::time::delta()` — duration of the last frame; `nnh::frame_count()` already exists.
- Fixed timestep: `nnh::time::FixedStep step{60}; while (step.next()) update_logic();` — runs logic a
  fixed number of times per second, catching up at most a few steps after a hitch.
- Durations are integer nanoseconds (`std::int64_t`), not floats, so long sessions don't lose precision.
- Tests use a fake clock: the time module reads time through the platform interface.

## Memory

- Regions: **heap** (`malloc`), **linear** (GPU-visible, 3DS), **VRAM** (3DS). The PC simulator emulates
  linear and VRAM budgets so overruns show up on PC first.
- Every engine allocation names a **tag** (`gfx`, `audio`, `events`, user tags). Counters per region and
  tag: current bytes, peak, allocation count.
- **Budgets** per region (defaults = Old 3DS) and optional per tag; going over logs a warning once.
- At shutdown (Debug/Development), any live allocation is reported per tag: a leak list.
- Per-frame code must not allocate; allocators can assert on allocation during a frame for tags marked
  "load-time only".

## Further reading

- Glenn Fiedler, [Fix Your Timestep!](https://gafferongames.com/post/fix_your_timestep/)
- [devkitpro-3ds.md](../tech/devkitpro-3ds.md) for memory regions on the 3DS.
