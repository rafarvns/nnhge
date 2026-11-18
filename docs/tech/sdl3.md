# SDL3 (PC simulator backend)

The PC platform library (`engine/platforms/pc/`, target `nnh::platform_pc`) uses SDL 3.4 for the window, rendering, input and
timing. Games never see SDL: it is linked `PRIVATE` and its headers never appear under `engine/core/include/`.

## Files

| File | Responsibility |
|------|----------------|
| `context_pc.hpp` | Shared state (window, renderer), screen rectangles, helpers |
| `app_pc.cpp` | Init/shutdown, window, renderer (GPU device or fallback), event polling, dev env vars |
| `texture_pc.cpp` | Textures: embedded PNG → `SDL_LoadPNG_IO` → `SDL_Texture` (nearest filtering) |
| `draw2d_pc.cpp` | One 2D pass: sprites (`SDL_RenderTextureRotated` + color/alpha mod) and shapes, clipped per screen |
| `video_pc.cpp` | Frame begin/end, 60 Hz pacing, screenshots |
| `input_pc.cpp` | Keyboard → buttons and sticks; combines keyboard, gamepad and mouse into one sample |
| `gamepad_pc.cpp` | First connected gamepad (hot-plug), buttons by position, sticks, triggers |
| `touch_pc.cpp` | Left mouse button on the bottom screen → touch, window → 3DS pixels |
| `sim_layout.cpp`, `sim_settings.cpp` | Window layouts and remembered settings (spec 006): pure, SDL-free, unit tested in `tests/simulator/` |
| `window_pc.cpp` | The window view: current layout, composition of the two screen targets (spec 006) |
| `input_override_pc.cpp` | `NNH_SIM_INPUT` scripted input (Debug/Development) |
| `debug_text_pc.cpp` | SDL's 8×8 debug font, for the crash screen only (it must work when the engine is broken); game and debug text use the engine's built-in font |
| `texture_pc.cpp` (also) | `texture_create_rgba`: `SDL_CreateTexture` + `SDL_UpdateTexture` (the built-in font) |

## How the 3DS is simulated

- Each 3DS screen is a **render target at native size** (400×240, 320×240): drawing on a screen =
  `pc::draw_on(screen)` and native screen pixels, clipped by the target itself. Sprites, rotation and
  blending are rasterized at the console's resolution, like on the 3DS.
- At the end of the frame `compose_screens()` (`window_pc.cpp`) copies both targets into the window at an
  integer scale (nearest filtering) where the layout says (`sim_layout.cpp`, spec 006): stacked like the
  console by default, at `AppConfig::pc_window_scale`. No SDL logical presentation: the window works in
  render-output pixels; the mouse goes through `SDL_RenderCoordinatesFromWindow` and the layout.
  (Before spec 006 the simulator drew into one 400×480 logical canvas scaled by SDL, which rasterized
  rotated sprites at the window's resolution: those screenshots changed slightly.)
- **60 Hz** like the console: vsync on, plus `SDL_DelayPrecise` pacing for faster monitors; if far behind
  (debugger, window drag) it resyncs instead of catching up.
- The game owns `main()`: the backend defines `SDL_MAIN_HANDLED`, includes `<SDL3/SDL_main.h>` and calls
  `SDL_SetMainReady()` before `SDL_Init`.

## Renderer (2D now, 3D later)

The renderer is created with `SDL_CreateGPURenderer(nullptr, window)` (SDL 3.4): SDL's 2D renderer on
an `SDL_GPUDevice` (D3D12/Vulkan/Metal), the device the future 3D renderer will share
([rendering.md](../architecture/rendering.md), D20). If no GPU device is available (headless runs, VMs,
old drivers) it falls back to `SDL_CreateRenderer`. Both give byte-identical screenshots of the examples
on the maintainer's PC (checked with `NNH_SIM_RENDERER=sdl`).

Headless runs (`SDL_VIDEO_DRIVER=offscreen`) use whatever renderer the offscreen driver offers: on Linux
the screenshots matched the windowed ones exactly; on Windows SDL's software renderer differs by ±1 in
semi-transparent pixels and samples the edges of **rotated** sprites differently. Compare headless
screenshots only with headless screenshots from the same system.

Sprites: source rect = region, destination = position − pivot with the scaled size, rotation in degrees
around the pivot, flips inside the destination rectangle, tint/opacity through the texture's color and
alpha mod. Textures use `SDL_SCALEMODE_NEAREST` and alpha blending; SDL batches consecutive draws.

## Developer features

| Variable | Effect |
|----------|--------|
| `NNH_EXIT_AFTER_FRAMES=<n>` | quit after *n* frames |
| `NNH_SCREENSHOT=<file.png>` | with the above, save the last frame (`SDL_RenderReadPixels` + `SDL_SavePNG`) |
| `NNH_SIM_INPUT=<script>` | fixed input on every frame, for screenshots ([simulator.md](../api/simulator.md)) |
| `NNH_SIM_RENDERER=sdl` | skip the GPU-device renderer and use SDL's default one (to compare) |
| `SDL_VIDEO_DRIVER=offscreen` | run without a display (CI smoke test) |

Use the screenshot to verify visual changes without a human: build, run with both variables, read the PNG.

## Rules

- Any SDL call stays inside `engine/platforms/pc/`.
- The simulator must not be *more capable* than the 3DS in ways games could rely on: enforce 3DS
  limits (texture sizes, memory budget) as those features arrive.
- New input devices map onto the same `nnh::` input API.
- Gamepads: `SDL_InitSubSystem(SDL_INIT_GAMEPAD)` after the video init (its failure is ignored: keyboard
  and mouse still work). Pads already connected arrive as `SDL_EVENT_GAMEPAD_ADDED` in the first poll;
  every polled event (also in the pause loop) goes through `pc::gamepad_event`. Face buttons use SDL's
  positional names (`SOUTH`/`EAST`/...), so the mapping is the same for every brand.
- Mouse: `SDL_GetMouseState` + `SDL_RenderCoordinatesFromWindow` give logical coordinates at any window
  scale; SDL's mouse auto-capture keeps reporting a drag outside the window.

## Further reading

- [SDL3 wiki](https://wiki.libsdl.org/SDL3/), [SDL_RenderDebugText](https://wiki.libsdl.org/SDL3/SDL_RenderDebugText),
  [logical presentation](https://wiki.libsdl.org/SDL3/SDL_SetRenderLogicalPresentation)
- [README-linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux)
