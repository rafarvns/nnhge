# Getting started

## 1. Install the tools

### Windows (primary platform)

1. **Host compiler, CMake, Ninja**: [WinLibs](https://winlibs.com/) MinGW-w64 GCC (UCRT, POSIX threads)
   ships all three: `winget install BrechtSanders.WinLibs.POSIX.UCRT`.
   Git for Windows provides the shell (Git Bash) used by the scripts.
2. **devkitPro** (3DS toolchain): run the graphical installer from
   <https://github.com/devkitPro/installer/releases> and select **3DS Development**. It installs to
   `C:\devkitPro`, with its own MSYS2.
3. **Ninja inside devkitPro's MSYS2** (devkitPro's CMake toolchain only runs from there):
   ```sh
   /c/devkitPro/msys2/usr/bin/bash.exe -lc 'pacman -S --needed --noconfirm ninja'
   ```

The first PC configure downloads the SDL3 development package (prebuilt, ~5 MB) into the build folder.
If CMake reports `certificate signer not trusted`, the build reuses Git for Windows' CA bundle
automatically; otherwise set the environment variable `CMAKE_TLS_CAINFO` to a CA bundle file.

### Linux / macOS (not yet tested)

Install CMake ≥ 3.25, Ninja and a C++20 compiler. SDL3 is used from the system if installed, otherwise
built from source on first configure; on Linux that needs SDL's build dependencies
(list: <https://wiki.libsdl.org/SDL3/README-linux>; the Ubuntu package names are in `.github/workflows/ci.yml`). For 3DS, install devkitPro pacman and `sudo dkp-pacman -S 3ds-dev`.

## 2. Build and run on PC (simulator)

```sh
cmake --preset pc-debug
cmake --build --preset pc-debug
build/pc-debug/examples/hello/hello.exe
```

Keyboard mapping: see [api/input.md](../api/input.md). Simulator options: [api/simulator.md](../api/simulator.md).

## 3. Build for 3DS

From Git Bash (the script runs the command inside devkitPro's environment):

```sh
scripts/dkp.sh cmake --preset 3ds-release
scripts/dkp.sh cmake --build --preset 3ds-release
```

Output: `build/3ds-release/examples/hello/hello.3dsx`.

## 4. Run on a 3DS

The console needs custom firmware with the Homebrew Launcher.

- **Over Wi-Fi**: open the Homebrew Launcher, press **Y** (netloader), then on the PC:
  ```sh
  cmake --build --preset pc-debug                    # builds the nnh tool too
  build/pc-debug/tools/nnh/nnh run hello             # build, send, show the game's log
  ```
  `nnh` finds the console and remembers it; `--console <ip>` sets it ([the nnh tool](nnh-tool.md)).
  devkitPro's `3dslink <file>.3dsx -a <console-ip>` works too. To let the PC press buttons, take
  screenshots and check the log for you, see [Testing on the console](testing-on-the-console.md).
- **From the console**: with **NNH Link** on the console and `nnh serve` on the PC, pick a game on the
  console; the PC builds it and the console runs it ([NNH Link](nnh-link.md)).
- **By SD card**: copy the `.3dsx` to `sdmc:/3ds/` and open it from the Homebrew Launcher.
- **Emulator**: open the `.3dsx` in [Azahar](https://azahar-emu.org/).

## 5. Make your own game

A game lives best in its own folder and repository, with the engine as a pinned dependency: a `CMakeLists.txt` that
adds the engine (a hash-pinned download, your local engine checkout, a git submodule or a copied folder), the six
presets and an `nnh.toml`, written by hand today ([Game projects](game-projects.md)); `nnh build`, `run`, `install`
and `serve` work from that folder as they do here. Whichever way, the game's own folder has a `CMakeLists.txt`:

```cmake
nnh_add_game(mygame
    SOURCES main.cpp
    TITLE "My Game"
    DESCRIPTION "A small game"
    AUTHOR "Me"
    ICON icon.png)   # optional, 48x48
```

added with `add_subdirectory(mygame)` from the project's `CMakeLists.txt` (or, to try something quickly inside this
repository, from its top-level one). Images, sounds and levels go in an
assets folder (`ASSETS assets`, [api/assets.md](../api/assets.md)); levels can be drawn in Tiled
([Levels in Tiled](levels.md)) or built in code ([api/tilemap.md](../api/tilemap.md)). A generated project
(`nnh new`) comes with spec 033. To give it to players — a `.3dsx`, or a `.cia` with
a banner for the HOME Menu, installed over the network with FBI — see
[Distributing your game](packaging.md).
