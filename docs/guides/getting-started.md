# Getting started

Install the tools for your system, get `nnh`, let `nnh doctor` check the result, then make a game with `nnh new`:
[Your first game](your-first-game.md) walks the whole path. Working on the engine itself is at the end
([Working inside the engine repository](#working-inside-the-engine-repository)).

## 1. Install the tools

### Windows (the main platform)

1. **Host compiler, CMake, Ninja**: [WinLibs](https://winlibs.com/) MinGW-w64 GCC (UCRT, POSIX threads) ships all
   three: `winget install BrechtSanders.WinLibs.POSIX.UCRT`. Git for Windows (`winget install Git.Git`) gives Git Bash
   and `git`.
2. **devkitPro** (the 3DS toolchain): run the graphical installer from
   <https://github.com/devkitPro/installer/releases> and select **3DS Development**. It installs to `C:\devkitPro`,
   with its own MSYS2.
3. **CMake and Ninja inside devkitPro's MSYS2** (`nnh` builds for the 3DS there):
   ```sh
   C:/devkitPro/msys2/usr/bin/bash.exe -lc "pacman -S --needed cmake ninja"
   ```
   (the same line works in PowerShell, cmd and Git Bash).

The first PC configure downloads the SDL3 development package (prebuilt, ~5 MB) into the build folder. If CMake reports
`certificate signer not trusted`, the build reuses Git for Windows' CA bundle automatically; otherwise set the
environment variable `CMAKE_TLS_CAINFO` to a CA bundle file (`nnh doctor --online` checks it).

### Linux

CMake ≥ 3.25, Ninja and GCC ≥ 12 or clang ≥ 16 (the project checks GCC 13 and 14 and clang 18 on Ubuntu 24.04). SDL3 is
used from the system if installed, otherwise built from source on the first configure, which needs `pkg-config` and
SDL's build dependencies; on Debian or Ubuntu:

```sh
sudo apt install cmake ninja-build g++ pkg-config libx11-dev libxext-dev libwayland-dev libxkbcommon-dev \
                 libasound2-dev libpulse-dev
```

For the 3DS, install [devkitPro pacman](https://devkitpro.org/wiki/devkitPro_pacman), then `sudo dkp-pacman -S
3ds-dev` and `export DEVKITPRO=/opt/devkitpro` (in `~/.profile`).

### macOS

Best effort, not checked by the project: Xcode's command-line tools (`xcode-select --install`), `brew install cmake
ninja`, devkitPro pacman for the 3DS. `nnh doctor` reports macOS as `warn`.

## 2. Get `nnh`

Until prebuilt `nnh` releases exist (spec 034), build it from an engine checkout; that checkout is then the engine your
projects use (through `NNHGE_ENGINE_DIR`, which `nnh new` explains):

```sh
git clone https://github.com/rafarvns/nnhge.git
cd nnhge
cmake --preset pc-release
cmake --build --preset pc-release --target nnh      # -> build/pc-release/tools/nnh/nnh(.exe)
```

Add `build/pc-release/tools/nnh` to your `PATH` ([Your first game](your-first-game.md#2-get-nnh) shows the lines).

## 3. Check with `nnh doctor`

```sh
nnh doctor
```

One line per tool (`ok`, `warn` or `FAIL`) and a `fix:` line under each problem: the command to paste or the page to
open. It ends with one verdict per group (`Simulator: ready`, `3DS: not ready (1 problem)`) and exits 0 when everything
required is there. With `--pc` the 3DS group is still checked and shown, but its problems don't count in the exit
code (for simulator-only work); `--online` also tries the downloads a first build makes and looks for your console. Details: [the nnh tool](nnh-tool.md#checking-your-machine-nnh-doctor).

## 4. Make a game

```sh
nnh new my-game
```

Then follow [Your first game](your-first-game.md): run it in the simulator and on the console, run its test script,
change it, install it on the HOME Menu. A project made by hand, or one that adds the engine as a git submodule or a
copied folder, works the same way: [Game projects](game-projects.md).

## Working inside the engine repository

For engine contributors: the engine repository is itself a project `nnh` works in, with every example and test.

### Build and run on PC (simulator)

```sh
cmake --preset pc-debug
cmake --build --preset pc-debug
build/pc-debug/examples/hello/hello.exe
ctest --preset pc-debug
```

Keyboard mapping: see [api/input.md](../api/input.md). Simulator options: [api/simulator.md](../api/simulator.md).

### Build for the 3DS

From Git Bash (the script runs the command inside devkitPro's environment):

```sh
scripts/dkp.sh cmake --preset 3ds-release
scripts/dkp.sh cmake --build --preset 3ds-release
```

Output: `build/3ds-release/examples/hello/hello.3dsx`. `nnh build hello` does the same without the script.

### Run on a 3DS

The console needs custom firmware with the Homebrew Launcher.

- **Over Wi-Fi**: open the Homebrew Launcher, press **Y** (netloader), then on the PC:
  ```sh
  build/pc-debug/tools/nnh/nnh run hello             # build, send, show the game's log
  ```
  `nnh` finds the console and remembers it; `--console <ip>` sets it ([the nnh tool](nnh-tool.md)).
  devkitPro's `3dslink <file>.3dsx -a <console-ip>` works too. To let the PC press buttons, take
  screenshots and check the log for you, see [Testing on the console](testing-on-the-console.md).
- **From the console**: with **NNH Link** on the console and `nnh serve` on the PC, pick a game on the
  console; the PC builds it and the console runs it ([NNH Link](nnh-link.md)).
- **By SD card**: copy the `.3dsx` to `sdmc:/3ds/` and open it from the Homebrew Launcher.
- **Emulator**: open the `.3dsx` in [Azahar](https://azahar-emu.org/).

### A game inside the repository

To try something quickly, a folder with a `CMakeLists.txt` added by `add_subdirectory()` from the top-level one:

```cmake
nnh_add_game(mygame
    SOURCES main.cpp
    TITLE "My Game"
    DESCRIPTION "A small game"
    AUTHOR "Me"
    ICON icon.png)   # optional, 48x48
```

Images, sounds and levels go in an assets folder (`ASSETS assets`, [api/assets.md](../api/assets.md)); levels can be
drawn in Tiled ([Levels in Tiled](levels.md)) or built in code ([api/tilemap.md](../api/tilemap.md)). To give a game
to players — a `.3dsx`, or a `.cia` with a banner for the HOME Menu, installed over the network with FBI — see
[Distributing your game](packaging.md).
