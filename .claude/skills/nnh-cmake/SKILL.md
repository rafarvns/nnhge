---
name: nnh-cmake
description: NNHGE build system — CMake presets, nnh_add_game(), adding sources/tests/examples, devkitPro 3DS toolchain, SDL3 fetching, ccache, CI workflow and build-time budgets. Use for any change to CMakeLists.txt, cmake/, CMakePresets.json, scripts/ or .github/workflows/.
---

# CMake and builds

Full notes: `docs/tech/cmake.md`.

## Commands

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug && ctest --preset pc-debug
scripts/dkp.sh cmake --preset 3ds-release && scripts/dkp.sh cmake --build --preset 3ds-release
```

On Windows, devkitPro's toolchain only works from its MSYS2: always go through `scripts/dkp.sh`.

## Map

`CMakeLists.txt` (C++20, `NINTENDO_3DS` → `NNH_PLATFORM`, ccache) · `engine/CMakeLists.txt`
(`nnh_engine` + alias `nnh::engine`, backend sources per platform block) · `cmake/nnh_game.cmake`
(`nnh_add_game(target SOURCES ... TITLE DESCRIPTION AUTHOR ICON)`) · `cmake/nnh_sdl3.cmake` (system →
prebuilt MinGW/MSVC → source; SHA-256 pinned; Git CA-bundle fallback) · `tests/CMakeLists.txt` ·
`examples/CMakeLists.txt` · `.github/workflows/ci.yml` (`pc-linux`, `nintendo-3ds`).

## Rules

- Targets and properties only; no `include_directories`, `link_libraries`, `CMAKE_CXX_FLAGS` edits.
- Explicit `PUBLIC/PRIVATE/INTERFACE`; SDL3 stays `PRIVATE`.
- **No `file(GLOB)`** — list every source.
- `cmake_parse_arguments(PARSE_ARGV ...)` in functions.
- New dependency = last resort: small, permissive, pinned URL + `URL_HASH SHA256`, measured impact.
- CI job ids start with a letter. 3DS CI runs in `devkitpro/devkitarm` and needs `ninja-build cmake`.

## Build-time budgets (constitution III)

Edit one game `.cpp` → simulator < 2 s, `.3dsx` < 5 s; clean engine build < 30 s.
Measure before and after build changes:

```sh
touch examples/hello/main.cpp && time cmake --build --preset pc-debug
touch examples/hello/main.cpp && time scripts/dkp.sh cmake --build --preset 3ds-release
```

Record new baselines in `docs/ROADMAP.md` when they change meaningfully.

## Checklist

- [ ] Both presets configure and build from clean (`rm -rf build/<preset>`)
- [ ] Tests pass; `hello` screenshot unchanged
- [ ] Budgets measured and still met
- [ ] `docs/tech/cmake.md` updated if behaviour changed
