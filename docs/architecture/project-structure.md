# Project structure

Code is easy to find when every concept has one obvious place. These rules apply to the engine, tools,
examples and tests.

## Folder map

```
engine/
  include/nnh/<module>.hpp           public API, one header per module (what games include)
  src/<module>/<concept>.cpp|.hpp    module implementation, internal headers
  src/core/                          engine-wide internal state shared by modules
  src/platform/platform.hpp          interface every backend implements
  src/platform/<target>/<module>_<target>.cpp   one backend file per module per target
tests/
  framework/                         the tiny test framework
  <module>/test_<concept>.cpp        mirrors engine/src/<module>/
examples/<name>/                     one small program per feature (main.cpp + CMakeLists.txt)
docs/
  api/<module>.md                    reference for each public module
  architecture/  process/  tech/  guides/
specs/NNN-<slug>/                    spec.md, plan.md, tasks.md per feature
tools/<tool>/                        nnh CLI, assetc (later)
cmake/                               CMake helpers
scripts/                             small shell helpers
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
| Internal headers | under `engine/src/`, never included by games or public headers |
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
| Platform-specific code | `engine/src/platform/<target>/<module>_<target>.cpp`, behind `platform.hpp` |
| Logic shared by modules | The module that owns the concept; expose an internal header in its folder |
| A helper used once | Next to its user, in an anonymous namespace |
| A test | `tests/<module>/test_<concept>.cpp` |
| A demo of a feature | `examples/<feature>/` |
