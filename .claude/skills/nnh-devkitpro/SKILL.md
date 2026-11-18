---
name: nnh-devkitpro
description: Nintendo 3DS development with devkitPro for NNHGE — writing or debugging code in engine/platforms/3ds/ (libctru, citro2d, citro3d, ndsp, romfs), respecting 3DS memory/texture limits, building the .3dsx and running it on a console with 3dslink. Use for any 3DS-specific work or console-only bug.
---

# devkitPro / 3DS

Full notes: `docs/tech/devkitpro-3ds.md`. **Verify APIs in the installed headers**
(`C:/devkitPro/libctru/include/3ds/`, `c2d/`, `c3d/`) before using them — don't rely on memory
(e.g. `envSetNextLoad` does not exist in libctru 2.7).

## Build and run

```sh
scripts/dkp.sh cmake --preset 3ds-release         # once (devkitPro's CMake must run inside its MSYS2)
scripts/dkp.sh cmake --build --preset 3ds-release # → build/3ds-release/examples/<ex>/<ex>.3dsx
/c/devkitPro/tools/bin/3dslink.exe build/3ds-release/examples/<ex>/<ex>.3dsx -a <console-ip>
```

Console must be in the Homebrew Launcher netloader (press **Y**). `-a` takes only the IP. "N% sent" is
the compressed size, not an error. Ask the user for the console IP if unknown (shown on the netloader screen).

## Platform library layout

`engine/platforms/3ds/` (target `nnh::platform_3ds`, see its README): `src/context_3ds.hpp` (shared
state: render targets, `draw_on`), `src/app_3ds.cpp` (init/shutdown, `aptMainLoop`),
`src/video_3ds.cpp` (frame begin/end), `src/input_3ds.cpp` (HID → `nnh::Button`),
`src/texture_3ds.cpp` (`.t3x` and RGBA textures), `src/draw2d_3ds.cpp` (citro2d passes). It implements the contract headers `engine/core/contract/nnh/platform/*.hpp`.
Namespace `nnh::platform` (`nnh::platform::ctr` for internals).
One file per module: `<module>_3ds.cpp`.

## Rules

- Init order: `gfxInitDefault` → `C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)` → `C2D_Init` → `C2D_Prepare` →
  targets; shut down in reverse. Every frame between `C3D_FrameBegin` / `C3D_FrameEnd`.
- `hidScanInput()` exactly once per frame (the engine does it in `read_input`).
- Memory: Old 3DS ~64 MB for apps, 32 MB linear heap by default; GPU textures/buffers in linear or VRAM
  (6 MB, two 3 MB banks); the CPU can't write VRAM. Check `osGetMemRegionFree()` when budgeting.
- Textures: power-of-two sides 8–1024, tiled (tex3ds produces `.t3x` already tiled).
- citro2d: max objects per frame set at `C2D_Init` (sized from `AppConfig::gfx.max_draws` + the debug pass). No citro2d text: all text is engine sprites of the built-in font.
- ZL/ZR/C-Stick only on New 3DS (`APT_CheckNew3DS`). Audio (`ndspInit`) needs `sdmc:/3ds/dspfirm.cdc`.
- Logs to PC: automatic in Debug/Development when launched by 3dslink (`app_3ds.cpp` does `socInit` +
  `link3dsStdio()`); run `3dslink -s <file> -a <ip>` to see them. Platform code never logs itself: it
  returns errors and sets `last_error()`, core logs.
- No platform headers outside `engine/platforms/3ds/`.

## Debugging console-only bugs

Full guide: `docs/guides/debugging-on-3ds.md` (3dslink -s logs, SELECT+L overlay, crash.txt, Luma3DS
UNITINFO for CPU faults, `arm-none-eabi-addr2line`, GDB via Rosalina).

1. Reproduce in the simulator first; if it doesn't reproduce, suspect memory region, texture size,
   alignment, uninitialized VRAM, or float/precision differences.
2. Add temporary `printf` + `link3dsStdio()` or on-screen debug text; remove afterwards.
3. Check limits: objects per frame, text buffer glyphs, linear heap free.
4. Try Azahar (emulator) for faster iteration, but confirm on hardware.

## Checklist

- [ ] APIs checked against installed headers
- [ ] Builds with `scripts/dkp.sh`, no new warnings
- [ ] Ran on a 3DS (or asked the user to) for visible changes
- [ ] Simulator behaviour matches (same limits)
