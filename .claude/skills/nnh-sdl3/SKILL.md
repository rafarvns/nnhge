---
name: nnh-sdl3
description: The NNHGE PC simulator backend built on SDL 3.4 — window with both 3DS screens, rendering, keyboard/gamepad input, 60 Hz pacing, screenshots and headless runs. Use when changing engine/platforms/pc/ or verifying visual output on PC.
---

# SDL3 simulator backend

Full notes: `docs/tech/sdl3.md`. Check SDL APIs in the fetched headers
(`build/pc-debug/_deps/nnh_sdl3_prebuilt-src/x86_64-w64-mingw32/include/SDL3/`) — SDL3 renamed many SDL2 APIs.

## Layout

`engine/platforms/pc/` (target `nnh::platform_pc`, see its README): `src/context_pc.hpp`
(window/renderer, frame counter, dev options, `screen_rect`, `set_draw_color`), `src/app_pc.cpp` (init,
window, events, env vars), `src/video_pc.cpp` (frame, pacing, screenshot), `src/input_pc.cpp` (keyboard
map), `src/debug_text_pc.cpp`. One file per contract area: `<area>_pc.cpp`. Namespace `nnh::platform`
(`::pc` internals). Contract tests: `tests/platform_conformance/` (`nnh_conformance_pc`, headless).

## Rules

- Logical area 400×480: top screen `{0,0,400,240}`, bottom `{40,240,320,240}`; integer-scale presentation.
  Draw in logical coordinates, offset by the screen rect, clip to it.
- 60 Hz like the console (vsync + `SDL_DelayPrecise` pacing).
- The game owns `main()`: `SDL_MAIN_HANDLED` + `SDL_SetMainReady()`.
- SDL stays inside `engine/platforms/pc/` and is linked `PRIVATE`.
- Never more capable than the 3DS in ways games could depend on; enforce 3DS limits as features arrive.
- Keyboard map changes must update `docs/api/input.md`.

## Verify visually

```sh
cmake --build --preset pc-debug
NNH_EXIT_AFTER_FRAMES=30 NNH_SCREENSHOT=build/shot.png build/pc-debug/examples/<ex>/<ex>.exe
```

Then read `build/shot.png`. Headless (CI/no display): add `SDL_VIDEO_DRIVER=offscreen`.

## Checklist

- [ ] Screenshot checked for every visual change
- [ ] Same behaviour as the 3DS backend for the same API calls
- [ ] No SDL types leak outside the backend
