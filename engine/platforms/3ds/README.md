# nnh::platform_3ds — Nintendo 3DS

Implements the platform contract (`engine/core/contract/nnh/platform/`) on the Nintendo 3DS with
devkitPro: libctru, citro3d and citro2d.

| | |
|-|-|
| Target | `nnh::platform_3ds` (selected by the `3ds-*` presets; build through `scripts/dkp.sh` on Windows) |
| Requirements | devkitPro `3ds-dev` (devkitARM, libctru, citro2d, citro3d, 3dstools) |
| Files | `src/app_3ds.cpp` · `src/video_3ds.cpp` · `src/input_3ds.cpp` · `src/log_3ds.cpp` · `src/memory_3ds.cpp` · `src/time_3ds.cpp` · `src/texture_3ds.cpp` · `src/draw2d_3ds.cpp` · `../shared/stdio_fs.cpp` · `src/context_3ds.hpp` |
| Capabilities | SecondScreen, TouchScreen, CirclePad; Stereo3D (not on 2DS); CStick, ZButtons (New models) — touch and sticks have engine APIs (spec 003); stereo 3D comes later |
| Extensions | `<nnh/3ds/console.hpp>`: `is_new_3ds()`, `slider_3d()`; `<nnh/3ds/network.hpp>`: `network_start()`/`network_stop()` (`src/network_3ds.cpp`, shared with the log redirect); `<nnh/3ds/launch.hpp>`: `can_launch()`, `launch_after_exit()` and `--nnh-return=` at shutdown (`src/launch_3ds.cpp`, Luma3DS `hb:ldr`; pure argument rules in `src/hbldr_args.cpp`, tested on PC) — [docs/api/3ds-extensions.md](../../../docs/api/3ds-extensions.md) |
| Console remote | Debug/Development only (spec 013, verified on an Old 3DS XL): `src/remote_3ds.cpp` (non-blocking reads and flushed sends on the 3dslink log socket, `ctr::link_socket()`), `src/remote_capture_3ds.cpp` (render-target read-back; 288 000 B linear while screenshots are used; a screenshot pair takes 0.6–0.8 s over Wi-Fi) — [devkitpro-3ds.md](../../../docs/tech/devkitpro-3ds.md), [guide](../../../docs/guides/testing-on-the-console.md) |
| Limits | Old 3DS: ~64 MB for apps (32 MB linear), 6 MB VRAM, textures power of two 8–1024 |
| Output | `.3dsx` + SMDH; `.cia` (spec 009) |
| Data folder | `sdmc:/3ds/<data_folder>/` (log.txt, crash.txt, saves) |

Details: [docs/tech/devkitpro-3ds.md](../../../docs/tech/devkitpro-3ds.md). Run on a console with
`nnh run <game>` or `3dslink` (Homebrew Launcher → Y).
