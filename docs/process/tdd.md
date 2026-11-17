# Test-Driven Development (TDD)

## The loop

1. **Red** — write one small test for the next behaviour. Build and run it; it must fail (or not compile)
   for the expected reason.
2. **Green** — write the simplest code that makes it pass. Run all tests.
3. **Refactor** — clean up code and test while staying green: names, duplication, file rules.

Repeat per behaviour. Commit when green.

## Commands

```sh
cmake --build --preset pc-debug && ctest --preset pc-debug     # build + all tests
build/pc-debug/tests/nnh_tests.exe                             # all tests, one line per test
build/pc-debug/tests/nnh_tests.exe <filter>                    # only tests whose name contains <filter>
```

The test build is fast on purpose: an in-house framework (`tests/framework/`), no external dependency.

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

- File: `tests/<module>/test_<concept>.cpp`, mirroring `engine/src/<module>/`. Add it to
  `tests/CMakeLists.txt`.
- Name: `behaviour_when_condition`, read as a sentence.
- One behaviour per test. Arrange, act, check. No logic (loops, ifs) in tests unless it's table data.
- Tests are deterministic: no real clock, no real files, seeded randomness.

## What gets unit tests

| Code | How it's verified |
|------|-------------------|
| Engine logic in `engine/src/<module>/` (event bus, input edges, scene stack, math, parsers) | Unit tests, written first |
| Platform backends (`engine/src/platform/<target>/`) | Example program + simulator screenshot (`NNH_SCREENSHOT`) + run on 3DS |
| Public API wiring | Unit tests against the **fake** platform backend |

Design for testability: put decisions in plain logic that takes data in and gives data out (like
`InputState::update(mask)`), and keep the platform call at the edge (`platform::read_buttons()`).
Logic that needs time or randomness receives them as parameters.

## Fakes, not mocks

A **fake** is a working, simplified implementation (e.g. `engine/src/platform/fake/` that records draw
calls and lets a test script button presses). A **mock** verifies call sequences. We prefer fakes: tests
check results, not how the code got there, so refactoring doesn't break them.

The fake platform backend is introduced together with the first test that needs it (spec 001).

## Don'ts

- Don't write the code first and the test after "to cover it".
- Don't test private details; test through the module's internal or public interface.
- Don't let unit tests start the whole engine; that's what examples are for.
- Don't skip the red step: a test you never saw fail might not test anything.

## Further reading

- Noel Llopis, [Backwards Is Forward: Making Better Games with TDD](https://gamesfromwithin.com/backwards-is-forward-making-better-games-with-test-driven-development) — fakes vs. mocks, platform layers.
- [Clean Game Architecture — unit tests](https://cleangamearchitecture.com/unit-tests/) — core logic without engine dependencies.
