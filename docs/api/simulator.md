# PC simulator

The PC build of a game is a native desktop app (SDL3) that behaves like the 3DS:

- One window, the top screen (400×240) above the bottom screen (320×240, centered), scaled by an integer
  factor (`AppConfig::pc_window_scale`, default 2). The window is resizable.
- Runs at 60 Hz like the console, even on faster monitors.
- Keyboard mapping: see [input.md](input.md).

It is not an emulator: use [Azahar](https://azahar-emu.org/) or a real console to check console behaviour.
Enforcement of 3DS memory and texture limits arrives with the features that need them.

## Environment variables (for tests and CI)

| Variable | Effect |
|----------|--------|
| `NNH_EXIT_AFTER_FRAMES=<n>` | `nnh::frame()` returns false after *n* frames. |
| `NNH_SCREENSHOT=<file.png>` | With `NNH_EXIT_AFTER_FRAMES`, saves the last frame as PNG. |

```sh
NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=shot.png build/pc-debug/examples/hello/hello.exe
```
