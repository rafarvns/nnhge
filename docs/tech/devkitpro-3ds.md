# devkitPro and the Nintendo 3DS

The 3DS backend (`engine/platforms/3ds/`) is built with devkitPro's open toolchain. Headers are in
`C:\devkitPro\libctru\include` (Linux/macOS: `/opt/devkitpro/libctru/include`); read them when in doubt.

## Pieces

| Library / tool | Role | Header |
|----------------|------|--------|
| devkitARM | GCC cross compiler for the ARM11 (`armv6k`, hard float) | — |
| libctru | System services: GPU memory, HID (input), APT (app lifecycle), filesystem, romfs, ndsp (audio), sockets | `<3ds.h>` |
| citro3d | GPU (PICA200) API: render targets, textures, shaders, frames | `<citro3d.h>` |
| citro2d | 2D on top of citro3d: sprites, shapes, text, sprite sheets | `<citro2d.h>` |
| tex3ds | Converts PNG → GPU texture/atlas `.t3x` (pre-tiled) | tool |
| picasso | Assembles PICA200 vertex shaders → `.shbin` | tool |
| 3dsxtool / smdhtool | ELF → `.3dsx`; title/icon metadata `.smdh` | tools |
| 3dslink | Sends a `.3dsx` to the Homebrew Launcher netloader | tool |

## App lifecycle (what our backend does)

```cpp
gfxInitDefault();                       // framebuffers
C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);      // GPU command buffer (0x40000)
C2D_Init(C2D_DEFAULT_MAX_OBJECTS);      // 4096 2D objects per frame
C2D_Prepare();
top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
while (aptMainLoop()) {                 // false when the user closes the app from the HOME Menu
    hidScanInput();                     // once per frame, then hidKeysHeld/Down/Up
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(top, color); C2D_SceneBegin(top);
    /* draw */
    C3D_FrameEnd(0);                    // submits and waits for vsync (60 Hz)
}
C2D_Fini(); C3D_Fini(); gfxExit();      // reverse order
```

Always leave through `aptMainLoop()` returning false or the game's own exit; never `exit()` mid-frame.

## Hardware limits (design for Old 3DS)

| Resource | Limit | Consequence |
|----------|-------|-------------|
| CPU | ARM11 MPCore 268 MHz (New 3DS: 804 MHz) | No per-object virtual calls in hot loops; batch work |
| RAM for apps | ~64 MB on Old 3DS in the default mode (more on New 3DS) | Budget assets; stream music |
| Linear heap | 32 MB by default on Old 3DS; rest is the normal heap (`malloc`) | GPU buffers and textures must be in linear memory or VRAM |
| VRAM | 6 MB, two 3 MB banks; allocations don't cross banks | Render targets (including the screens) live here |
| Textures | Width and height each a power of two, 8 to 1024 | Pad or atlas images; `assetc` enforces this |
| Screens | Top 400×240 (stereo 3D), bottom 320×240 (touch) | Framebuffers are rotated in memory; citro2d handles it |

- The CPU **cannot write VRAM** directly; upload with the GPU (citro3d does it).
- Memory split can be changed by defining `__ctru_heap_size` / `__ctru_linear_heap_size`
  (see `<3ds/env.h>`); measure before changing it. `osGetMemRegionFree()` reports free memory.
- `APT_CheckNew3DS(&is_new)` tells the model at runtime. New 3DS extra memory/CPU modes need a `.cia`
  with the right `.rsf` settings.

## Input (HID)

`hidScanInput()` once per frame, then `hidKeysHeld()`, `hidKeysDown()`, `hidKeysUp()` (`KEY_A`, `KEY_DUP`...),
`hidTouchRead(&touch)` for the bottom screen, `hidCircleRead(&circle)` for the Circle Pad. ZL/ZR and the
C-Stick exist only on New 3DS models.

## Files, romfs, audio

- `romfsInit()` mounts the assets packed into the `.3dsx` as `romfs:/`. `sdmc:/` is the SD card.
- Audio uses `ndspInit()` (needs the DSP firmware dumped on the console: `sdmc:/3ds/dspfirm.cdc`);
  buffers must be in linear memory.

## Getting it onto the console

- `.3dsx` + `.smdh`: `ctr_generate_smdh` / `ctr_create_3dsx` in CMake (see [cmake.md](cmake.md)).
- Send: Homebrew Launcher → **Y**, then `3dslink <file>.3dsx -a <ip>` (`-a` takes only the IP).
- **Logs on the PC**: call `link3dsStdio()` (from `<3ds/3dslink.h>`) after the socket service is up, and
  `printf` goes to the `3dslink` terminal (TCP port 17491). Planned in M1.
- `.cia` (installable): needs `makerom` + `bannertool` (not bundled with devkitPro). Planned in M5.
- Launching another `.3dsx` from a homebrew app: libctru only offers `aptSetChainloader()` for installed
  titles; there is no 3dsx-to-3dsx API (relevant for NNH Link, see [decisions](../architecture/decisions.md)).

## Windows setup gotchas

- devkitPro's CMake toolchain **refuses a Windows-native CMake**; it must run from `C:\devkitPro\msys2`.
  Use `scripts/dkp.sh` from Git Bash. Install Ninja there once: `pacman -S --needed ninja`.
- The graphical installer edits the **system PATH** (prepends `c:\devkitPro\msys2\usr\bin`). On one
  machine it left a stray `"` at the end of the system PATH, which broke every user-PATH command in
  `cmd.exe`. If commands vanish from `cmd` after installing, check the end of the system PATH.
- PowerShell/cmd don't understand `/c/...` paths; use `C:\devkitPro\tools\bin\3dslink.exe` there.

## Further reading

- [devkitPro 3ds-examples](https://github.com/devkitPro/3ds-examples), [libctru](https://github.com/devkitPro/libctru), [citro3d](https://github.com/devkitPro/citro3d), [citro2d](https://github.com/devkitPro/citro2d)
- [3dbrew wiki](https://www.3dbrew.org/) — hardware and system reference
- Texture/VRAM rules: [devkitPro forum t=9219](https://devkitpro.org/viewtopic.php?t=9219), citro3d `source/texture.c`
- Memory split in practice: [sh23ds PERFORMANCE.md](https://github.com/ankushKun/sh23ds/blob/main/port/PERFORMANCE.md)
