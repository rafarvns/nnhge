# CMake

One CMake project builds every target. The platform is chosen by the preset (toolchain), never by
options in game code.

## Presets

| Preset | Target | Toolchain |
|--------|--------|-----------|
| `pc-debug`, `pc-release` | PC simulator | host compiler (MinGW GCC, MSVC, clang) |
| `3ds-debug`, `3ds-release` | Nintendo 3DS | `$DEVKITPRO/cmake/3DS.cmake` (run via `scripts/dkp.sh` on Windows) |

```sh
cmake --preset pc-debug && cmake --build --preset pc-debug && ctest --preset pc-debug
scripts/dkp.sh cmake --preset 3ds-release && scripts/dkp.sh cmake --build --preset 3ds-release
```

Build folders: `build/<preset>/`. Personal presets go in `CMakeUserPresets.json` (git-ignored).

## Project layout in CMake

| File | Role |
|------|------|
| `CMakeLists.txt` | Project, C++20, platform detection (`NINTENDO_3DS` → `NNH_PLATFORM`), ccache, subdirectories |
| `engine/CMakeLists.txt` | `nnh_engine` static library (alias `nnh::engine`), per-platform backend sources |
| `cmake/nnh_game.cmake` | `nnh_add_game()` |
| `cmake/nnh_sdl3.cmake` | Provides `SDL3::SDL3` for the simulator |
| `examples/CMakeLists.txt` | One `add_subdirectory` per example |
| `tests/CMakeLists.txt` | `nnh_tests` (PC only) |

## Common changes

**Add a source file to the engine**: list it in `engine/CMakeLists.txt` (shared sources in
`add_library`, backend files in the matching platform block). Sources are always listed explicitly.

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

`.github/workflows/ci.yml`: `docs` (broken-link check with `scripts/docs_tool.py check`), `pc-linux`
(installs SDL deps, builds, `ctest`, runs `hello` headless with `SDL_VIDEO_DRIVER=offscreen`) and
`nintendo-3ds` (container `devkitpro/devkitarm`, uploads `.3dsx`). Job ids must start with a letter.

`.github/workflows/wiki.yml`: on push to `main` touching docs, regenerates the GitHub wiki from the
repository docs and pushes it ([documentation.md](../process/documentation.md)).

## Further reading

- [Effective Modern CMake](https://gist.github.com/mbinna/c61dbb39bca0e4fb7d1f73b0d66a4fd1)
- Henry Schreiner, [Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
- [cmake-presets(7)](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
