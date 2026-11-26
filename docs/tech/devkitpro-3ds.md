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
C2D_Init(max_draws + max_tiles + 4096 + 256); // 2D objects per frame: draws + tile quads + debug glyphs + overlay
C2D_Prepare();
top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
while (aptMainLoop()) {                 // false when the user closes the app from the HOME Menu
    hidScanInput();                     // once per frame, then hidKeysHeld/Down/Up
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(top, color); C2D_SceneBegin(top);
    /* draw */
    C3D_FrameEnd(0);                    // submits; the GPU draws on, the next FrameBegin waits (vsync, 60 Hz)
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
- **Float compares are expensive in hot loops.** The VFPv2 has no compare-to-register: every `if (a < b)` on floats is
  `vcmp` + `vmrs` + a branch, and `vmrs` stalls the ARM pipeline until the VFP result is ready (adds take 8 cycles).
  A loop over many items should reject them with integer tests where it can (spec 019's box scans compare floats'
  bit patterns as integers and test finiteness on the bits: 88 instructions with 10 float compares became 11–17
  integer instructions per rejected box, see
  [plan 019](../../specs/019-collision-layers-and-queries/plan.md#console-bench-refinement)). Check the disassembly
  (`arm-none-eabi-objdump -dr` of a `3ds-dev` object) for `vmrs`, `memset`/`memcpy` calls, `__aeabi_*` helpers
  (64-bit conversions, divisions: the ARM11 has no integer divide) and `floorf`/`ceilf` calls (VFPv2 can't round).

## 2D drawing (what our backend does)

Spec 004, [rendering.md](../architecture/rendering.md). Files: `texture_3ds.cpp`, `draw2d_3ds.cpp`,
`draw_tiles_3ds.cpp` (spec 012).

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
- **Capacity**: citro2d's object buffer is sized at `C2D_Init` from `AppConfig::gfx.max_draws` plus
  `AppConfig::gfx.max_tiles` (core never sends more tile quads per frame) plus the debug pass (core's 4096
  debug glyphs and overlay rectangles); an outline costs 4 objects. Each object reserves about 192 bytes of
  `Linear` memory (6 vertices).
- **Tiles** (`draw_tiles_3ds.cpp`, spec 012, D26): one `C2D_DrawImage` per quad with angle 0 and no pivot;
  `1/width` and `1/height` of the texture are computed once per batch and one `C2D_ImageTint` is shared
  (none when the batch is white and opaque). Mirrors are a negative `pos.w`/`pos.h`, as for sprites (citro2d
  swaps the corner texcoords). Tiled's **diagonal flip** uses tex3ds' rotated sub-texture rule: when
  `top < bottom`, a corner reads `(u, v) = (top|bottom, left|right)`, so `left = v` of the tile's top row,
  `right = v` of its bottom row, `top = u` of its left column and `bottom = u` of its right column shows the
  tile transposed; the mirrors then apply to that, as Tiled does. **Confirmed on an Old 3DS XL** (spec 012
  T3.9, 2026-11-21): `examples/platformer`'s flip test shows all 8 orientations as the PC screenshot (the PC
  side is verified pixel by pixel), with no seams under a scrolling camera. **Zoom** (spec 018, D33): each quad
  is drawn `dst_w` (+1 with `kTileOrientWiden`) by `dst_h` (+1 with `kTileOrientTaller`) with the same texture
  coordinates (the source is still the `tile_w × tile_h` square; the diagonal flip's transposed texcoords work for a
  non-square destination); `GPU_NEAREST` makes integer zooms exact texel squares. Texel bleeding at fractional
  zooms (the PICA200's limited texcoord precision) is checked by the console screenshots of
  `examples/camera/tests/check.nnhs` (zoom 1.5 and 0.5; pending); if it shows, the plan's mitigation insets the
  source by 1/256 texel only when the destination isn't the tile size.
- **Tile cost** (Old 3DS XL, Development build, `examples/platformer`'s bench, `tests/gate.nnhs`): 4 full
  layers of 16×16 tiles on the top screen + 300 sprites run at avg 16.7 ms, worst 17.4 ms, 0 missed frames of
  600; the limits at 60 Hz are **6 layers** with 300 sprites and **600 sprites** with 4 layers. No fallback of
  decision D26 (a tighter vertex loop, pre-rendered chunks) was needed.
- **Textures from pixels** (`texture_create_rgba`, used for the built-in font): `C3D_TexInit(w, h,
  GPU_RGBA8)` in linear memory, then the CPU writes the GPU layout itself: 8×8 tiles left to right, rows
  of tiles from the **top** of the image, Morton (Z) order inside a tile with rows top-down too, each
  pixel a `u32` `0xRRGGBBAA`; then `C3D_TexFlush`. **No vertical flip** — measured on an Old 3DS XL
  with four variants of the font (flipping tiles and/or rows gave scrambled or upside-down glyphs). The
  font is a good test pattern: each glyph is exactly one tile. No citro2d text (`C2D_TextBuf`): all text
  is engine sprites.

## 3D drawing (spec 026, what our backend does)

> Status: built and linked (026a, mono); the first console run (the smoke and check scripts of `examples/models`)
> confirms the open points below.

- **Only with 3D on** (`AppConfig::gfx.max_draws_3d > 0`): `C3D_Init` gets 640 bytes of command buffer per 3D draw
  more; `video3d_init` replaces citro2d's top target (RGBA8 + 16-bit depth) by `C3D_RenderTargetCreate(240, 400,
  GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8)` on the same output (`C3D_RenderTargetSetOutput(…, GFX_TOP, GFX_LEFT, …)`,
  transfer flags as citro2d's). VRAM: the depth buffer grows from 192 000 to 384 000 bytes. A 2D game keeps
  citro2d's targets untouched.
- **2D in a 3D game** draws with the depth test off and colour writes only (`C3D_DepthTest(false, GPU_ALWAYS,
  GPU_WRITE_COLOR)` after every `C2D_Prepare()`), so 2D below the world never hides it; the frame-start
  `C2D_TargetClear` clears the depth to 0.
- **The pass** (`pass3d_3ds.cpp`): `C2D_Flush()`, bind the mesh shader, depth test `GPU_GREATER` with writes (blended
  draws: colour writes only), the light environment; per draw: the model-view (3×4, the quantisation decode folded in)
  and normal matrices, then — each only when it changes in the pass (`pass_cache3d.hpp`; citro3d itself resends what it
  is given: a vertex buffer costs 40 command words on every `C3D_GetBufInfo`, a `C3D_TexBind` clears the GPU's texture
  cache) — the uv decode and colour scale (base × tint / 255), the attribute layout and vertex buffer of the mesh's
  vertex format (12/16/20 bytes; missing texture coordinates and colours are fixed attributes, set once per pass),
  texture filter, wrap and binding, combiners, culling, alpha test or blending, then `C3D_DrawElements(GPU_TRIANGLES,
  n, C3D_UNSIGNED_SHORT, …)` from linear memory: 58–66 command words for a run of draws of one mesh (plan 026,
  "Optimisation round"). At the end: touched textures' parameters restored, lighting unbound, alpha test and culling
  off, every combiner reset, `C2D_Prepare()` and the 2D depth state.
- **Frame timing**: `C3D_FrameBegin(C3D_FRAME_SYNCDRAW)` (at the start of `nnh::frame()`) waits for the previous
  frame's GPU work and the GPU starts on a frame's commands only in `C3D_FrameEnd`, so a frame's CPU and GPU times add
  up; citro3d's `C3D_UpdateUniforms` scans all 96 float registers of the vertex and the geometry shader at every draw.
  Measured on the Old 3DS XL (console run 2): ≈ 110 µs of frame time per small 3D draw, ≈ 63 µs of it the GPU's.
  Overlapping them (the wait moved to the end of the frame) was measured and dropped: the ARM11 ran 45–50 % slower
  while the GPU worked (plan 026). Because `C3D_FrameEnd` returns with the GPU still drawing, core frees released
  textures and meshes only after the next `C3D_FrameBegin` (D47); `platform::finish_frames()` enters citro3d's next
  frame early (`C3D_FrameBegin(0)`, no public wait otherwise) and `shutdown()` closes it with `C3D_FrameEnd(0)`.
- **Shader** `engine/platforms/3ds/shaders/mesh3d.v.pica`, built by `picasso` through `ctr_add_shader_library` and
  embedded by `dkp_add_embedded_binary_library` (`nnh_mesh3d_shader_shbin.h`): raw int16 positions × model-view,
  int8 normals × normal matrix → the normal quaternion (citro3d's `fragment_light` method), the view vector, uv × scale
  + offset, colour × colour scale. Uniforms are always the instruction's first source (picasso refuses a uniform in
  the second one).
- **Projection**: core gives an OpenGL clip-space projection; `tilt3d_3ds.cpp` multiplies it by a constant matrix taken
  once from citro3d itself (`Mtx_PerspTilt(ref) × inverse(GL(ref))`): the PICA's depth range and the 90° screen
  rotation. A Development self-test compares the result with `Mtx_PerspTilt` and `Mtx_OrthoTilt` for other parameters
  and logs `3d tilt: PASS|FAIL` after the first frame.
- **Lighting** (`lights3d_3ds.cpp`): one `C3D_LightEnv`, 4 `C3D_Light`s (position w = 0 directional, 1 point), material
  ambient and diffuse 1 and specular 0 = the draw's specular colour; `D0` on N·H with a signed input, one table per
  shininess level (8 × 1 KB, filled from `engine/platforms/shared/raster3d/light_tables.cpp`, the simulator's tables),
  `C3D_LightEnvClampHighlights(true)`; point lights' distance attenuation through a `C3D_LightLutDA` with scale
  1/range and the shared (1 − x)² table. Combiners: `TEXTURE0 × PRIMARY` (or `PRIMARY`), then `× FRAGMENT_PRIMARY`,
  then `+ FRAGMENT_SECONDARY` (lit only).
- **Texture orientation**: the engine's v is 0 at an image's top (glTF); citro3d samples v = 1 there, so
  `pass3d_3ds.cpp` sends the shader's uv decode as `(scale.u, −scale.v, offset.u, 1 − offset.v)` (v' = 1 − v), as the
  2D subtextures do with `1 − y/H`. Confirmed by the probe check on the Old 3DS (spec 026, console run 3).
- **Stereo** (026b): `video3d_init` also makes the right eye's target (RGBA8 + D24S8,
  768 000 B of VRAM: 1.5 MB with the left one) on `GFX_TOP, GFX_RIGHT` with the same transfer flags. `eyes_3ds.cpp`:
  `set_stereo(on)` calls `gfxSet3D` only when the frame's eye count changes; `select_eye` swaps `ctx.targets[0]` between
  the two targets, so citro2d and the 3D pass draw on the selected eye, and clears the right one with `C2D_TargetClear`
  (colour, depth 0) at its first selection in a frame — it flushes and splits the frame first, so the clear is ordered
  after what came before. citro3d copies a target to its framebuffer only in frames that drew on it, so a mono frame
  leaves the right target alone. The slider is `osGet3DSliderState()` (`slider_3ds.cpp`, shared memory: no service
  call); the console model is read once (`system_3ds.cpp`), since core asks for the capabilities every frame with 3D
  on. `remote_capture_right` reads the right target after a stereo frame (else the top one) with the same marker wait.
  Checked on the Old 3DS XL (spec 026, plan "Stage 13 results"): the off-axis projection (`P[0][2] ≠ 0`), the right
  eye's clear and depth and `gfxSet3D` with the slider moving all pass; two eyes cost about 1.8 times one in the
  draw-bound case.
- **Never delete a render target inside citro3d's frame**: `C3D_RenderTargetDelete` there is a `svcBreak` (the console
  freezes). At exit core has already waited for the GPU through `finish_frames`, which leaves citro3d inside an empty
  frame, so `video3d_shutdown` deletes no target: `C3D_Fini` frees them all after `shutdown()` closes that frame.

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
- Send: Homebrew Launcher → **Y**, then `nnh run <game>` ([nnh tool](../guides/nnh-tool.md): its own netloader client,
  decision D23) or `3dslink <file>.3dsx -a <ip>` (`-a` takes only the IP).
- **Logs on the PC** (implemented): in Debug/Development builds started by 3dslink (`__3dslink_host` set),
  the platform library starts the socket service (`n3ds::network_start()`, `socInit` with a 1 MB buffer,
  reference-counted with the game's own use; `src/network_3ds.cpp`), connects to the PC on port 17491
  (`LINK3DS_COMM_PORT`) and `dup2`s the socket onto stdout/stderr, so every log line reaches the PC.
  Launch with `3dslink -s ...` so the PC keeps listening. We don't use libctru's `link3dsStdio()`: its
  `connect` blocks with no timeout, and when the PC isn't listening and its firewall drops the attempt
  the console hangs on a black screen. Our connect is non-blocking with a 1 s limit (`poll`; connected =
  `POLLOUT` without `POLLERR`/`POLLHUP` — `getsockopt(SO_ERROR)` reported a failure for a good
  connection on hardware; `link/src/net.cpp` does the same since NNH Link's first console try failed
  every connection with it); without a listening PC the game starts without logs. At shutdown, flush and `close()` fds 1 and 2 as well as the
  socket **before** `socExit()`, or newlib's exit-time stdio cleanup hangs the console on a black screen
  instead of returning to the Homebrew Launcher.
- **Console remote** (spec 013, Debug/Development; verified on an Old 3DS XL, 2026-11-21): `nnh run --script`
  and typed commands drive the game over that same log socket (`ctr::link_socket()`;
  [guide](../guides/testing-on-the-console.md)).
  - **Socket reads**: `src/remote_3ds.cpp` reads with `recv(..., MSG_DONTWAIT)` once per frame (inside the
    profile scope `remote`), at most what the 64-message queue has room for. `EAGAIN`/`EWOULDBLOCK`/`EINTR`
    mean "nothing yet"; `recv` returning 0 (the PC closed) or another error ends the remote for this run.
  - **Sends**: records go out after `fflush(stdout)`/`fflush(stderr)` (stdout is fully buffered on the
    socket, so earlier log lines stay first), in 16 KB pieces, blocking until written. `TCP_NODELAY` is set
    at the first remote send, so a log-only connection (`3dslink -s`, `nnh serve`) is unchanged.
  - **Read-back**: `src/remote_capture_3ds.cpp` copies a screen with `C3D_SyncDisplayTransfer` from its
    citro2d render target (`frameBuf.colorBuf`, `GX_BUFFER_DIM(240, 400|320)`, the target's own
    `transferFlags`, which must give linear `GX_TRANSFER_FMT_RGB8`, with `gfxGetScreenFormat()` =
    `GSP_BGR8_OES`) into one 288 000-byte `linearAlloc` buffer (from the first screenshot until the PC
    leaves or the game shuts down). `C3D_SyncDisplayTransfer` can return before its copy lands (its PPF wait
    may wake on the frame's own transfer): the buffer's last pixel is set to a marker first, and the CPU then
    waits (1 ms sleeps, `svcInvalidateProcessDataCache` each time, 100 ms at most) until it changes. The result is the framebuffer's
    layout: bytes B, G, R; screen columns left to right, each bottom to top. Confirmed on hardware: upright,
    not mirrored, the same colours as the simulator's screenshot; no fallback was needed. The read-back is
    refused while a frame is open (`ctr::Context::frame_open`): it must run after `end_frame()`.
  - **Measured costs** (Old 3DS XL, Development build, over Wi-Fi): a screenshot pair (top + bottom,
    ≈ 518 KB) takes **0.62–0.78 s**, during which the game doesn't run; that frame is logged as not
    representative. The per-frame poll (and a 12-byte tick every 4th frame) is too small to show: the tile
    gate ran attached to `nnh` with 4 layers + 300 sprites at avg 16.7 ms, worst 17.4 ms, 0 missed frames of
    600, and found limits of 6 layers / 600 sprites (spec 012 T3.9). Release builds have none of this code.
  - **Disconnect**: when `nnh` goes away (even killed without `Bye`), the next `recv` reports it and every
    remote control is released (checked on hardware with a held L).
- UDP on the console: answers to a socket bound to port 0 ("any") never arrived on hardware (NNH Link's
  search, 2026-11-21); bind a fixed port, as the Homebrew Launcher's netloader does (NNH Link binds 17492,
  `apps/nnh-link/src/net_client.cpp`). Sending broadcasts with `SO_BROADCAST` works.
- `.cia` (installable, spec 009): `nnh_add_game(... CIA UNIQUE_ID ...)` and the target `<game>_cia`; makerom (`-f cia -target t -exefslogo`, our RSF `cmake/3ds/nnhge.rsf`, the game's SMDH, a bannertool banner, the assets folder as romfs) and bannertool are fetched pinned on the first `.cia` build. Install with FBI over the network: `nnh install <game>` ([packaging guide](../guides/packaging.md)). romfs, saves on the SD card and exit to the HOME Menu work as in the `.3dsx`.
- Launching another `.3dsx` from a homebrew app: libctru only offers `aptSetChainloader()` for installed
  titles; there is no 3dsx-to-3dsx API in libctru. The Homebrew Launcher uses Luma3DS's loader service
  `hb:ldr` (Rosalina) directly, and so does `<nnh/3ds/launch.hpp>` (spec 011,
  [api](../api/3ds-extensions.md)). Facts (Homebrew Launcher `source/loaders/rosalina.c`; Luma3DS
  `sysmodules/loader/source/{hbldr,3dsx}.c`; libctru `initArgv.c`, `apt.c`):
  - `svcConnectToPort("hb:ldr")`; command 2 SetTarget (`IPC_MakeHeader(2,0,2)`, static buffer 0 = the
    path **without** `sdmc:`), command 3 SetArgv (`IPC_MakeHeader(3,0,2)`, static buffer 1 = a 0x400-byte
    block: `u32` argc, then NUL-terminated strings, argv[0] the full `sdmc:/` path; the loader always
    copies 0x400 bytes).
  - The loader clears the target after each load (the next one is `/boot.3dsx`, the Homebrew Launcher);
    argv stays. It sets `RUNFLAG_APTCHAINLOAD` in every `.3dsx` it loads, so libctru's `aptExit()`
    relaunches the same title, which loads the current target: set the target, then return from `main`.
  - libctru turns the **last** argument, if it is `XXXXXXXX_3DSLINK_` (17 characters, the log host's
    `s_addr` in hex), into `__3dslink_host` and removes it from argv.

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

## Live values on the 3DS (spec 030)

`nnh run --tunes/--hold` adds `--nnh-hold` to the netloader's argv; `poll_dev_requests()` returns it once
(Development only) and core holds the game before frame 1. While paused, voices and streams are held with
`ndspChnSetPaused` (through `voice_pause`/`stream_pause`) and released for a stepped frame.
