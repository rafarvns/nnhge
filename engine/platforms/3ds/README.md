# nnh::platform_3ds — Nintendo 3DS

Implements the platform contract (`engine/core/contract/nnh/platform/`) on the Nintendo 3DS with
devkitPro: libctru, citro3d and citro2d.

| | |
|-|-|
| Target | `nnh::platform_3ds` (selected by the `3ds-*` presets; build through `scripts/dkp.sh` on Windows) |
| Requirements | devkitPro `3ds-dev` (devkitARM, libctru, citro2d, citro3d, 3dstools) |
| Files | `src/app_3ds.cpp` · `src/video_3ds.cpp` · `src/input_3ds.cpp` · `src/log_3ds.cpp` · `src/time_3ds.cpp` · `src/debug_text_3ds.cpp` · `src/context_3ds.hpp` |
| Capabilities | two screens, buttons (ZL/ZR on New 3DS); touch, Circle Pad, stereo 3D: not yet |
| Limits | Old 3DS: ~64 MB for apps (32 MB linear), 6 MB VRAM, textures power of two 8–1024 |
| Output | `.3dsx` + SMDH; `.cia` planned (M5) |

Details: [docs/tech/devkitpro-3ds.md](../../../docs/tech/devkitpro-3ds.md). Run on a console with
`3dslink` (Homebrew Launcher → Y).
