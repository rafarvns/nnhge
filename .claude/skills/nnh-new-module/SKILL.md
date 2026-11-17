---
name: nnh-new-module
description: Scaffold a new NNHGE engine module (e.g. audio, sprite, events, scene) with the right files in the right places — public header, module folder, platform backend files, tests, docs/api page, example and CMake wiring. Use after the module's spec and plan are approved.
---

# New engine module

Structure rules: `docs/architecture/project-structure.md`. The module must already be in an approved
spec/plan (`specs/`). Replace `<m>` with the module name (`snake_case`, singular: `audio`, `sprite`).

## Files to create

| File | Content |
|------|---------|
| `engine/include/nnh/<m>.hpp` | Public API only. Doxygen comment per symbol: what, ownership/lifetime, cost (allocates? per-frame safe?), platform notes, 2–5 line example. Includes only `<cstdint>`-level std headers and other `nnh/` headers. |
| `engine/src/<m>/<m>.cpp` | Implementation of the public API (thin; delegates logic) |
| `engine/src/<m>/<concept>.hpp/.cpp` | Testable logic, one concept per file |
| `engine/src/<m>/<m>_internal.hpp` | Only if another module needs internal access |
| `engine/src/platform/platform.hpp` | Add the platform functions the module needs (documented) |
| `engine/src/platform/{pc,3ds}/<m>_{pc,3ds}.cpp` | Backend implementations (+ `fake/` once it exists) |
| `tests/<m>/test_<concept>.cpp` | Unit tests, written first (`nnh-tdd`) |
| `docs/api/<m>.md` | Concepts, table of functions, examples, limits per platform, pitfalls |
| `examples/<feature>/main.cpp` + `CMakeLists.txt` | Small demo with `nnh_add_game(...)` |

## Wiring

- `engine/CMakeLists.txt`: shared sources in `add_library(nnh_engine ...)`, backend files in the matching
  platform block. Explicit lists.
- `tests/CMakeLists.txt`: add test files.
- `examples/CMakeLists.txt`: `add_subdirectory(<feature>)`.
- `engine/include/nnh/nnh.hpp`: add the header.
- If the module publishes events, declare them in its public header (see `docs/architecture/events.md`).

## Checklist

- [ ] No platform headers in `engine/include/`
- [ ] No exceptions/RTTI/per-frame heap; capacities documented
- [ ] Builds on PC and 3DS; example screenshot checked; tests green
- [ ] Files within limits; names follow `<module>_<target>.cpp`
- [ ] `docs/api/<m>.md` written; ROADMAP item checked
