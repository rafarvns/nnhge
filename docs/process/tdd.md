# Test-Driven Development (TDD)

## The loop

1. **Red** — write one small test for the next behaviour. Build and run it; it must fail (or not compile)
   for the expected reason.
2. **Green** — write the simplest code that makes it pass. Run the tests of the module you changed.
3. **Refactor** — clean up code and test while staying green: names, duplication, file rules.

Repeat per behaviour. Commit when green.

## Commands

```sh
cmake --build --preset pc-debug && ctest --preset pc-debug     # build + all tests
build/pc-debug/tests/nnh_tests.exe                             # all tests, one line per test
build/pc-debug/tests/nnh_tests.exe <filter>                    # only tests whose name contains <filter>
build/pc-debug/tests/nnh_tests.exe --reverse [filter...]       # the other way round (ctest unit_reverse)
```

The test build is fast on purpose: an in-house framework (`tests/framework/`), no external dependency.

Tests must not lean on what an earlier test left behind: another platform's link order runs them in a different
order. ctest runs `nnh_tests` twice, as `unit` (registration order) and `unit_reverse` (`--reverse`), so a leak
fails on every machine (cost: one more `nnh_tests` run, 75 s in pc-debug on the maintainer's machine, about 8% of the
full ctest's 921 s). When only the reverse run fails, bisect with `nnh_tests --reverse <failing test> <earlier
tests...>` until one earlier test is left, then fix the cause: reset the engine state in `nnh::shutdown()` (or the
fake platform's `reset()`), or make the test set up what it reads (a static object, a file-level variable an assert
may leave unassigned). Never order the tests to hide it.

## How much to run, and when

The full suite (every ctest, Release, the 3DS build, every example's `--pc` script, mutation testing, the Linux
VM, fuzzing) takes long; running it after every change made development crawl. Run what the change can break:

| When | Run |
|------|-----|
| Each red/green step | `cmake --build --preset pc-debug --target nnh_tests` and `nnh_tests <module filter>` (the module you touched, plus its direct users' tests if you changed a header they include) |
| End of a plan stage | the above, plus the stage's own ctests (`ctest --preset pc-debug -R <module>`), the goldens the stage could move, and `scripts/dkp.sh cmake --build --preset 3ds-dev` only when the stage touched 3DS or platform code |
| End of a spec (before merging) | everything: `ctest --preset pc-debug` and `pc-release`, the 3DS release build, every example's `--pc` script, the docs check, then stage 9 (Linux VM) and stage 10 (console) |

Mutation testing covers the new logic only: about 15–20 hand mutants on the module's core files, each judged by that
module's tests (`nnh_tests <filter>`), never by a full rebuild and suite per mutant. Fuzz targets run 2 minutes each
in stage 9 (longer only when chasing a finding). Agents never leave a long run in the background and wait for it: run
it in the foreground or not at all.

## Writing a test

```cpp
// tests/input/test_input_state.cpp
#include "input/input_state.hpp"
#include "test.hpp"

NNH_TEST(press_is_reported_only_on_first_frame) {
    nnh::detail::InputState s;
    s.update(A);
    CHECK(s.pressed() == A);
    s.update(A);
    CHECK(s.pressed() == 0);
}
```

- File: `tests/<module>/test_<concept>.cpp`, mirroring `engine/core/src/<module>/`. Add it to
  `tests/CMakeLists.txt`.
- Name: `behaviour_when_condition`, read as a sentence.
- One behaviour per test. Arrange, act, check. No logic (loops, ifs) in tests unless it's table data.
- Tests are deterministic: no real clock, no real files, seeded randomness.

## What gets unit tests

| Code | How it's verified |
|------|-------------------|
| Engine logic in `engine/core/src/<module>/` (event bus, input edges, scene stack, math, parsers) | Unit tests, written first |
| Platform backends (`engine/platforms/<target>/`) | Example program + simulator screenshot (`NNH_SCREENSHOT`) + run on 3DS |
| Public API wiring | Unit tests against the **fake** platform backend |
| An example's pure logic (no engine calls, e.g. `examples/platformer/gate_run.cpp`) | Unit tests in `tests/<example>/`, compiled from the example folder |

Design for testability: put decisions in plain logic that takes data in and gives data out (like
`InputState::update(mask)`), and keep the platform call at the edge (`platform::read_input()`).
Logic that needs time or randomness receives them as parameters.

## Fakes, not mocks

A **fake** is a working, simplified implementation (e.g. `engine/platforms/fake/` that records draw
calls and lets a test script button presses). A **mock** verifies call sequences. We prefer fakes: tests
check results, not how the code got there, so refactoring doesn't break them.

Tests include `nnh/platform_fake.hpp` to script input (`set_held_buttons`, `set_quit_requested`) and
inspect what core asked the platform to do (`state().frames_begun`, drawn texts, clear colors).
`set_alloc_fill(byte)` fills new heap blocks with a byte, to prove core initialises every byte it compares or hashes.
The same contract is checked on every platform by `tests/platform_conformance/`.

## Don'ts

- Don't write the code first and the test after "to cover it".
- Don't test private details; test through the module's internal or public interface.
- Don't let unit tests start the whole engine; that's what examples are for.
- Don't skip the red step: a test you never saw fail might not test anything.

## Further reading

- Noel Llopis, [Backwards Is Forward: Making Better Games with TDD](https://gamesfromwithin.com/backwards-is-forward-making-better-games-with-test-driven-development) — fakes vs. mocks, platform layers.
- [Clean Game Architecture — unit tests](https://cleangamearchitecture.com/unit-tests/) — core logic without engine dependencies.
