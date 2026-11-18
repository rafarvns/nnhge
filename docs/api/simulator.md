# PC simulator

The PC build of a game is a native desktop app (SDL3) that behaves like the 3DS:

- One window, the top screen (400×240) above the bottom screen (320×240, centered), scaled by an integer
  factor (`AppConfig::pc_window_scale`, default 2). The window is resizable.
- Runs at 60 Hz like the console, even on faster monitors.
- Controls: keyboard, mouse (touch screen) and a gamepad — see [input.md](input.md#pc-simulator-controls).

It is not an emulator: use [Azahar](https://azahar-emu.org/) or a real console to check console behaviour.
Enforcement of 3DS memory and texture limits arrives with the features that need them.

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
| `NNH_SIM_INPUT=<script>` | Debug/Development: replaces keyboard, mouse and gamepad on every frame with fixed input, e.g. `circle=0.7,0.5;cstick=-1,0;touch=160,120;buttons=A+R+Start`. Every part is optional; sticks in [-1, 1] (y up, before the dead zone), touch in bottom-screen pixels, buttons by `nnh::Button` name joined with `+`. |

```sh
NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=shot.png build/pc-debug/examples/hello/hello.exe
```
