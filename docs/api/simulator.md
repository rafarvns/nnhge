# PC simulator

The PC build of a game is a native desktop app (SDL3) that behaves like the 3DS:

- One window with both screens, by default the top screen (400×240) above the bottom screen (320×240,
  centered), scaled by an integer factor (`AppConfig::pc_window_scale`, default 2). Other layouts,
  scales and fullscreen: see [Window](#window). Pixels always stay sharp (integer scales).
- Runs at 60 Hz like the console, even on faster monitors.
- Controls: keyboard, mouse (touch screen) and a gamepad — see [input.md](input.md#pc-simulator-controls).

It is not an emulator: use [Azahar](https://azahar-emu.org/) or a real console to check console behaviour.
Enforcement of 3DS memory and texture limits arrives with the features that need them.

## Window

Keys that change how the screens are shown (every build, including Release; the game doesn't notice):

| Key | Action |
|-----|--------|
| `F2` | next layout: Stacked → Side by side → Large top → Top only → Bottom only |
| `F3` / `F4` | scale down / up (1× to 6×); the window resizes to fit the screens |
| `F5` | fit: the largest integer scale that fits the window, screens centered |
| `F9` or `Alt+Enter` | fullscreen on / off (always "fit" while fullscreen) |

| Layout | Screens |
|--------|---------|
| Stacked | like the console: top above, bottom centered below |
| Side by side | top on the left, bottom on the right |
| Large top | top big, bottom at half its scale to the right (bottom-aligned) |
| Top only / Bottom only | one screen; with Top only there is no touch screen |

The window title shows the new layout and scale for two seconds. The mouse touches the bottom screen
wherever the layout draws it ([input.md](input.md#pc-simulator-controls)).

**Remembered**: layout, scale, fit, fullscreen and the window's position and size are saved when the game
closes and restored next time, per game, in the user's preferences — on Windows
`%APPDATA%\NNHGE simulator\<data folder>\window.ini` — never in the game's data folder or saves. Delete
the file to go back to the defaults; a damaged file is ignored, and so is a saved position that is no
longer on any monitor.

**Screenshots and scripted runs** (`NNH_EXIT_AFTER_FRAMES`, `NNH_SCREENSHOT`, `SDL_VIDEO_DRIVER=offscreen`)
neither read nor write that file: they use the stacked layout at `AppConfig::pc_window_scale` (default 2),
so reference images don't depend on anyone's preferences. To capture another layout, set
`NNH_SIM_LAYOUT` / `NNH_SIM_SCALE` (below).

## The 3D slider and the eyes

The simulator has `Capability::Stereo3D` and a 3D slider of its own (spec 026 FR-16/FR-22; what it does to 3D:
[gfx3d.md](gfx3d.md#stereo)). Keys work in every build:

| Key | Action |
|-----|--------|
| `F6` / `F7` | slider down / up by 0.1 (with Shift: 0.01), 0 to 1 |
| `F8` | what the top screen shows when a frame draws two eyes: left eye → right eye → red/cyan anaglyph |

The title shows "3D slider 0.50, right eye" for two seconds. The game reads the slider with
`nnh::system::stereo_slider()`. Frames drawn with one picture (slider 0, no 3D draws, orthographic cameras) look
the same in every view. The anaglyph (left eye × red + right eye × cyan, added by the renderer) is a preview for red/cyan
glasses; the console has the real thing. Slider and view are remembered with the window settings (`slider=0.50`,
`eye=left|right|anaglyph` in `window.ini`); screenshots and scripted runs start at slider 0, left eye, unless
`NNH_SIM_SLIDER` / `NNH_SIM_EYE` say otherwise; a run driven by `nnh run --pc` shows the left eye, and while a script
runs the slider is the script's (`slider V`), not the simulator's.

```sh
NNH_SIM_SLIDER=1 NNH_SIM_EYE=right NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=right.png build/pc-debug/examples/models/models.exe
```

## Developer keys (Debug and Development builds)

| Key | Action |
|-----|--------|
| `F1` | toggle the debug overlay |
| `Pause` | pause / resume the game loop (the last frame stays on screen) |
| `F10` | while paused: run one frame |
| `F11` | slow motion (15 Hz) on/off |
| `F12` | screenshot to `<data folder>/screenshots/shot-NNN.png` |

## Environment variables (for tests and CI)

| Variable | Effect |
|----------|--------|
| `NNH_EXIT_AFTER_FRAMES=<n>` | `nnh::frame()` returns false after *n* frames. |
| `NNH_SCREENSHOT=<file.png>` | With `NNH_EXIT_AFTER_FRAMES`, saves the last frame as PNG. |
| `NNH_SIM_LAYOUT=<layout>` | Layout for this run: `stacked`, `side`, `large`, `top`, `bottom`. Disables the remembered settings. |
| `NNH_SIM_SCALE=<n>` | Scale for this run: `1`–`6`, or `fit`. Disables the remembered settings. |
| `NNH_SIM_SLIDER=<v>` | The 3D slider for this run: `0` to `1`, at most two decimals (spec 026). Disables the remembered settings. |
| `NNH_SIM_EYE=<view>` | The top screen's view of stereo frames: `left`, `right`, `anaglyph`. Disables the remembered settings. |
| `NNH_SIM_LANGUAGE=<tag>` | The system language for this run (`ja`, `pt-BR`…; spec 029), what `system::language()` returns. Without it, `language=<tag>` in `language.ini` beside `window.ini` (only when the simulator remembers its settings), else `en`. |
| `NNH_SIM_RENDERER=sdl` | Use SDL's default renderer instead of the GPU-device one (see [sdl3.md](../tech/sdl3.md#renderer-2d-now-3d-later)). |
| `NNH_SIM_INPUT=<script>` | Debug/Development: replaces keyboard, mouse and gamepad on every frame with fixed input, e.g. `circle=0.7,0.5;cstick=-1,0;touch=160,120;buttons=A+R+Start`. Every part is optional; sticks in [-1, 1] (y up, before the dead zone), touch in bottom-screen pixels, buttons by `nnh::Button` name joined with `+`. |
| `NNH_REMOTE=127.0.0.1:<port>` | Debug/Development, set by `nnh run --pc` (spec 013): the simulator connects back to that `nnh` (1 s timeout; loopback addresses only, anything else is ignored), sends its log there instead of the terminal, and takes remote input and screenshot requests like a console ([nnh tool](../guides/nnh-tool.md#driving-the-game-from-the-pc)). Without a connection it runs as before. |

You don't set `NNH_REMOTE` yourself: `nnh run <game> --pc` does. Then the simulator's log appears in `nnh`'s
terminal (coloured by `nnh`) and is saved in `build/logs/<game>-<date>-<time>.txt`, like a console's, and test
scripts or typed commands drive it exactly as they drive a console: same frames, same screenshot sizes
(400×240 and 320×240, read back from each screen's render target). At the end of a script `nnh` closes the
simulator. Headless runs (CI, agents) add `SDL_VIDEO_DRIVER=offscreen`. Guide:
[Testing on the console](../guides/testing-on-the-console.md).

```sh
NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=shot.png build/pc-debug/examples/hello/hello.exe
NNH_SIM_LAYOUT=side NNH_SIM_SCALE=2 NNH_EXIT_AFTER_FRAMES=60 NNH_SCREENSHOT=side.png \
  build/pc-debug/examples/sprites/sprites.exe
```

## Pause, step and the held start (spec 030)

Pause and F10 now ask core to pause or step one frame (D52), the same machinery as SELECT + Y / SELECT + X on the
console: while paused the last frame stays on screen under a live overlay, audio holds, and `nnh` still talks to the game.
`NNH_HOLD=1` (set by `nnh run --pc --tunes/--hold`) starts the game paused before frame 1 until `nnh` resumes it (10 s
without a PC, then it runs).
