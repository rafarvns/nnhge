# Your first game

From nothing to a game running in the simulator and on your 3DS, tested by a script, installed on the HOME Menu, in
about fifteen minutes of your time (plus a few builds). Every command is `nnh`, the engine's tool
([the nnh tool](nnh-tool.md)). Commands are written for Git Bash on Windows; PowerShell, cmd and Linux shells take the
same `nnh` lines.

## 1. Install the tools

Install a C++ compiler, CMake, Ninja and devkitPro as [Getting started](getting-started.md#1-install-the-tools) says
for your system. You don't have to get everything right first time: step 3 tells you what is missing.

## 2. Get `nnh`

Until prebuilt releases exist (spec 034), `nnh` is built from an engine checkout, and that checkout is the engine your
games use:

```sh
git clone https://github.com/rafarvns/nnhge.git E:/src/nnhge
cd E:/src/nnhge
cmake --preset pc-release
cmake --build --preset pc-release --target nnh
export PATH="$PATH:/e/src/nnhge/build/pc-release/tools/nnh"      # PowerShell: $env:Path += ";E:/src/nnhge/build/pc-release/tools/nnh"
nnh --version
```

The last line prints `nnh 0.1.0 (protocol 1, link 1, windows-x64, from a source checkout)`. Put the `PATH` line in your
shell's profile (`~/.bashrc`, your PowerShell `$PROFILE`) to keep it.

## 3. Check your machine

```sh
nnh doctor
```

Each line is `ok`, `warn` or `FAIL`; every problem has a `fix:` line under it with the command to paste or the page to
open. Fix the `FAIL`s and run it again until it ends with `Simulator: ready` and `3DS: ready` (exit code 0). Only
using the simulator for now? `nnh doctor --pc` still shows the 3DS problems but doesn't count them.
[The nnh tool](nnh-tool.md#checking-your-machine-nnh-doctor) explains every check.

## 4. Make the project

Pick the folder that will hold your game's folder, then:

```sh
cd E:/games
nnh new my-game --title "My Game" --author "Me"
```

`nnh new` writes twelve files into `my-game/` ([what they are](nnh-tool.md#a-new-project-nnh-new)) and prints the next
commands. Because this `nnh` was built from a checkout, the project uses that checkout as its engine through the
environment variable `NNHGE_ENGINE_DIR`, and `nnh new` prints the lines that set it, for example:

```sh
export NNHGE_ENGINE_DIR=E:/src/nnhge        # Git Bash (PowerShell: $env:NNHGE_ENGINE_DIR = "E:/src/nnhge")
setx NNHGE_ENGINE_DIR E:/src/nnhge          # Windows: for every new shell too
```

Run the one for your shell (and `setx` once, so new shells have it). The name must be lowercase letters, digits, `_`
and `-`; `nnh new "My Game"` refuses and suggests `my-game`.

## 5. Run it in the simulator

```sh
cd my-game
nnh run my-game --pc
```

The first build compiles the engine into `build/pc-dev/` (a minute or two; later builds take seconds). A window opens
with both screens: move the sprite with the arrow keys (the D-Pad) or `I` `J` `K` `L` (the Circle Pad), press `X`
(the A button) for a sound and `Enter` (START) to quit. The keyboard map: [the simulator](../api/simulator.md).

## 6. Run it on your 3DS

On the 3DS, open the Homebrew Launcher and press **Y**. Then:

```sh
nnh run my-game
```

`nnh` builds the `.3dsx`, finds the console on your network, sends the game and shows its log; Ctrl+C stops following
it. Your console's log and the game's lines (`play: blip`, ...) appear in the terminal and in `build/logs/`.

## 7. Run the test script

`tests/smoke.nnhs` plays the game for you: it taps A, pushes the Circle Pad, takes a screenshot, presses START and
checks the log lines on the way.

```sh
nnh run my-game --pc --script tests/smoke.nnhs     # in the simulator
nnh run my-game --script tests/smoke.nnhs          # on the console (Homebrew Launcher, Y)
```

Both end with `nnh: PASS (...)` and exit 0. Script commands: [Testing on the console](testing-on-the-console.md).

## 8. Change the scene

Open `src/scenes/play_scene.cpp`. Change `kSpeed` to `4.0f`, or the text in `draw()`, and run `nnh run my-game --pc`
again: only that file is compiled and the game linked again, two or three seconds on a recent PC (three or four for
the 3DS). Every block of the
scene has a comment naming the API page it uses (`api/scene.md` is [docs/api/scene.md](../api/scene.md)). Keep the
log lines the test script expects, or change the script with them.

## 9. Add an asset

Drop a WAV file into `assets/sfx/` (say `assets/sfx/jump.wav`, 16-bit PCM), then load and play it like the blip:

```cpp
nnh::Sound jump_;                                                     // play_scene.hpp, next to blip_
if (auto s = nnh::load_sound("sfx/jump")) jump_ = std::move(s.value()); // on_enter()
if (nnh::input::pressed(nnh::Button::B)) nnh::audio::play(jump_);      // update()
```

Everything in `assets/` is packed with the game; images go in `assets/` too and load with `nnh::load_texture`.
Formats and limits: [api/assets.md](../api/assets.md), [api/audio.md](../api/audio.md), [api/texture.md](../api/texture.md).

## 10. Install it on the HOME Menu

```sh
nnh install my-game
```

On the 3DS, open FBI → Remote Install → Receive URLs over the network first; `nnh` builds the `.cia` and sends it.
The game then starts from the HOME Menu with the NNHGE icon and banner until you add your own
([Distributing your game](packaging.md)). `CIA UNIQUE_ID` in `CMakeLists.txt` identifies the game: keep it.

## Where to go next

- Your project's `README.md`: the files, the engine pin, upgrading, licences.
- [Game projects](game-projects.md): how the project brings in the engine, the presets, `nnh.toml`.
- The API: [scenes](../api/scene.md), [sprites](../api/gfx.md), [tilemaps and levels](levels.md), [UI](../api/ui.md),
  [saves](../api/saves.md); every feature has an example in the engine's `examples/` folder.
- `git init` in `my-game/` and commit: `.gitignore` already keeps the build folders out.
