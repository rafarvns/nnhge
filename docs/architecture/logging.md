# Logging

> Status: **implemented** (spec 002 stages 4 and 7). API: [docs/api/log.md](../api/log.md).

## Usage

```cpp
#include <nnh/log.hpp>

NNH_LOG_CHANNEL(player);                       // define a channel (once, in a .cpp)

NNH_LOG_INFO(player, "spawned at %d,%d", x, y);
NNH_LOG_WARN(gfx, "texture %s is %dx%d, not a power of two", name, w, h);
NNH_LOG_ERROR(fs, "save failed: %s", result.error().message);
```

## Levels

| Level | For | Kept in |
|-------|-----|---------|
| `trace` | very frequent detail (every frame) | Debug |
| `debug` | developer detail | Debug, Development |
| `info` | normal milestones (scene loaded, save written) | Debug, Development |
| `warn` | something wrong but handled (budget exceeded, fallback used) | all |
| `error` | an operation failed | all |
| `fatal` | about to crash (used by `NNH_PANIC`) | all |

Levels below the flavor's minimum **compile to nothing**: no call, no format string in the binary.
At runtime the minimum can be raised per channel (`NNH_LOG_CHANNEL_REF(gfx).min_level = nnh::LogLevel::Warn`)
or for all channels (`nnh::log::set_global_level`).

## Channels

One channel per engine module (`app`, `gfx`, `input`, `events`, `fs`, `memory`, ...) plus any the game
declares. A channel is a static object; logging through it costs a level check and nothing else when
filtered out.

## Line format

```
[f1234    20.571s] W gfx    texture hero.png is 100x60, not a power of two
```

Frame number, seconds since start, level letter, channel, message. Lines are at most 159 characters
(longer messages end with `...`), formatted into a stack buffer: **no heap allocation**.

## Sinks

A sink receives each formatted line. Several sinks run at once.

| Sink | Platform | Default in |
|------|----------|------------|
| Console (colored by level) | PC | all flavors |
| File (`<data dir>/log.txt`, previous run in `log.prev.txt`) | PC, 3DS | all flavors (Release: warn and above) |
| Ring buffer (last 64 lines, in memory) | all | all (feeds the crash screen) |
| 3dslink stdio → PC terminal (`3dslink -s`) | 3DS | Debug, Development, when launched via 3dslink |
| Debugger (`svcOutputDebugString`) | 3DS | Debug |

Games can add sinks (e.g. an in-game log viewer) through `nnh::log::add_sink`.

## Platform libraries never log

Platform libraries don't call core, so they never log directly (this keeps the core ↔ platform
dependency one-way). They report failures through return values and `platform::last_error()`, and core
logs them on channel `app`. Core writes every line to `platform::log_console()`, which each platform
implements (PC terminal with colors; 3DS stdout + debugger).

## Rules

- Log events that help someone debug later; don't log every frame above `trace`.
- No secrets or personal data in logs (logs end up in bug reports).
- Messages in English, lowercase start, no trailing period, values quoted when they may be empty.
- Errors are logged once, where they are handled — not at every level they pass through.
