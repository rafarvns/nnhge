# Project structure

Code is easy to find when every concept has one obvious place. These rules apply to the engine, tools,
examples and tests.

## Folder map

```
engine/
  core/include/nnh/<module>.hpp              public API, one header per module (what games include)
  core/src/<module>/<concept>.cpp|.hpp       core implementation, internal headers
  core/src/core/                             engine-wide internal state shared by modules
  core/contract/nnh/platform/<area>.hpp      platform contract (what each platform library implements)
  platforms/<system>/CMakeLists.txt          nnh::platform_<system> (pc, 3ds, fake)
  platforms/<system>/src/<area>_<system>.cpp one file per contract area per system
  platforms/<system>/include/nnh/<system>/   extension headers (optional, system-only features)
  platforms/<system>/README.md               requirements, limits, capabilities
tests/
  framework/                                 the tiny test framework
  <module>/test_<concept>.cpp                mirrors engine/core/src/<module>/
  platform_conformance/                      contract tests, linked against every platform
examples/<name>/                             one small program per feature (main.cpp + CMakeLists.txt)
docs/
  api/<module>.md                            reference for each public module
  architecture/  process/  tech/  guides/
specs/NNN-<slug>/                            spec.md, plan.md, tasks.md per feature
link/                                        nnh_link (spec 011, D24): sockets, SHA-256/HMAC, messages, frames,
  include/nnh_link/  src/  tests/(fuzz/)     discovery, pairing, server/client sessions; built for PC and 3DS,
                                             shared by nnh and the app
tools/<tool>/                                host tools: tools/nnh (the nnh command, spec 010); assetc later
third_party/<library>/                       vendored code, unmodified and pinned (README: commit, SHA-256,
                                             license); exempt from the file-size rules
cmake/                                       CMake helpers
scripts/                                     small helpers (dkp.sh, docs tool, hooks)
```

A game made with NNHGE follows the same idea:

```
mygame/
  src/main.cpp                       init, push first scene, run
  src/scenes/<name>_scene.hpp|.cpp   one scene per pair
  src/<feature>/...                  game modules (player/, enemies/, ui/)
  src/events.hpp                     the game's event structs
  assets/
```

## File rules

| Rule | Limit |
|------|-------|
| One concept per file (one class, or one cohesive group of free functions) | always |
| `.cpp` length | aim ≤ 200 lines, **hard limit 400** |
| Header length | aim ≤ 150 lines |
| Function length | aim ≤ 40 lines |
| Public headers per module | exactly one: `<nnh/<module>.hpp>` |
| Internal headers | under `engine/core/src/`, never included by games or public headers |
| Source lists in CMake | explicit (no `file(GLOB)`) |

When a file grows past the aim, split it by concept before adding more (e.g. `video_pc.cpp` and
`input_pc.cpp` instead of one `platform_pc.cpp`). A pull request that crosses a hard limit must split.

## Naming

- Files and folders: `snake_case`. Backend files end with the target: `input_3ds.cpp`, `input_pc.cpp`.
- Types `PascalCase`, functions and variables `snake_case`, constants `kPascalCase`, macros `NNH_UPPER`.
- Members end with `_` in classes with invariants (`score_`); plain structs use bare names.
- Tests: `test_<concept>.cpp`; test cases `behaviour_when_condition`.

## Where does new code go?

| You are adding... | Put it in |
|-------------------|-----------|
| A new engine feature (audio, sprites) | New module: see the `nnh-new-module` skill |
| Platform-specific code | `engine/platforms/<system>/src/<area>_<system>.cpp`, behind a contract header `nnh/platform/<area>.hpp` |
| Logic shared by modules | The module that owns the concept; expose an internal header in its folder |
| A helper used once | Next to its user, in an anonymous namespace |
| A test | `tests/<module>/test_<concept>.cpp` |
| A demo of a feature | `examples/<feature>/` |
