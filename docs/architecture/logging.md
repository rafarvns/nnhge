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
| 3dslink stdio → PC terminal (`nnh run`, `3dslink -s`) | 3DS | Debug, Development, when launched via 3dslink |
| Debugger (`svcOutputDebugString`) | 3DS | Debug |

The file sink batches lines in a 4 KB static buffer: one append to the 3DS SD card costs ~20 ms, so writing each line
made every logged frame miss 60 Hz (found by the platformer's replay on an Old 3DS, 2026-11-22). A warning or worse
writes the batch out at once with the lines before it, in order; so do a full batch and shutdown, and a crash (it logs a
fatal line first). Info lines of the last moments before a power-off can be lost; the crash report keeps the last 64.

Games can add sinks (e.g. an in-game log viewer) through `nnh::log::add_sink`.

In the simulator started by `nnh run --pc` (Debug/Development, `NNH_REMOTE` set), the console sink sends each
line to `nnh` over the loopback connection instead of the terminal, so `nnh` shows and saves it like a
console's log (`build/logs/<game>-<date>-<time>.txt`). If that connection fails, lines go back to the terminal.

## Records in the log stream

While `nnh run` drives a game (spec 013, decision D28; [debugging](debugging.md#driving-a-game-from-the-pc)), the
game also writes small binary **records** into the same stream as its log lines: the answer to `nnh`'s `Hello`,
a `Tick` every 4th frame, `Late`/`Rejected`/`Dropped` for messages, and `Screen`/`ScreenFailed` for screenshots.
A record starts with `0x1E 'N' 'R'` (ASCII record separator + magic), then a type byte and a 32-bit length, so
it can't be mistaken for a log line (formatted lines are printable text); `nnh` treats anything that doesn't
parse as a record as text. Records go out only after `nnh` said `Hello` and before it says `Bye`, so a PC that
never says `Hello` (`3dslink -s`, `nnh serve`, a plain `nnh run`) sees exactly the plain-text log. On the 3DS,
stdout and stderr are flushed before each record: one ordered stream, so once the tick of frame *f* arrives, a script
knows every line of the frames before *f* has arrived. The remote logs on channel `remote` (attach, detach,
rejected messages at most once per 60 frames, the screenshot frame's cost).

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
