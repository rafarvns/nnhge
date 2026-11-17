# log — logging

Header: `<nnh/log.hpp>` · Design: [architecture/logging.md](../architecture/logging.md)

```cpp
#include <nnh/log.hpp>

NNH_LOG_CHANNEL(player);                          // define once, in one .cpp
// NNH_LOG_CHANNEL_EXTERN(player);                // in a header, to use it from other files

NNH_LOG_INFO(player, "spawned at %d,%d", x, y);
NNH_LOG_WARN(player, "health below zero: %d", hp);
```

Output (PC terminal, colored by level; 3DS: the `3dslink -s` terminal):

```
[f0        0.000s] I app    started "My Game" (Development build)
[f120      2.003s] I player spawned at 10,20
```

## Macros

| Macro | Compiled in |
|-------|-------------|
| `NNH_LOG_TRACE(channel, fmt, ...)` | Debug |
| `NNH_LOG_DEBUG(channel, fmt, ...)` | Debug, Development |
| `NNH_LOG_INFO(channel, fmt, ...)` | Debug, Development |
| `NNH_LOG_WARN` / `NNH_LOG_ERROR` / `NNH_LOG_FATAL` | all flavors |
| `NNH_LOG_CHANNEL(name)` / `NNH_LOG_CHANNEL_EXTERN(name)` / `NNH_LOG_CHANNEL_REF(name)` | define / declare / access a channel |

Macros below the flavor's level expand to nothing: arguments are **not evaluated** and the format string
is not in the binary. Format strings are checked at compile time (GCC/Clang).

## Runtime control

| Function | |
|----------|-|
| `NNH_LOG_CHANNEL_REF(player).min_level = nnh::LogLevel::Warn;` | filter one channel |
| `nnh::log::set_global_level(level)` | filter every channel |
| `nnh::log::add_sink(fn, user)` / `remove_sink(fn, user)` | extra outputs (max `kMaxSinks` = 8); `fn(level, line, user)` |
| `nnh::log::ring_size()` / `ring_line(i)` | last 64 lines, oldest first (used by the crash report) |
| `nnh::log::kCompiledMinLevel` | lowest level compiled in this flavor |

## Limits and costs

- Lines are at most 159 characters; longer messages end with `...`. No heap allocation.
- Logging works before `nnh::init()` and after `shutdown()` (frame and time are then 0).
- A sink must not log; nested logging from a sink is dropped.
- Engine channels (`app`, `gfx`, `input`, ...) live in namespace `nnh`, so your own channels can reuse
  those names.

## Seeing 3DS logs on the PC

In Debug and Development builds, a game started through 3dslink sends its log to the PC. Keep 3dslink
listening with `-s`:

```sh
/c/devkitPro/tools/bin/3dslink.exe -s build/3ds-dev/examples/hello/hello.3dsx -a <console-ip>
```
