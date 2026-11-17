# CMake

One CMake project builds every target. The platform is chosen by the preset (toolchain), never by
options in game code.

## Presets

| Preset | Target | Toolchain |
|--------|--------|-----------|
| `pc-debug`, `pc-dev`, `pc-release` | PC simulator | host compiler (MinGW GCC, MSVC, clang) |
| `3ds-debug`, `3ds-dev`, `3ds-release` | Nintendo 3DS | `$DEVKITPRO/cmake/3DS.cmake` (run via `scripts/dkp.sh` on Windows) |

The suffix is the build flavor ([docs/api/build.md](../api/build.md)): `-debug` → `Debug`,
`-dev` → `RelWithDebInfo` (Development), `-release` → `Release`. Hidden presets `flavor-*` hold the
build types; platform presets combine `base`/`3ds-base` with one of them.

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug && ctest --preset pc-debug
scripts/dkp.sh cmake --preset 3ds-release && scripts/dkp.sh cmake --build --preset 3ds-release
```

Build folders: `build/<preset>/`. Personal presets go in `CMakeUserPresets.json` (git-ignored).

## Project layout in CMake

| File | Role |
|------|------|
| `CMakeLists.txt` | Project, C++20, platform detection (`NINTENDO_3DS` → `NNH_PLATFORM`), ccache, subdirectories |
| `engine/CMakeLists.txt` | Adds core + the preset's platform library; defines `nnh::engine` (INTERFACE: core + platform), what games link |
| `engine/core/CMakeLists.txt` | `nnh::core` (static) and `nnh::core_contract` (contract headers, INTERFACE) |
| `engine/platforms/<system>/CMakeLists.txt` | `nnh::platform_<system>` (static), links the system SDK `PRIVATE` |
| `cmake/nnh_engine_options.cmake` | `nnh_engine_options(target)`: C++20, warnings, no exceptions/RTTI |
| `cmake/nnh_game.cmake` | `nnh_add_game()` |
| `cmake/nnh_sdl3.cmake` | Provides `SDL3::SDL3` for the PC platform |
| `examples/CMakeLists.txt` | One `add_subdirectory` per example |
| `tests/CMakeLists.txt` | `nnh_tests` (core + fake), `nnh_conformance_<platform>` (PC only) |

## Common changes

**Add a source file**: core files go in `engine/core/CMakeLists.txt`; a system's files go in
`engine/platforms/<system>/CMakeLists.txt` (and every platform implements each new contract function,
fake included). Sources are always listed explicitly.

**Add a platform**: new folder `engine/platforms/<system>/` with a `nnh_platform_<system>` target and
presets selecting it ([platforms.md](../architecture/platforms.md)).

**Add an example**: `examples/<name>/CMakeLists.txt` with `nnh_add_game(...)`, then
`add_subdirectory(<name>)` in `examples/CMakeLists.txt`.

```cmake
nnh_add_game(scenes
    SOURCES main.cpp scenes/title_scene.cpp
    TITLE "Scenes demo" DESCRIPTION "NNHGE example" AUTHOR "NNHGE contributors"
    ICON icon.png)            # optional 48x48; default icon otherwise
```

On 3DS this calls devkitPro's `ctr_generate_smdh()` and `ctr_create_3dsx()`; on Windows it copies
`SDL3.dll` next to the executable.

**Add a test file**: list it in `tests/CMakeLists.txt`.

**Add a dependency**: avoid it. If unavoidable: small, permissive license, fetched with a pinned URL and
`URL_HASH SHA256=...`, linked `PRIVATE`, and the build-time impact measured (constitution III).

## Rules (Modern CMake)

- Targets and properties only: `target_include_directories`, `target_link_libraries`,
  `target_compile_options`. No `include_directories()`, `link_libraries()`, or editing `CMAKE_CXX_FLAGS`.
- Say `PUBLIC` / `PRIVATE` / `INTERFACE` explicitly. SDL3 is `PRIVATE` to the engine.
- Namespaced aliases for libraries (`nnh::engine`).
- No `file(GLOB)`: explicit lists make added/removed files visible in review and keep Ninja exact.
- Custom functions parse arguments with `cmake_parse_arguments(PARSE_ARGV ...)`.
- C++ standard through `CMAKE_CXX_STANDARD` / `target_compile_features`, never `-std=` flags.

## SDL3 for the simulator

`cmake/nnh_sdl3.cmake`: system SDL3 if found → official prebuilt package for MinGW or MSVC (no compile)
→ static build from source (Linux/macOS, needs SDL's [Linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux)).
Downloads are pinned with SHA-256. If HTTPS fails with "certificate signer not trusted" (WinLibs/MSYS2
CMake builds with GnuTLS and no CA store), the script reuses Git for Windows' CA bundle; otherwise set
`CMAKE_TLS_CAINFO`.

## devkitPro helpers available

From `C:\devkitPro\cmake\Platform\Nintendo3DS.cmake`: `ctr_generate_smdh`, `ctr_create_3dsx`
(`SMDH`, `ROMFS` options), `ctr_add_shader_library` (picasso), `ctr_add_graphics_target` (tex3ds:
`IMAGE`, `ATLAS`, `CUBEMAP`, `SKYBOX`). Platform flags: `-march=armv6k -mtune=mpcore -mfloat-abi=hard`,
`-D__3DS__`, links `-lctru -lm` by default.

## Build speed

- Ninja + ccache (auto-detected) → incremental game rebuild ≈ 1.3 s PC, 1.4 s 3DS (baseline in ROADMAP).
- The engine is a static library; game builds compile only game sources.
- Keep public headers light (see [cpp.md](cpp.md)). Measure before/after any build change:
  `time cmake --build --preset pc-debug` after touching one game `.cpp`.

## CI

**CI is manual** (maintainer's decision, 2026-11-17, to save Actions minutes): run it from the Actions
tab (CI → Run workflow) before releases or when a Linux/container check is needed. Day-to-day
verification happens locally (Windows: PC + 3DS presets) and on the homelab build VM (Linux).

`.github/workflows/ci.yml`: `docs` (broken links, layering check), `pc-linux` (installs SDL deps,
builds, `ctest`, runs `hello` headless with `SDL_VIDEO_DRIVER=offscreen`) and `nintendo-3ds` (container
`devkitpro/devkitarm`, uploads `.3dsx`). Job ids must start with a letter.

Linux builds and libFuzzer run on the homelab build VM (`ssh nnhge-build`, VM 120, off by default; see
CLAUDE.md "Build" for the start/sync/stop commands). Its devkitPro comes from the official
`devkitpro/devkitarm` image because `*.devkitpro.org` answers 403 to the maintainer's IP.

Before pushing, run the same checks locally:

```sh
python -I scripts/docs_tool.py check
cmake --build --preset pc-debug && ctest --preset pc-debug
scripts/dkp.sh cmake --build --preset 3ds-release
```

`.github/workflows/wiki.yml`: on push to `main` touching docs, regenerates the GitHub wiki from the
repository docs and pushes it ([documentation.md](../process/documentation.md)).

## Further reading

- [Effective Modern CMake](https://gist.github.com/mbinna/c61dbb39bca0e4fb7d1f73b0d66a4fd1)
- Henry Schreiner, [Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
- [cmake-presets(7)](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
