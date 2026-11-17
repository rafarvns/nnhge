# nnh::platform_pc — PC simulator

Implements the platform contract (`engine/core/contract/nnh/platform/`) on Windows, Linux and macOS with
SDL 3.4, imitating a Nintendo 3DS: one window with both screens, 60 Hz, keyboard as buttons.

| | |
|-|-|
| Target | `nnh::platform_pc` (selected by the `pc-*` presets) |
| Requirements | C++20 host compiler; SDL3 is found or downloaded by `cmake/nnh_sdl3.cmake` |
| Files | `src/app_pc.cpp` · `src/video_pc.cpp` · `src/input_pc.cpp` · `src/log_pc.cpp` · `src/memory_pc.cpp` · `src/time_pc.cpp` · `src/debug_text_pc.cpp` · `../shared/stdio_fs.cpp` · `src/context_pc.hpp` |
| Capabilities | SecondScreen (simulated); touch, Circle Pad, stereo 3D: not yet |
| Developer keys | F1 overlay, Pause, F10 step, F11 slow motion, F12 screenshot (Debug/Development) |
| Developer options | `NNH_EXIT_AFTER_FRAMES`, `NNH_SCREENSHOT`, `SDL_VIDEO_DRIVER=offscreen` |
| Data folder | Debug/Development: `<exe dir>/data/<data_folder>/`; Release: `SDL_GetPrefPath("NNHGE", <data_folder>)` |

Details: [docs/tech/sdl3.md](../../../docs/tech/sdl3.md), keyboard map in
[docs/api/input.md](../../../docs/api/input.md). Contract tests: `nnh_conformance_pc` (runs headless).
