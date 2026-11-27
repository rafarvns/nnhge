# Game projects

A game can live in its own folder and its own repository, with NNHGE as a pinned dependency
([spec 032](../../specs/032-game-projects/spec.md), decision D55). This page writes such a project by hand; `nnh new`
(spec 033) will generate the same files. The engine repository's own `tests/consumer/` is a complete example, built by
the engine's tests in every way below.

```
mygame/
  CMakeLists.txt        the project: brings the engine in, adds the game's folders
  CMakePresets.json     the six presets nnh uses (copy it from below)
  nnh.toml              marks the folder as a game project for nnh
  .gitignore            build/ and your local files
  game/
    CMakeLists.txt      nnh_add_game(mygame SOURCES ... ASSETS assets ...)
    src/main.cpp
    assets/             images, sounds, Tiled maps, data files, fonts, strings, models
```

## The project's `CMakeLists.txt`

The engine is a CMake subproject: once it is added, `nnh_add_game()` works from any folder of the project with every
keyword it has ([cmake.md](../tech/cmake.md#game-projects-the-engine-as-a-subproject)). Four ways of adding it give the
same result; pick one.

**1. A pinned download** (the default): an engine source archive by URL and SHA-256. CMake checks the hash before
anything is built; a wrong hash or an unreachable URL fails the configure, naming the URL.

```cmake
cmake_minimum_required(VERSION 3.25)
project(mygame)

include(FetchContent)
FetchContent_Declare(nnhge
    URL https://github.com/rafarvns/nnhge/releases/download/vX.Y.Z/nnhge-X.Y.Z-src.tar.gz
    URL_HASH SHA256=<the release's SHA-256>
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nnhge)

if(NNHGE_VERSION VERSION_LESS 0.1)
    message(FATAL_ERROR "mygame needs NNHGE 0.1 or later (found ${NNHGE_VERSION})")
endif()

add_subdirectory(game)
```

Release archives with their SHA-256 are spec 034's; until then point `URL` at an archive you made
(`git archive --format=tar.gz --prefix=nnhge/ -o nnhge.tar.gz HEAD` in an engine checkout) or use a local engine
(below). Pin an uploaded release file, never GitHub's automatic "Source code" archives: those are not promised to keep
their bytes, so their hash can change. A local archive's URL is `file://` followed by the path as CMake writes it:
`file://E:/archives/nnhge.tar.gz` on Windows (`file:///E:/...` doesn't work), `file:///home/me/nnhge.tar.gz` on Linux.

**2. The same, with your local engine checkout** instead of the download, without editing `CMakeLists.txt`: see
[a local engine](#a-local-engine).

**3. A git submodule:**

```cmake
cmake_minimum_required(VERSION 3.25)
project(mygame)
add_subdirectory(nnhge)        # git submodule add https://github.com/rafarvns/nnhge.git nnhge
add_subdirectory(game)
```

**4. A copied folder** (an unpacked archive or a plain copy): the same `add_subdirectory(nnhge)`. The copy may leave out
`examples/`, `tests/`, `apps/`, `specs/` and `docs/`: the engine configures without them.

Nothing else is needed: no `include()`, path or variable. Write no engine path in the game's `CMakeLists.txt`.

## `CMakePresets.json`

`nnh` uses the engine repository's six presets and folders (`build/<preset>/`). Copy this file as it is; the one
addition is the mapping of `NNHGE_ENGINE_DIR` ([a local engine](#a-local-engine)):

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "FETCHCONTENT_SOURCE_DIR_NNHGE": "$env{NNHGE_ENGINE_DIR}"
      }
    },
    { "name": "flavor-debug", "hidden": true, "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" } },
    { "name": "flavor-dev", "hidden": true, "cacheVariables": { "CMAKE_BUILD_TYPE": "RelWithDebInfo" } },
    { "name": "flavor-release", "hidden": true, "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" } },
    { "name": "3ds-base", "hidden": true, "inherits": "base", "toolchainFile": "$env{DEVKITPRO}/cmake/3DS.cmake" },
    { "name": "pc-debug", "inherits": ["base", "flavor-debug"] },
    { "name": "pc-dev", "inherits": ["base", "flavor-dev"] },
    { "name": "pc-release", "inherits": ["base", "flavor-release"] },
    { "name": "3ds-debug", "inherits": ["3ds-base", "flavor-debug"] },
    { "name": "3ds-dev", "inherits": ["3ds-base", "flavor-dev"] },
    { "name": "3ds-release", "inherits": ["3ds-base", "flavor-release"] }
  ],
  "buildPresets": [
    { "name": "pc-debug", "configurePreset": "pc-debug" },
    { "name": "pc-dev", "configurePreset": "pc-dev" },
    { "name": "pc-release", "configurePreset": "pc-release" },
    { "name": "3ds-debug", "configurePreset": "3ds-debug" },
    { "name": "3ds-dev", "configurePreset": "3ds-dev" },
    { "name": "3ds-release", "configurePreset": "3ds-release" }
  ],
  "testPresets": [
    { "name": "pc-debug", "configurePreset": "pc-debug" },
    { "name": "pc-dev", "configurePreset": "pc-dev" },
    { "name": "pc-release", "configurePreset": "pc-release" }
  ]
}
```

A project without the preset `nnh` needs gets `nnh: this project has no preset "3ds-dev" (CMakePresets.json)` before
CMake runs. Presets of your own go in `CMakeUserPresets.json` (git-ignored), under other names; pick one with
`NNH_PRESET` ([nnh-tool.md](nnh-tool.md)).

## `nnh.toml`

A file named `nnh.toml` marks the folder as a game project: `nnh build`, `run`, `install` and `serve` work from it and
from any folder below it, exactly as in the engine repository. An empty file is valid; two keys are read today:

```toml
name = "mygame"       # the project's name
engine = "0.1.0"      # the engine version the project was made for (spec 033's nnh doctor compares it)
```

Other keys are reported as `nnh.toml:<line>: unknown key "x" (ignored)`; a malformed line stops `nnh` with
`nnh.toml:<line>: <reason>`. `nnh` uses the nearest folder that holds `nnh.toml` or is the engine repository, so a game
project inside another folder (even inside the engine repository) is found as itself.

## `.gitignore`

```
build/
CMakeUserPresets.json
```

## Where outputs go

Everything is written under the project's `build/<preset>/`: the games manifest `nnh` reads (`nnh_games.txt`), the
executables, `.3dsx`, romfs, `dist/` and `.cia`, the engine's own downloads (SDL3 for the simulator, makerom and
bannertool for `.cia`), the host programs (`nnh-assetc`, data schema programs) and the engine's build
(`_deps/nnhge-build/` with FetchContent). Logs and screenshots of `nnh run` go to `build/logs/`. Nothing is written into
the engine's folder, so one engine checkout serves several projects, and a downloaded copy stays as downloaded.

Each preset downloads the pinned archive once into its own build folder (a few MB). For a slow or no connection, unpack
the archive once and use it as [a local engine](#a-local-engine).

## A local engine

Set the environment variable `NNHGE_ENGINE_DIR` to an engine checkout (or an unpacked archive), in the usual drive form
on Windows:

```sh
export NNHGE_ENGINE_DIR=E:/src/nnhge      # Git Bash; setx NNHGE_ENGINE_DIR E:\src\nnhge for every new shell
nnh run mygame --pc
```

The presets above turn it into `FETCHCONTENT_SOURCE_DIR_NNHGE`, so no download happens and your engine changes show up
at the next build. Empty or unset: the pinned download. `nnh` passes the same folder to 3DS configures in the form
devkitPro's CMake reads. Every configure prints which engine it used (`-- NNHGE 0.1.0: E:/src/nnhge`). The variable
applies to every game project on the machine: set it per shell when you work on one project only. An existing build
folder keeps the engine it was configured with: after setting or clearing the variable, delete `build/<preset>/` (or
pass `-DFETCHCONTENT_SOURCE_DIR_NNHGE=...` once).

## Upgrading the engine

Change `URL` and `URL_HASH` together (the two lines of the release notes), then configure again: the new archive is
downloaded into each build folder. Read the engine's version in CMake to refuse an engine that is too old:
`NNHGE_VERSION` and `NNHGE_VERSION_MAJOR`, `_MINOR`, `_PATCH`, set as soon as the engine is added.

## What the engine adds to your build

- Only what a game needs: the engine's libraries, the simulator's SDL3, the host programs. Its examples, unit tests,
  NNH Link and the `nnh` tool are not configured (their options `NNH_BUILD_EXAMPLES`, `NNH_BUILD_TESTS`,
  `NNH_BUILD_APPS`, `NNH_BUILD_TOOLS` are OFF in a game project; set one ON to build that part). Your `ctest` lists only
  your own tests.
- Your game's targets compile with C++20 (no compiler extensions) and the compiler cache when one is installed, like the
  engine's examples, unless your project sets `CMAKE_CXX_STANDARD`, `CMAKE_CXX_EXTENSIONS` or a compiler launcher
  itself. `-DNNH_CCACHE=` turns the cache off. Your project's other targets are left as they are.
- Target names the engine claims (don't use them for your own targets) and cache entries:
  [cmake.md](../tech/cmake.md#reserved-names).

## 3DS builds on Windows

`nnh build` and `nnh run` configure 3DS presets through devkitPro's MSYS2 shell and pass what it lacks: your host C++
compiler as `NNH_HOST_CXX` (`NNH_HOST_CXX` from the environment, else `g++`, `c++` or `clang++` on your `PATH`; the build
compiles `nnh-assetc` and your data schema program with it) and `NNHGE_ENGINE_DIR` in `/e/...` form. With no host
compiler, a game with Tiled maps, Aseprite exports, data assets or fonts fails to configure with a message naming
`NNH_HOST_CXX` and `NNH_ASSETC`.

Configuring a 3DS preset by hand goes through `scripts/dkp.sh` of an engine checkout (it sets `NNH_HOST_CXX`), with
paths in MSYS form, because devkitPro's CMake reads `E:/...` as a relative path:

```sh
/e/src/nnhge/scripts/dkp.sh sh -c "cd /e/games/mygame && cmake --preset 3ds-dev -DFETCHCONTENT_SOURCE_DIR_NNHGE=/e/src/nnhge"
/e/src/nnhge/scripts/dkp.sh sh -c "cd /e/games/mygame && cmake --build --preset 3ds-dev"
```

(Leave out the `-D` for the pinned download.)

Windows limits one command line to 32,767 characters. The engine archives its core library from a response file in 3DS
presets, so its longest command stays short (about 3,100 characters in a game project). A game's own link command lists every
object of the game, relative to the build folder (about 50 characters each: `game/CMakeFiles/mygame.dir/src/player.o`):
a game of about 300 source files reaches half the limit, so a game that large should check its build with the
engine's `scripts/check_command_length.sh build/3ds-dev`.

**Certificates.** Some Windows CMake builds have no CA certificates, so the `https://` download of the engine fails with
"certificate signer not trusted". Set `CMAKE_TLS_CAINFO` to a CA bundle (Git for Windows ships one:
`C:/Program Files/Git/mingw64/etc/ssl/certs/ca-bundle.crt`), or use a local engine.

## See also

- [Getting started](getting-started.md) — tools, the first build.
- [The nnh tool](nnh-tool.md) — build, run, install, serve.
- [cmake.md](../tech/cmake.md) — the subproject contract, options, reserved names.
