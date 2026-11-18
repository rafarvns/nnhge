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
| `cmake/nnh_assets.cmake` | `nnh_add_assets()` (the `ASSETS` folder of `nnh_add_game`), with `nnh_asset_index.cmake` (the index and its name checks) |
| `cmake/nnh_images.cmake` | `nnh_add_images()` (the `IMAGES` of `nnh_add_game`), with the build-time scripts `nnh_check_image.cmake` and `nnh_embed.cmake` |
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
nnh_add_game(sprites
    SOURCES main.cpp
    ASSETS assets                       # optional folder: assets/art/hero.png -> load_texture("art/hero")
    IMAGES art/logo.png                 # optional embedded images: load_texture("logo")
    TITLE "Sprites demo" DESCRIPTION "NNHGE example" AUTHOR "NNHGE contributors"
    VERSION 1.0.0                       # optional, default 0.1.0 (major.minor.micro, at most 63.63.15)
    ICON icon.png                       # optional 48x48 PNG; the NNHGE icon otherwise
    CIA UNIQUE_ID 0xE0A01)              # optional: an installable .cia (spec 009, see below)
```

On 3DS this calls devkitPro's `ctr_generate_smdh()` and `ctr_create_3dsx()`; on Windows it copies
`SDL3.dll` next to the executable. **Package metadata** (spec 009, `cmake/nnh_package_rules.cmake`) is
checked at configure time, naming the field: title and author at most 64 characters, description 128
(counted in UTF-8 characters), `VERSION` major and minor 0–63 and micro 0–15; with `CIA`, a `UNIQUE_ID`
in the homebrew range `0x00300`–`0xF7FFF`, an optional `PRODUCT_CODE` like `CTR-H-ABCD` (default
`CTR-H-` + the target's first 4 letters), optional `BANNER` (256×128 PNG) and `BANNER_AUDIO` (16-bit
WAV, at most 3 s), with NNHGE defaults from `cmake/3ds/` (drawn by `scripts/make_package_art.py`). The
icon is checked by a build step on every system (`nnh_check_icon.cmake`), so a wrong icon fails on PC
too. `TITLE` is also compiled into the game as `NNH_GAME_TITLE`, the default `AppConfig::title`. Script
tests: `icon_check_*`, `banner_check_*`, `package_check_*`, `package_resolve_*`, `game_title_*`
(fixtures in `tests/data/package/`).

**Distribution** (3DS, spec 009, `cmake/nnh_cia.cmake`): none of these is part of the normal build. `<game>_dist` copies the `.3dsx` (and, with `CIA`, the `.cia`) into `build/<preset>/dist/<game>/`; `dist` does it for every game; `<game>_cia` builds `dist/<game>/<game>.cia` with makerom from the same ELF, SMDH and assets folder (as romfs; an empty one without `ASSETS`), our RSF `cmake/3ds/nnhge.rsf` and a banner made by bannertool. The tools are fetched by the target `nnh_ctr_tools` the first time a `.cia` is built (`cmake/nnh_fetch_ctr_tools.cmake`: makerom v0.19.0 and bannertool 1.2.3, Windows and Linux x86-64, pinned by SHA-256, into `build/<preset>/_tools/`), so configuring and building games downloads nothing. Offline or on macOS, configure with `-DNNH_MAKEROM=<path> -DNNH_BANNERTOOL=<path>`.

**Assets** (`ASSETS <folder>`, spec 007, the recommended way): every file of the folder becomes an asset
named by its relative path ([api/assets.md](../api/assets.md)). One build step per file, rerun only when
that file changes, writes into `build/<preset>/<game dir>/assets/<path>`: PNGs are checked like images
(below) and, on 3DS, converted by `tex3ds` under the same name; `.wav` and `.ogg` files are checked by
`nnh_check_sound.cmake` (header only: PCM 8/16-bit WAV or Ogg Vorbis, mono or stereo, 8000–48000 Hz —
`jump.wav is 24-bit: save it as PCM 16-bit (or 8-bit)`; ctest cases `sound_check_*`, fixtures in
`tests/data/sounds/`) and copied; other files are copied. Then
`nnh_asset_index.cmake` writes `assets/.nnh_assets` (the sorted list core reads at init) and fails the
build for names that break the asset path rules, use characters outside `A-Z a-z 0-9 space - _ .`, are
reserved, or differ from another only by letter case; it also deletes outputs of removed assets. On PC
that folder is next to the executable; on 3DS the target `<game>_assets` is a devkitPro asset target
passed as `ROMFS` to `ctr_create_3dsx`, so the `.3dsx` is repacked when an asset changes. The folder is
scanned with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` — the one exception to "no GLOB" below: assets
are data, and adding a file must not require editing CMake. Script tests: `asset_index_*` (fixtures in
`tests/data/asset_lists/`).

**Images** (`IMAGES`, decision D19: embedded in the executable, for small games; `ASSETS` is the
recommended way): one build step per image, rerun only when that image changes:

1. `nnh_check_image.cmake` reads the PNG header and fails the build unless both sides are powers of two
   from 8 to 1024 — `hero.png is 100x60: textures need power-of-two sides from 8 to 1024 ...`;
2. on 3DS, `tex3ds -f rgba8888 -z auto` converts it to a `.t3x`; on PC the PNG stays as is;
3. `nnh_embed.cmake` writes `image_<name>.cpp` (a byte array).

A registry `images.cpp` maps names (file name without folder and extension; duplicates are a configure
error) to the arrays; games without `IMAGES` link an empty default from core. Generated files live in
`build/<preset>/<game dir>/nnh_images/<game>/`. The check script has its own `ctest` cases
(`image_check_*`, fixtures in `tests/data/images/`).

**Host tools** (`tools/`, PC presets only, never a dependency of games): `tools/nnh/CMakeLists.txt` builds the `nnh` program (spec 010) from `nnh_tool_lib` and its tests `nnh_tool_tests` (ctest `nnh_tool`). With MinGW they link the C++ runtime statically (`-static`): Git for Windows puts an older `libstdc++-6.dll` first on the PATH, which made the test program fail to start (exit code 127, no output) as soon as it used `std::filesystem`.

**Shared link library** (`link/`, every preset, spec 011): `nnh_link` holds the network code `nnh` and the
NNH Link console app share (sockets, SHA-256/HMAC, message codec, pairing). It never depends on
`nnh::core`, so the tool doesn't pull the engine in; on the 3DS it builds against libctru's sockets.
Its tests `nnh_link_tests` (ctest `nnh_link`, also static on MinGW) include a mini-fuzz; with
`-DNNH_BUILD_FUZZERS=ON` and clang, `fuzz_link_{messages,frames,discovery}` are built in `build/<preset>/link/`.

**Add a test file**: list it in `tests/CMakeLists.txt`.

**Add a dependency**: avoid it. If unavoidable: small, permissive license, fetched with a pinned URL and
`URL_HASH SHA256=...`, linked `PRIVATE`, and the build-time impact measured (constitution III). A
single-file library may instead be vendored, unmodified, in `third_party/<name>/` with a `README.md`
(upstream, commit, SHA-256, license) and its own `CMakeLists.txt` building an object library with
warnings off — like `third_party/stb_vorbis` (spec 008), linked into `nnh_core`.

## Rules (Modern CMake)

- Targets and properties only: `target_include_directories`, `target_link_libraries`,
  `target_compile_options`. No `include_directories()`, `link_libraries()`, or editing `CMAKE_CXX_FLAGS`.
- Say `PUBLIC` / `PRIVATE` / `INTERFACE` explicitly. SDL3 is `PRIVATE` to the engine.
- Namespaced aliases for libraries (`nnh::engine`).
- No `file(GLOB)` for sources: explicit lists make added/removed files visible in review and keep Ninja
  exact. (Asset folders are the exception, see `ASSETS` above.)
- Custom functions parse arguments with `cmake_parse_arguments(PARSE_ARGV ...)`.
- C++ standard through `CMAKE_CXX_STANDARD` / `target_compile_features`, never `-std=` flags.
- Build-step scripts run with `cmake -P` (`cmake/nnh_*.cmake` with `-D` arguments) start with
  `cmake_minimum_required(VERSION 3.25)`: a `-P` script doesn't inherit the project's policies. Without
  it, CMake 3.x rejects `if(x IN_LIST y)`; CMake 4 (Windows) silently accepts it, so the bug only shows on
  Linux. Test such scripts with a ctest (`tests/data/asset_lists/`).

## SDL3 for the simulator

`cmake/nnh_sdl3.cmake`: system SDL3 if found → official prebuilt package for MinGW or MSVC (no compile)
→ static build from source (Linux/macOS, needs SDL's [Linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux)).
Downloads are pinned with SHA-256. If HTTPS fails with "certificate signer not trusted" (WinLibs/MSYS2
CMake builds with GnuTLS and no CA store), the script reuses Git for Windows' CA bundle; otherwise set
`CMAKE_TLS_CAINFO`.

## devkitPro helpers available

From `C:\devkitPro\cmake\Platform\Nintendo3DS.cmake`: `ctr_generate_smdh`, `ctr_create_3dsx`
(`SMDH`, `ROMFS` options), `ctr_add_shader_library` (picasso), `ctr_add_graphics_target` (tex3ds:
`IMAGE`, `ATLAS`, `CUBEMAP`, `SKYBOX`; we call `tex3ds` directly through `CTR_TEX3DS_EXE` instead, to
keep one step per image). Platform flags: `-march=armv6k -mtune=mpcore -mfloat-abi=hard`,
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
