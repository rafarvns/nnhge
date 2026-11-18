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
C2D_Init(max_draws + 4096 + 256);       // 2D objects per frame: draws + debug glyphs + overlay (≥ 4096)
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
| Textures | Width and height each a power of two, 8 to 1024 | Pad or atlas images; the build checks it (`IMAGES` and `ASSETS`) |
| Screens | Top 400×240 (stereo 3D), bottom 320×240 (touch) | Framebuffers are rotated in memory; citro2d handles it |

- The CPU **cannot write VRAM** directly; upload with the GPU (citro3d does it).
- Memory split can be changed by defining `__ctru_heap_size` / `__ctru_linear_heap_size`
  (see `<3ds/env.h>`); measure before changing it. `osGetMemRegionFree()` reports free memory.
- `APT_CheckNew3DS(&is_new)` tells the model at runtime. New 3DS extra memory/CPU modes need a `.cia`
  with the right `.rsf` settings.

## 2D drawing (what our backend does)

Spec 004, [rendering.md](../architecture/rendering.md). Files: `texture_3ds.cpp`, `draw2d_3ds.cpp`.

- **Textures**: the build converts each PNG with `tex3ds -f rgba8888 -z auto` into a `.t3x` embedded
  in the executable; `Tex3DS_TextureImport(data, size, &tex, nullptr, false)` decompresses it into a
  `C3D_Tex` in **linear** memory (`false` = not VRAM). Nearest filter, clamp to edge. The handle owns
  the `C3D_Tex` and the `Tex3DS_Texture` (freed with `C3D_TexDelete` + `Tex3DS_TextureFree`). Core
  accounts `tex.size` bytes under the `textures` tag.
- **Regions**: one `Tex3DS_SubTexture` per draw. citro3d textures are stored bottom-up, so
  `top = 1 - y/H` and `bottom = 1 - (y+h)/H` (top > bottom; top < bottom would mean "rotated" to tex3ds).
- **Sprites**: `C2D_DrawImage` with `C2D_DrawParams`: `pos.x/y` is where `center` (the pivot) lands,
  `pos.w/h` the size — negative to flip inside the rectangle, like SDL — and `angle` in radians.
  Tint/opacity: `C2D_SetTintMode(C2D_TintMult)` and `C2D_PlainImageTint(color, 1.0)`; white opaque
  draws pass no tint. The mode is set back to `C2D_TintSolid` after the pass (debug text).
- **Shapes**: `C2D_DrawRectSolid`; outlines as four 1-pixel rectangles inside the rectangle; lines with
  `C2D_DrawLine` through pixel centers, 1 pixel thick.
- **Capacity**: citro2d's object buffer is sized at `C2D_Init` from `AppConfig::gfx.max_draws` plus the
  debug pass (core's 4096 debug glyphs and overlay rectangles); an outline costs 4 objects.
- **Textures from pixels** (`texture_create_rgba`, used for the built-in font): `C3D_TexInit(w, h,
  GPU_RGBA8)` in linear memory, then the CPU writes the GPU layout itself: 8×8 tiles left to right, rows
  of tiles from the **top** of the image, Morton (Z) order inside a tile with rows top-down too, each
  pixel a `u32` `0xRRGGBBAA`; then `C3D_TexFlush`. **No vertical flip** — measured on an Old 3DS XL
  with four variants of the font (flipping tiles and/or rows gave scrambled or upside-down glyphs). The
  font is a good test pattern: each glyph is exactly one tile. No citro2d text (`C2D_TextBuf`): all text
  is engine sprites.

## Input (HID)

`hidScanInput()` once per frame, then `hidKeysHeld()`, `hidKeysDown()`, `hidKeysUp()` (`KEY_A`, `KEY_DUP`...),
`hidTouchRead(&touch)` for the bottom screen, `hidCircleRead(&circle)` for the Circle Pad. ZL/ZR and the
C-Stick exist only on New 3DS models.

What our backend (`input_3ds.cpp`) does:

- Buttons from the D-Pad keys only (`KEY_DUP`...); `KEY_UP` would also include the Circle Pad, which the
  engine keeps separate (the game can opt into a mirror, see [api/input.md](../api/input.md)).
- Touch: `KEY_TOUCH` in `hidKeysHeld()` says whether the screen is touched; `hidTouchRead` gives the point
  (`px` 0–319, `py` 0–239).
- Circle Pad: `circlePosition.dx/dy`, y up, divided by 156. Measured on an Old 3DS XL: 159–167 at full
  push depending on the direction, so 156 reaches full deflection everywhere; core clamps the rest.
- C-Stick: needs the **IRRST** service. `init()` calls `irrstInit()` only when `APT_CheckNew3DS` says New
  3DS (`irrstExit()` at shutdown); each frame `irrstScanInput()` then `hidCstickRead`. If it fails to
  start, the C-Stick reads 0 and the game still runs. Its range (156) is not measured yet.

## Files, romfs, audio

- `romfsInit()` mounts the assets packed into the `.3dsx` as `romfs:/`. `sdmc:/` is the SD card.
  Our backend (`assets_3ds.cpp`, spec 007): `assets_mount()` = `romfsInit()` (a `.3dsx` without romfs
  fails it: the game has no assets), reads with `fopen("romfs:/<path>")`, `romfsExit()` at shutdown. The
  build packs `build/<preset>/<game dir>/assets/` with `ctr_create_3dsx(... ROMFS <game>_assets)`; romfs
  paths are case-sensitive, which core enforces on every system. Files are read from the SD card on
  demand, not loaded with the program.
- Audio uses `ndspInit()`, which needs the DSP firmware dumped once on the console
  (`sdmc:/3ds/dspfirm.cdc`, e.g. with the DSP1 homebrew); without it `ndspInit()` fails. Our backend
  (`audio_3ds.cpp`, spec 008, decision D21): the DSP mixes 24 channels in hardware; voices use channels
  0–21 (one `ndspWaveBuf` each, `looping` for loops, status QUEUED/PLAYING = playing), music streams use
  22 and 23 with 4 queued `ndspWaveBuf`s of 4096 frames. `nsamples` counts **frames** (per channel) for
  stereo PCM16. Mix: `mix[0]`/`mix[1]` = front left/right gain; rate per channel (`ndspChnSetRate`),
  linear interpolation. Samples and stream buffers are `linearAlloc`ed, and `DSP_FlushDataCache` runs
  before the DSP reads new data. A missing firmware makes core run silently (`audio::available()` false).
  Linking audio adds ~8 KB to every `.3dsx` (`ndspInit` runs in `nnh::init()`); the Ogg decoder only
  comes with music or Ogg sounds (~60 KB).

## Getting it onto the console

- `.3dsx` + `.smdh`: `ctr_generate_smdh` / `ctr_create_3dsx` in CMake (see [cmake.md](cmake.md)).
- Send: Homebrew Launcher → **Y**, then `3dslink <file>.3dsx -a <ip>` (`-a` takes only the IP).
- **Logs on the PC** (implemented): in Debug/Development builds started by 3dslink (`__3dslink_host` set),
  the platform library calls `socInit` (1 MB buffer), connects to the PC on port 17491
  (`LINK3DS_COMM_PORT`) and `dup2`s the socket onto stdout/stderr, so every log line reaches the PC.
  Launch with `3dslink -s ...` so the PC keeps listening. We don't use libctru's `link3dsStdio()`: its
  `connect` blocks with no timeout, and when the PC isn't listening and its firewall drops the attempt
  the console hangs on a black screen. Our connect is non-blocking with a 1 s limit (`poll`; connected =
  `POLLOUT` without `POLLERR`/`POLLHUP` — `getsockopt(SO_ERROR)` reported a failure for a good
  connection on hardware); without a listening PC the game starts without logs. At shutdown, flush and `close()` fds 1 and 2 as well as the
  socket **before** `socExit()`, or newlib's exit-time stdio cleanup hangs the console on a black screen
  instead of returning to the Homebrew Launcher.
- `.cia` (installable, spec 009): `nnh_add_game(... CIA UNIQUE_ID ...)` and the target `<game>_cia`; makerom (`-f cia -target t -exefslogo`, our RSF `cmake/3ds/nnhge.rsf`, the game's SMDH, a bannertool banner, the assets folder as romfs) and bannertool are fetched pinned on the first `.cia` build. Install with FBI over the network: `scripts/fbi_install.py` ([packaging guide](../guides/packaging.md)). romfs, saves on the SD card and exit to the HOME Menu work as in the `.3dsx`.
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
