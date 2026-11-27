# nnh::platform_pc — PC simulator

Implements the platform contract (`engine/core/contract/nnh/platform/`) on Windows, Linux and macOS with
SDL 3.4, imitating a Nintendo 3DS: one window with both screens, 60 Hz, keyboard as buttons.

| | |
|-|-|
| Target | `nnh::platform_pc` (selected by the `pc-*` presets) |
| Requirements | C++20 host compiler; SDL3 is found or downloaded by `cmake/nnh_sdl3.cmake` |
| Files | `src/app_pc.cpp` · `src/video_pc.cpp` · `src/input_pc.cpp` · `src/log_pc.cpp` · `src/memory_pc.cpp` · `src/time_pc.cpp` · `src/texture_pc.cpp` · `src/draw2d_pc.cpp` · `src/debug_text_pc.cpp` (crash screen) · `src/remote_pc.cpp`, `src/remote_capture_pc.cpp` (console remote, spec 013) · `../shared/stdio_fs.cpp` · `src/context_pc.hpp`; full list in [sdl3.md](../../../docs/tech/sdl3.md) |
| Capabilities | SecondScreen, TouchScreen, CirclePad, CStick, ZButtons (simulated); stereo 3D: not yet |
| Console remote | Debug/Development only (spec 013): `NNH_REMOTE` (set by `nnh run --pc`, loopback addresses only) connects back to `nnh`; the log then goes to `nnh` instead of the terminal, and scripts drive the simulator like a console (`src/remote_pc.cpp`, `src/remote_address_pc.cpp`; screens read back with `SDL_RenderReadPixels` in `src/remote_capture_pc.cpp`) — [simulator.md](../../../docs/api/simulator.md) |
| Developer keys | F1 overlay, Pause, F10 step, F11 slow motion, F12 screenshot (Debug/Development) |
| Developer options | `NNH_EXIT_AFTER_FRAMES`, `NNH_SCREENSHOT`, `NNH_SIM_*`, `SDL_VIDEO_DRIVER=offscreen`; `NNH_REMOTE` (set by `nnh run --pc`) |
| Links | `ws2_32` on Windows (the console remote's loopback socket) |
| Data folder | Debug/Development: `<exe dir>/nnh_user/<data_folder>/` (never `<exe dir>/data/`: on Linux that may be the `data` program itself); Release: `SDL_GetPrefPath("NNHGE", <data_folder>)` |

Details: [docs/tech/sdl3.md](../../../docs/tech/sdl3.md), keyboard map in
[docs/api/input.md](../../../docs/api/input.md). Contract tests: `nnh_conformance_pc` (runs headless).
