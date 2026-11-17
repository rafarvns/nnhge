# SDL3 (PC simulator backend)

The PC platform library (`engine/platforms/pc/`, target `nnh::platform_pc`) uses SDL 3.4 for the window, rendering, input and
timing. Games never see SDL: it is linked `PRIVATE` and its headers never appear under `engine/core/include/`.

## Files

| File | Responsibility |
|------|----------------|
| `context_pc.hpp` | Shared state (window, renderer), screen rectangles, helpers |
| `app_pc.cpp` | Init/shutdown, window, event polling, dev env vars |
| `video_pc.cpp` | Frame begin/end, 60 Hz pacing, screenshots |
| `input_pc.cpp` | Keyboard → `nnh::Button` mapping |
| `debug_text_pc.cpp` | Debug text with SDL's built-in 8×8 font |

## How the 3DS is simulated

- One window with a **400×480 logical area**: top screen at (0,0) 400×240, bottom screen at (40,240)
  320×240. `SDL_SetRenderLogicalPresentation(..., SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)` keeps pixels
  sharp at any window size; drawing code always uses logical coordinates.
- Drawing on a screen = offset by its rectangle + clip rect to it, so nothing leaks onto the other screen.
- **60 Hz** like the console: vsync on, plus `SDL_DelayPrecise` pacing for faster monitors; if far behind
  (debugger, window drag) it resyncs instead of catching up.
- The game owns `main()`: the backend defines `SDL_MAIN_HANDLED`, includes `<SDL3/SDL_main.h>` and calls
  `SDL_SetMainReady()` before `SDL_Init`.

## Developer features

| Variable | Effect |
|----------|--------|
| `NNH_EXIT_AFTER_FRAMES=<n>` | quit after *n* frames |
| `NNH_SCREENSHOT=<file.png>` | with the above, save the last frame (`SDL_RenderReadPixels` + `SDL_SavePNG`) |
| `SDL_VIDEO_DRIVER=offscreen` | run without a display (CI smoke test) |

Use the screenshot to verify visual changes without a human: build, run with both variables, read the PNG.

## Rules

- Any SDL call stays inside `engine/platforms/pc/`.
- The simulator must not be *more capable* than the 3DS in ways games could rely on: enforce 3DS
  limits (texture sizes, memory budget) as those features arrive.
- New input devices (gamepad, mouse as touch) map onto the same `nnh::` input API.

## Further reading

- [SDL3 wiki](https://wiki.libsdl.org/SDL3/), [SDL_RenderDebugText](https://wiki.libsdl.org/SDL3/SDL_RenderDebugText),
  [logical presentation](https://wiki.libsdl.org/SDL3/SDL_SetRenderLogicalPresentation)
- [README-linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux)
