---
name: nnh-tdd
description: Test-Driven Development loop for NNHGE. Use when implementing any engine behaviour or fixing a bug — write the failing test first, make it pass, refactor. Covers test file layout, the in-house test framework, build/test commands, fakes, and what is verified by examples/screenshots instead of unit tests.
---

# TDD in NNHGE

Full guide: `docs/process/tdd.md`.

## Loop (per behaviour)

1. **Red**: write one small test; build; see it fail for the expected reason.
2. **Green**: simplest code that passes; run the changed module's tests (`nnh_tests <filter>`); the full suite only at the end of a spec (docs/process/tdd.md, "How much to run, and when").
3. **Refactor**: clean code and test, stay green, respect file rules.

Bug fix = first a test that reproduces the bug (red), then the fix.

## Commands

```sh
cmake --build --preset pc-debug && ctest --preset pc-debug     # all
build/pc-debug/tests/nnh_tests.exe                             # all, one line per test
build/pc-debug/tests/nnh_tests.exe <substring>                 # filter by test name
```

If the preset isn't configured yet: `cmake --preset pc-debug` first.

## Writing tests

```cpp
// tests/<module>/test_<concept>.cpp  — mirrors engine/core/src/<module>/
#include "<module>/<concept>.hpp"
#include "test.hpp"

NNH_TEST(behaviour_when_condition) {
    // arrange
    // act
    CHECK(result == expected);
}
```

- Register new files in `tests/CMakeLists.txt` (explicit list).
- One behaviour per test, name reads as a sentence, no logic in tests, deterministic (no real clock,
  files or unseeded randomness).
- Test internal/public interfaces, not private details.

## What is tested how

| Code | Verification |
|------|--------------|
| Logic in `engine/core/src/<module>/` | unit test first |
| Public API wiring | unit tests linked with the **fake** platform backend (`engine/platforms/fake/`, controls in `nnh/platform_fake.hpp`) |
| Platform backends (pc, 3ds) | example program + simulator screenshot + run on 3DS |

Screenshot check: `NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/shot.png build/pc-debug/examples/<ex>/<ex>.exe`,
then read the PNG.

## Design for testability

Decisions in plain functions/structs (data in → data out, like `InputState::update(mask)`); platform calls
at the edges; time and randomness passed in. Prefer fakes (working simplified implementations) over mocks.

## Done when

- [ ] Saw each new test fail first
- [ ] All tests green, no new warnings
- [ ] PC build and `scripts/dkp.sh cmake --build --preset 3ds-release` both succeed
- [ ] Files within limits (`.cpp` ≤ 400, aim 200)
