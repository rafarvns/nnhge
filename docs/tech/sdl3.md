# SDL3 (PC simulator backend)

The PC platform library (`engine/platforms/pc/`, target `nnh::platform_pc`) uses SDL 3.4 for the window, rendering, input and
timing. Games never see SDL: it is linked `PRIVATE` and its headers never appear under `engine/core/include/`.

## Files

| File | Responsibility |
|------|----------------|
| `context_pc.hpp` | Shared state (window, renderer), screen rectangles, helpers |
| `app_pc.cpp` | Init/shutdown, window, renderer (GPU device or fallback), event polling, dev env vars |
| `assets_pc.cpp` | Assets (spec 007): `<exe dir>assets/<path>` through `SDL_GetPathInfo`/`SDL_IOFromFile` |
| `audio_pc.cpp` | Audio (spec 008): one `SDL_OpenAudioDeviceStream` (48 kHz stereo float) whose callback runs the mixer; every call from core takes `SDL_LockAudioStream` (SDL holds that lock during the callback). No device: `audio_init` fails and core runs silently |
| `audio_mixer_pc.cpp` | The software stand-in for the 3DS DSP: voices and streams resampled (linear) to 48 kHz, gains ramped per block; SDL-free, unit tested in `tests/simulator/` |
| `texture_pc.cpp` | Textures: embedded PNG → `SDL_LoadPNG_IO` → `SDL_Texture` (nearest filtering) |
| `draw2d_pc.cpp` | One 2D pass: sprites (`SDL_RenderTextureRotated` + color/alpha mod) and shapes, clipped per screen |
| `draw_tiles_pc.cpp` | Tile batches (spec 012, D26): one `SDL_RenderTexture` per quad, `SDL_RenderTextureRotated` for flips; one color/alpha mod per batch; zoomed quads at the batch's destination size (spec 018, D33) |
| `video_pc.cpp` | Frame begin/end, 60 Hz pacing, screenshots |
| `input_pc.cpp` | Keyboard → buttons and sticks; combines keyboard, gamepad and mouse into one sample |
| `gamepad_pc.cpp` | First connected gamepad (hot-plug), buttons by position, sticks, triggers |
| `touch_pc.cpp` | Left mouse button on the bottom screen → touch, window → 3DS pixels |
| `sim_layout.cpp`, `sim_settings.cpp` | Window layouts and remembered settings (spec 006): pure, SDL-free, unit tested in `tests/simulator/` |
| `window_pc.cpp` | The window view: current layout, composition of the two screen targets (spec 006) |
| `input_override_pc.cpp` | `NNH_SIM_INPUT` scripted input (Debug/Development) |
| `remote_pc.cpp`, `remote_address_pc.cpp` | Console remote (spec 013, Debug/Development): loopback TCP connection to `nnh run --pc` from `NNH_REMOTE` (Winsock/BSD sockets, not SDL), non-blocking reads, the log through it while connected (`log_pc.cpp`); the address parser is pure, tested in `tests/simulator/` |
| `remote_capture_pc.cpp` | Screenshots for the remote: `SDL_RenderReadPixels` of a screen's render target after `end_frame()` → RGBA8 rows |
| `debug_text_pc.cpp` | SDL's 8×8 debug font, for the crash screen only (it must work when the engine is broken); game and debug text use the engine's built-in font |
| `texture_pc.cpp` (also) | `texture_create_rgba`: `SDL_CreateTexture` + `SDL_UpdateTexture` (the built-in font) |
| `video3d_pc.cpp` | 3D (spec 026, D41): meshes copied to the heap; each pass rasterised on the CPU (`engine/platforms/shared/raster3d/`) into a 400×240 RGBA8 + depth buffer that starts as the clear colour or as the top target read back (`SDL_RenderReadPixels`) when 2D was drawn below the world, then put back with `SDL_UpdateTexture` + `SDL_RenderTexture` (`SDL_BLENDMODE_NONE`, 1:1); `video3d_self_test` reports no tilt to check |
| `texels3d_pc.cpp` | A texture's pixels for the rasteriser: read back once (drawn into a temporary target without blending or colour modulation) and kept as an `SDL_Texture` property with a cleanup callback, so they go with the texture |

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

## Renderer (2D on SDL, 3D on the CPU)

The renderer is created with `SDL_CreateGPURenderer(nullptr, window)` (SDL 3.4): SDL's 2D renderer on
an `SDL_GPUDevice` (D3D12/Vulkan/Metal, D20). If no GPU device is available (headless runs, VMs, old drivers) it
falls back to `SDL_CreateRenderer`.

3D doesn't use the GPU device (decision D41, spec 026): the engine's CPU rasteriser draws the 3D pass into its own
buffers with fixed-point edges and a fixed sequence of float operations (`-O2 -ffp-contract=off` in every flavour), and
the finished picture is copied onto the top target opaque. So 3D works headless and with SDL's default renderer, and
the 3D pixels are the same on every renderer and host compiler. `scripts/check_png_hash.sh <exe> <frames>` runs a
game windowed (GPU device), with `NNH_SIM_RENDERER=sdl` and with `SDL_VIDEO_DRIVER=offscreen` and compares the
screenshots: the two headless ones match (the models example's golden hashes are recorded from them); on the
maintainer's Windows PC the windowed GPU-device screenshot differs from them already for 2D-only examples
(`examples/text`: semi-transparent shadows round differently), so whole-picture goldens use the headless renderers.

Headless runs (`SDL_VIDEO_DRIVER=offscreen`) use whatever renderer the offscreen driver offers: on Linux
the screenshots matched the windowed ones exactly; on Windows SDL's software renderer differs by ±1 in
semi-transparent pixels and samples the edges of **rotated** sprites differently. Compare headless
screenshots only with headless screenshots from the same system.

Sprites: source rect = region, destination = position − pivot with the scaled size, rotation in degrees
around the pivot, flips inside the destination rectangle, tint/opacity through the texture's color and
alpha mod. Textures use `SDL_SCALEMODE_NEAREST` and alpha blending; SDL batches consecutive draws.

Tiles (`draw_tiles`, spec 012): quads arrive at whole pixels with the tile's exact source rectangle, so they
never show seams. Tiled applies the diagonal flip (swap x and y) first, then the x and y mirrors; SDL mirrors a
copy first and then rotates it around its centre. With texture pixel `(u, v)` around the tile's centre and a 90°
clockwise turn `R(u, v) = (−v, u)`, Tiled's result `F·T` equals `R·S` for the SDL mirror `S = R⁻¹·F·T`:

| Tiled flips | `F·T (u, v)` | SDL mirror before the 90° turn |
|-------------|--------------|--------------------------------|
| diagonal | `(v, u)` | vertical |
| diagonal + x | `(−v, u)` | none (a plain clockwise turn) |
| diagonal + y | `(v, −u)` | horizontal + vertical |
| diagonal + x + y | `(−v, −u)` | horizontal |

That is: horizontal = `flip_y`, vertical = `!flip_x` (square tiles only, so the turn stays in place).
`tests/platform_conformance/test_draw_tiles_pc.cpp` draws the 8 orientations of an 8×8 texture with a different
color per pixel and reads every pixel back (headless, software renderer).

**Zoom** (spec 018, D33): a quad covers `{x, y, w, h}` with `w = dst_w (+1 with kTileOrientWiden)` and
`h = dst_h (+1 with kTileOrientTaller)`; the source rectangle stays the tile. The fast path is
`(orient & 7) == 0` (no flip; the size bits don't matter). A diagonal quad with `w ≠ h` passes the **pre-swapped**
rectangle `{x + (w − h) / 2, y + (h − w) / 2, h, w}` to `SDL_RenderTextureRotated`: SDL turns it 90° about its
centre, so its footprint is exactly `{x, y, w, h}` (with `w = h` it is the plain destination, so zoom 1 is
unchanged). The screens are native-size render targets with nearest sampling, so the window scale doesn't interact
with the zoom. The conformance test checks zoom 2 texel by texel and that a widened quad and a widened diagonal quad
cover exactly their 13 × 12 rectangles.

## Developer features

| Variable | Effect |
|----------|--------|
| `NNH_EXIT_AFTER_FRAMES=<n>` | quit after *n* frames |
| `NNH_SCREENSHOT=<file.png>` | with the above, save the last frame (`SDL_RenderReadPixels` + `SDL_SavePNG`) |
| `NNH_SIM_INPUT=<script>` | fixed input on every frame, for screenshots ([simulator.md](../api/simulator.md)) |
| `NNH_SIM_RENDERER=sdl` | skip the GPU-device renderer and use SDL's default one (to compare) |
| `NNH_REMOTE=127.0.0.1:<port>` | set by `nnh run --pc`: connect back to it (log, remote input, screenshots; [simulator.md](../api/simulator.md)) |
| `SDL_VIDEO_DRIVER=offscreen` | run without a display (CI smoke test) |
| `SDL_AUDIO_DRIVER=dummy` | run without sound (scripted runs and screenshots stay quiet; music still advances) |

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
