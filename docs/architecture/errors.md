# Errors, asserts and crashes

> Status: **design**, specified in [spec 002](../../specs/002-core-foundation/spec.md).

## Two kinds of failure

| Kind | Example | Mechanism |
|------|---------|-----------|
| **Expected** — can happen in a correct program | file missing, save corrupted, out of memory at load | return `nnh::Result<T>`; caller decides |
| **Bug** — a broken assumption | null handle, index out of range, frame not open, queue overflow | `NNH_ASSERT` / `NNH_VERIFY` / `NNH_PANIC` → crash screen |

No exceptions anywhere in the engine (constitution V).

## Result

```cpp
nnh::Result<SaveData> load_save();

auto save = load_save();
if (!save) {
    NNH_LOG_WARN(game, "no save: %s", save.error().message);
    return new_game();
}
use(save.value());
```

`Result<T>` holds either a `T` or an `nnh::Error { ErrorCode code; const char* message; }` (the message
is a static string or a short fixed buffer — no heap). `Result<void>` for operations without a value.

## Asserts

| Macro | Active in | Use |
|-------|-----------|-----|
| `NNH_ASSERT(cond, fmt, ...)` | Debug, Development | checks that are too costly or too internal for players' builds |
| `NNH_VERIFY(cond, fmt, ...)` | all flavors | checks whose failure would corrupt data or crash anyway |
| `NNH_PANIC(fmt, ...)` | all flavors | unreachable states; never returns |

The condition of a disabled `NNH_ASSERT` is **not evaluated** — never put side effects in it.

## Crash screen and report

On a failed assert/verify, a panic, or a CPU exception:

1. Log the failure at `fatal`.
2. Draw the crash screen on both screens, bypassing the normal frame (works even if rendering state is
   broken):
   - top: message, `file:line`, function, and on CPU exceptions the exception type and registers;
   - bottom: the last log lines from the ring buffer, "report saved to …", "press START to exit".
3. Write the report to `<data dir>/crash.txt` (overwritten each time; previous kept as `crash.prev.txt`).
4. Wait for START (3DS) or window close (PC), then exit cleanly.

Platform hooks:
- **3DS**: `threadOnException` (from `<3ds/thread.h>`) installs a per-thread handler for data abort,
  prefetch abort and undefined instruction, with its own small stack; it gets `CpuRegisters`.
- **PC**: signal handlers (`SIGSEGV`, `SIGABRT`, `SIGFPE`) and, on Windows, an unhandled-exception
  filter; the report goes to the console and the file. A debugger attached on PC breaks at the assert
  instead (`NNH_DEBUG_BREAK`).

## Release builds

Open question in spec 002: full details on the crash screen, or a friendly message with the details only
in `crash.txt`. Either way the report file is always written.
