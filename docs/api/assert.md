# assert — catching bugs

Header: `<nnh/assert.hpp>` · Design: [architecture/errors.md](../architecture/errors.md)

```cpp
#include <nnh/assert.hpp>

NNH_ASSERT(index < count, "index %d out of range (%d)", index, count); // Debug + Development
NNH_VERIFY(texture.valid(), "texture used after destroy");             // every flavor
NNH_PANIC("unknown state %d", state);                                   // never returns
NNH_ASSERT(ptr);                                                        // message is optional
```

| Macro | Active in | On failure |
|-------|-----------|------------|
| `NNH_ASSERT(cond, fmt, ...)` | Debug, Development | crash screen |
| `NNH_VERIFY(cond, fmt, ...)` | all flavors | crash screen |
| `NNH_PANIC(fmt, ...)` | all flavors | always crash screen |

In Release, `NNH_ASSERT` compiles to nothing and its condition is **not evaluated** — never put side
effects in it (`NNH_ASSERT(load())` would skip `load()` in Release).

## What happens on failure

1. The failure is logged at `fatal` (`[f..] F app    Verify failed: ... (file:line)`).
2. On PC with a debugger attached (Windows), execution stops in the debugger.
3. The **crash screen** replaces the game on both screens:
   - Debug/Development: kind, message, failed condition, `file:line`, function; CPU registers for hardware
     faults; the last log lines on the bottom screen.
   - Release: "The game stopped because of an error." with `file:line`; details go to the report file.
4. The user dismisses it (3DS: START; PC: Enter, Esc or closing the window) and the program ends.

Hardware faults are caught too: PC fatal signals (segmentation fault, abort, ...); on 3DS data/prefetch
aborts and undefined instructions — when the console reports them to the game (Luma3DS
"Set developer UNITINFO"); otherwise Luma3DS shows its own exception screen and saves a dump in
`/luma/dumps/arm11/`.

The report file (`crash.txt` in the game's data folder) is written once file support lands (spec 002
stage 7); until then the full report goes to the log console.

## When to use which

- Expected failures (missing file, corrupted save) are not bugs: return `nnh::Result` ([result.md](result.md)).
- `NNH_ASSERT` for internal invariants that are costly or only matter while developing.
- `NNH_VERIFY` when continuing would corrupt data or crash anyway.
- `NNH_PANIC` for states that must be impossible (`default:` of an exhaustive switch).
