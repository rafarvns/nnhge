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
built from source on first configure. For 3DS, install devkitPro pacman and `sudo dkp-pacman -S 3ds-dev`.

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
  /c/devkitPro/tools/bin/3dslink.exe build/3ds-release/examples/hello/hello.3dsx -a <console-ip>
  ```
  `-a` is optional when UDP broadcast works on your network.
- **By SD card**: copy the `.3dsx` to `sdmc:/3ds/` and open it from the Homebrew Launcher.
- **Emulator**: open the `.3dsx` in [Azahar](https://azahar-emu.org/).

## 5. Make your own game

Inside this repository, add a folder with a `CMakeLists.txt`:

```cmake
nnh_add_game(mygame
    SOURCES main.cpp
    TITLE "My Game"
    DESCRIPTION "A small game"
    AUTHOR "Me"
    ICON icon.png)   # optional, 48x48
```

and `add_subdirectory(mygame)` it from the top-level `CMakeLists.txt`. A standalone project template
(engine as a dependency) comes with the `nnh` CLI.
