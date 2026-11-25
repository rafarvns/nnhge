# Spec 026: 3D foundation

Status: approved 2026-11-23 (overnight pre-approval by the maintainer: every "Decisions to take" item and open
question as recommended; decision 2, a CPU rasterizer in the simulator (D41,
reversing part of D20), confirmed by the maintainer the same night). Part 026a (mono) implemented and checked on the Old 3DS; part 026b
(stereo) implemented on the PC 2026-11-25, console run pending. Implemented after spec 022 (it uses 022's `sin_cos`)
Author: NNHGE maintainers · Date: 2026-11-23
Depends on: [spec 004](../004-2d-graphics/spec.md) (recorded per-screen passes, one texture type, layers; FR-11 to
FR-14 "Ready for 3D"), [spec 007](../007-assets-and-romfs/spec.md) (the assets folder, per-file conversion steps,
`load_texture` by path), [spec 002](../002-core-foundation/spec.md) (asserts, logs, `Result`, tagged memory, profiling,
capabilities: `Capability::Stereo3D`), [spec 022](../022-scene-tree-and-transforms/spec.md) (the engine's libm-free
`sin_cos` and its exact quarter-turn constants; decision D37) and D27 (`nnh-assetc` converts assets at build time).
Verified on the console with [spec 013](../013-console-remote/spec.md)'s `nnh run --script`. Takes over
[spec 006](../006-simulator-window/spec.md)'s deferred item "the 3D slider and stereoscopic preview". Implements the
first part of the ROADMAP's M8 ("citro3d meshes, materials, stereoscopic rendering, model import in `assetc`");
the rest of M8 is split into follow-up specs (end of this file). Optional for games (constitution XI, decision D25).

> Decision rows: this spec proposes **D41** (the PC simulator draws 3D with the engine's own CPU rasterizer, amending
> D20), **D42** (meshes: glTF 2.0 → `.nnhmesh` with quantised vertices, converted by `nnh-assetc`), **D43** (the 3D
> pass: a world layer splits the 2D list, the top screen is submitted once per eye, 3D is enabled by a capacity) and
> **D44** (3D math conventions and determinism). D37 to D40 belong to specs 022 to 025. The rows are written when the
> spec is approved.

## Problem and goal

NNHGE games can draw sprites, text, shapes and tile layers, but nothing in three dimensions: the 3DS's GPU (the
PICA200, with programmable vertex shaders, fixed-function per-pixel lighting and a depth buffer) and its signature
feature, the stereoscopic top screen driven by the 3D slider, are out of reach without leaving the engine for raw
citro3d — and then the game no longer runs in the PC simulator. After this spec a game can **load a model** made in a
common tool (Blender's glTF 2.0 export, converted at build time like Tiled maps and Aseprite sheets) or **build a mesh in
code**, give it a **material** (base colour, a texture, vertex colours, ambient/diffuse/specular lighting, cut-out or
blended transparency), light the screen with an **ambient colour and up to four lights**, point a **3D camera** at it
and **draw it on the top screen**, mixed with today's 2D (backgrounds behind the world, the HUD in front, the bottom
screen 2D). With the 3D slider up the top screen is drawn **once per eye** with the right parallax; with it down, or on
a 2DS, once. The same game runs in the simulator, which draws the same scene with the engine's own deterministic CPU
rasterizer (no new dependency, headless runs included). The math everything is built on (vectors, quaternions,
matrices, projections) gives the **same bits on PC and 3DS**; pixels are identical across PCs and close to the
console's. A game that draws no 3D pays nothing.

## User stories

- As a game developer, I want to export a model from Blender as glTF, drop it with its PNG texture into my assets
  folder and draw it with `load_mesh("models/ship")`, so that 3D content comes from the tool I already use.
- As a game developer, I want to build simple meshes in code (a cube, a plane, a sphere, or my own vertex arrays), so
  that I can prototype and write tests without any asset.
- As a game developer, I want a material per part of a model — colour, texture, vertex colours, lit or unlit, shiny or
  matte, opaque, cut-out (leaves, fences) or see-through (glass, ghosts) — with the model's own settings as defaults and
  my overrides per draw, so that a hit flash or a team colour is one value.
- As a game developer, I want an ambient colour and a few directional and point lights, and a model to look the same
  lit on the console and in the simulator, so that I tune lighting on the PC.
- As a game developer, I want a 3D camera I place with a position and a target (or a rotation), perspective or
  orthographic, so that a chase camera or an isometric view is a few lines.
- As a game developer, I want the 3D slider to just work — depth when it is up, a flat picture when it is down or on a
  2DS — with a strength and a focal distance I can set, so that stereo looks comfortable without per-eye code.
- As a game developer, I want 2D behind the 3D world (a sky) and 2D in front of it (the HUD, text, the debug overlay),
  with the bottom screen still all 2D, so that 3D adds to what I already have.
- As a game developer, I want to know where a 3D point lands on the screen (a name tag over a character) and the ray
  under a screen point, so that the HUD and later picking can follow 3D objects.
- As a game developer, I want positions, rotations, matrices and projections to give the same bits on PC and 3DS,
  so that replays and console checks still compare exactly.
- As a game developer, I want to know what 3D costs on an Old 3DS (triangles and draws per frame at 60 Hz, mono and
  stereo), so that I design scenes that hold the frame rate.
- As a game developer making a 2D game, I want my game, its size, its memory and its build time unchanged.
- As a contributor, I want the math, the mesh format, the recording and the PC rasterizer tested without a console.

## Functional requirements

### 3D math (values, usable without graphics)

- **FR-1 Value types.** A light header (only `<nnh/math.hpp>` and spec 022's `sin_cos`) provides trivially copyable
  values: a 3D vector, a 4D vector, a **quaternion** (rotation), a 4×4 **matrix**, and a **3D transform** (position,
  rotation quaternion, scale vector) that converts to a matrix. Operations: vector arithmetic, dot, cross, length,
  normalise (a zero vector stays zero and asserts in Debug); quaternion from axis and angle, from yaw/pitch/roll, product,
  conjugate, rotate a vector, normalised linear and spherical interpolation, to a matrix; matrix product, transpose,
  inverse (general, and a faster one for rigid and scaled transforms), transform a point and a direction; **look-at**
  (eye, target, up), **perspective** (vertical field of view, aspect, near, far) and **orthographic** projections. Names
  are fixed by the plan (decision D44); the 2D types (`Vec2`, `Rect`, spec 022's 2D transform) are untouched.
- **FR-2 Conventions.** Right-handed world, **+y up**, a camera looks down its **−z** (glTF's and OpenGL's
  conventions, so models need no conversion); one world unit is whatever the game wants (glTF's metre by default);
  angles in **radians**; matrices act on **column vectors** (`M × v`), stored column-major. Normalised device depth and
  the 3DS screen's 90° rotation are the backends' business: no public value depends on them.
- **FR-3 Same bits everywhere.** Every operation of FR-1 uses only IEEE `+ − × ÷`, square root (correctly rounded by
  IEEE on both hosts and the ARM11's VFP) and spec 022's `sin_cos` — no libm `tan`, `atan2`, `acos` or `pow` — in a
  fixed sequence, compiled with `-ffp-contract=off` like the rest of core. The same inputs give the same bits on PC
  (every supported host compiler) and 3DS. Exactness: the identity transform, quaternion and matrix are exact;
  rotations by the engine's quarter-turn constants map axes to axes exactly; a translation-only transform moves a point
  by exactly one addition per axis. Accuracy against a `double` reference (AC-3): directions and unit quaternions within
  2⁻¹⁸ per component, transformed points within 2⁻¹⁶ of the coordinate's magnitude for coordinates up to 4096 and
  chains of 8 transforms (the plan measures and confirms the bounds).

### Meshes

- **FR-4 What a mesh is.** A mesh is **one or more submeshes** (at most 32), each a list of **triangles** with
  16-bit indices into at most 65 535 vertices, drawn with one material. A vertex has a position (required), a normal
  (required by lit materials; FR-6 generates them when missing), optionally one texture coordinate set and optionally a
  colour. A mesh knows its local **bounds** (an axis-aligned box and a sphere) and each submesh's default material. The
  vertex data is **quantised** (decision D42): positions as 16-bit integers over the mesh's bounds, normals as 8-bit
  components, texture coordinates as 16-bit integers with a per-submesh scale and offset, colours as 8-bit RGBA — the
  plan fixes the layout (expected ≤ 20 bytes per vertex) and states the quantisation error, which the PC and the 3DS
  decode identically.
- **FR-5 Meshes built in code.** A game creates a mesh from its own arrays (float positions, normals, texture
  coordinates, colours, indices, submesh ranges and materials) in one call at load time, quantised exactly as the
  converter quantises the same data (the same bytes, AC-4). Helpers build a **cube**, a **plane** (subdivided) and a
  **UV sphere** (rings and segments) with normals and texture coordinates, deterministically (spec 022's `sin_cos`).
  Plain C++ with code-built meshes is a fully supported way to make a 3D game (constitution XI).
- **FR-6 Models from glTF 2.0.** A `.gltf` (with its `.bin` buffers) or `.glb` file in the game's assets folder is
  converted at build time by `nnh-assetc` into a `.nnhmesh` (D27, D42), one step per model, rerun only when the model or
  a file it uses changes. The converter reads the default scene's nodes and **bakes their transforms** into one mesh
  (one submesh per glTF primitive, merged when they share a material), triangle primitives (`mode` 4, indexed or not),
  `POSITION`, `NORMAL` (flat normals generated when missing, as glTF says), `TEXCOORD_0`, `COLOR_0`, and each
  material's `baseColorFactor`, `baseColorTexture`, `alphaMode`, `alphaCutoff`, `doubleSided`, `KHR_materials_unlit`
  and the texture sampler's filter and wrap modes. It **ignores with a note** (the model still builds) skins,
  animations, morph targets, cameras, lights, extra texture coordinate sets, metallic/roughness and normal, occlusion and
  emissive maps; it **refuses** with `file: reason` (and the JSON path): non-triangle primitives, sparse accessors,
  required extensions it doesn't know (Draco, meshopt, texture transforms…), images embedded in the file or a data URI,
  more than the limits of FR-4 after merging, and malformed or out-of-range data. A texture must be a PNG **in the same
  assets folder**, following the texture rules (power-of-two sides from 8 to 1024; constitution II), and becomes an
  ordinary engine texture (spec 007's conversion).
- **FR-7 Bounded, checked, fuzzed.** The converter has fixed limits (file size, buffers, accessors, nodes, materials,
  JSON depth — numbers fixed by the plan), checks every index and accessor range, and is fuzzed; each refusal has a
  fixture in `tests/data/` (as specs 012, 014 and 016). The `.nnhmesh` reader on the console validates its header,
  version, sizes, ranges and every index before use, never trusts the file, and is fuzzed (constitution X).
- **FR-8 Loading.** `load_mesh("models/ship")` loads a converted mesh from the assets, with its textures, and returns
  a `Result` with an owning handle (RAII: destroying it frees its memory; a draw recorded before is skipped, as 2D does
  with textures). Mesh memory is accounted in the `Linear` region under a `meshes` tag on every system (the PC accounts
  the bytes the console would use). Loading may allocate; nothing allocates afterwards.

### Materials and lights

- **FR-9 Materials.** A material is a plain, trivially copyable value: **base colour** (RGBA), an optional
  **base-colour texture**, **vertex colours** on or off, **lit** or **unlit**, a **specular colour** and a
  **shininess** (from a fixed set of levels; the plan fixes how many, expected 8), an **alpha mode** — opaque, cut-out
  (with a threshold) or blended — **back-face culling** on or off, and the texture's **filter** (nearest or linear) and
  **wrap** (repeat, clamp, mirror). Each submesh has a default material from its model or its code; a draw may replace
  any submesh's material or multiply a **tint** into all of them.
- **FR-10 One shading formula.** The colour of a pixel is, with every value in 0..1:
  `surface = texture × vertex colour × base colour`; unlit: `surface`; lit:
  `surface.rgb × (ambient + Σ diffuseᵢ × max(N·Lᵢ, 0)) + Σ specularᵢ × specular colour × (N·Hᵢ)^shininess`
  (Blinn-Phong, per pixel), clamped, with `surface.a` as the alpha. Cut-out discards pixels whose alpha is below the
  threshold; blended draws mix with what is behind them. The 3DS computes it with the PICA200's fragment lighting
  (lookup tables) and texture combiners within its six stages; the PC computes the same formula (the plan shows both
  mappings). Colours may differ between them by the documented tolerance (FR-24).
- **FR-11 Lights.** Per screen and per frame: an **ambient colour** and **up to 4 lights**, each **directional**
  (a direction and a colour) or **point** (a position, a colour and a range with the documented falloff, zero at the
  range). Set like the camera, by value, before or during the frame. Default when a game sets none: ambient 0.3 grey and
  one white directional light from the upper front left, so that a first model is visible.

### Camera and stereo

- **FR-12 A 3D camera per screen.** A camera is a value: a position, an orientation (a quaternion, or set by look-at),
  a **perspective** (vertical field of view, near, far) or **orthographic** (visible height, near, far) projection, and
  the stereo settings of FR-14. The aspect is the screen's (400:240 on the top screen). In this spec only the **top
  screen** draws 3D (the bottom screen is a follow-up); the API names the screen so it can be added without changes.
- **FR-13 Screen and world.** `to_screen(point)` gives a world point's position in screen pixels ((0, 0) top-left, as
  2D), its depth and whether it is in front of the camera and inside the view — for the centre eye (the HUD sits at
  screen depth); `screen_ray(pixel)` gives the world ray under a screen pixel. Both are pure FR-3 math: the same bits on
  PC and 3DS.
- **FR-14 Stereo from the slider.** On a system with `Capability::Stereo3D`, when the slider is above 0 and the game
  has not turned stereo off, the top screen is drawn **once per eye**: each eye's camera is moved sideways by half the
  eye separation and uses an **off-axis** (asymmetric) projection, so that a point at the camera's **focal distance**
  appears at screen depth, nearer points in front of the screen and farther ones behind. Eye separation =
  slider × the camera's **stereo strength** (world units; default derived from the focal distance, the plan fixes it).
  Orthographic cameras draw the same picture for both eyes (no 3D depth; Non-goals). With the slider at 0, on a 2DS, or
  in a 2D-only game, the screen is drawn once and nothing extra is spent.
- **FR-15 2D in stereo.** When the top screen is drawn per eye, its 2D layers, text and the debug pass are drawn
  identically in both eyes (at screen depth). Per-layer stereo depth for 2D is a follow-up.
- **FR-16 The slider everywhere.** A portable query gives the slider's value (0..1; 0 without the capability). The
  simulator has the capability, a slider the developer moves with keys (shown in the window's status, remembered like
  spec 006's settings) and an environment variable to set it for screenshots. In Debug/Development builds a test script
  sets it (a new `.nnhs` command), on the simulator and on the console (where it replaces the hardware slider's value for
  the engine), so that scripts check stereo with nobody touching the console.

### Drawing

- **FR-17 Recording.** Inside a frame, `draw(mesh, transform [, options])` records a draw of the whole mesh or one
  submesh for the top screen: a model matrix (or a 3D transform), optional material overrides, a tint. Draws are
  recorded like 2D (no GPU work at the call, no allocation) up to a capacity set at `init()` (FR-26); draws past it
  are dropped, counted and assert in Debug, as 2D does. Drawing outside a frame is ignored (as 2D).
- **FR-18 Where the world sits among the 2D layers.** Each screen has a **world layer** (0..16, default 8): when the
  frame has 3D draws, 2D layers below it are drawn first (a sky), then the 3D pass, then the 2D layers from it up (the
  HUD), then the debug pass, as [rendering.md](../../docs/architecture/rendering.md) foresaw. 2D drawn after the 3D pass
  is never hidden by 3D depth; 2D drawn before never hides 3D. A frame without 3D draws submits the 2D list exactly as
  today (byte-identical screenshots, AC-9).
- **FR-19 Order, depth and transparency.** Opaque and cut-out draws are drawn first, in call order, with depth test
  and write; blended draws then, **sorted back to front** by the camera-space depth of their bounds' centre (ties in
  call order), with depth test and no depth write. Triangles inside one blended draw are not sorted (Non-goals). The
  depth buffer is cleared for each eye; the colour is the screen's clear colour plus the 2D drawn below the world. The
  order is deterministic and the same on every system.
- **FR-20 Culling.** A draw whose world bounds are outside the view (both eyes' union in stereo) is skipped on the CPU
  before any GPU work and counted. Back faces are culled per material. Triangles crossing the near plane are clipped
  (not dropped) on every system.
- **FR-21 Same results everywhere.** Everything a game can read — math, `to_screen`/`screen_ray`, culling decisions,
  the draw order, the stats — gives the same values on PC and 3DS for the same calls (decision D44). Pixels are each
  backend's rasterisation: identical across PCs (FR-23), close between the PC and the console (FR-24).

### The PC simulator and the fake platform

- **FR-22 The simulator draws 3D on the CPU** (decision D41): the PC platform rasterises the 3D pass itself into a
  per-screen colour and depth buffer at the console's resolution, then puts it between the 2D slices; no new library,
  no shader toolchain, and it works with the GPU-device renderer, SDL's default renderer and `SDL_VIDEO_DRIVER=offscreen`
  alike. It implements FR-10's formula per pixel, perspective-correct texture coordinates and colours, a depth buffer,
  nearest and bilinear filtering, the three wrap modes, cut-out, blending, back-face culling and near-plane clipping,
  with a documented fill rule and sub-pixel precision. It enforces the console's limits (texture rules, FR-4's mesh
  limits, capacities). It shows the left eye on the top screen; a simulator key switches the view between the left eye,
  the right eye and a red/cyan anaglyph preview (a simulator aid; the console only has the real thing).
- **FR-23 Deterministic simulator pixels.** The rasteriser uses fixed-point edges and a fixed sequence of float
  operations under `-ffp-contract=off`, so a 3D frame gives the **same PNG bytes** on Windows and Linux with every
  supported host compiler, with or without a GPU device or a display; screenshots of 3D can be golden hashes (AC-5).
- **FR-24 Close to the console.** Simulator and console pictures of the same frame match within a documented
  tolerance — the same triangles cover the same pixels except along edges, and lit colours differ by at most a small
  amount per channel (the plan proposes the numbers, the first console run confirms them). The example's check mode
  verifies it on both systems by reading back fixed **probe pixels** of fixed scenes and comparing them with FR-10's
  formula (AC-7).
- **FR-25 The fake platform records the 3D pass** (the draws as submitted: mesh, submesh, matrices per eye, material,
  lights, depth and blend state, eye) for unit tests, and can run the same CPU rasteriser for pixel tests without a
  window.

### Capacity, cost, safety, opt-in

- **FR-26 3D is enabled by a capacity.** `AppConfig`'s graphics settings gain a capacity of 3D draws per frame
  (default **0 = 3D off**). With 0, `draw` asserts and does nothing, no 3D memory, render targets or shaders are set
  up, and stereo is never turned on. With a capacity the engine reserves, once at `init()`: the draw records (the plan
  states the bytes, expected ≤ 96 each), the top screen's depth buffers (24-bit depth with 8-bit stencil) and right-eye
  target in VRAM (expected ≈ 1.5 MB of the 6 MB with both eyes; the plan states it), and the lighting tables.
- **FR-27 Bounded cost, no allocation while playing.** After loading, nothing in 3D allocates, logs in the normal path
  or calls `virtual` functions. Recording is constant time per draw; culling and sorting linear and `n log n` in the
  frame's draws; vertex data stays in linear memory and is never touched by the CPU after loading; the per-draw GPU
  state is matrices, material and lights only. Costs are measured on an Old 3DS (AC-8).
- **FR-28 Stats and debug drawing.** Stats: draws recorded, culled, dropped, triangles submitted per eye, eyes drawn,
  meshes and mesh memory, peak draws — a debug overlay line. A debug draw (Debug and Development only; nothing in
  Release) draws each draw's bounds and the world axes projected into the 2D debug pass.
- **FR-29 Misuse is loud in Debug, harmless in Release.** A draw outside a frame or with 3D off, a destroyed or null
  mesh, a submesh index out of range, a NaN or infinite matrix, a non-invertible camera, a material with an invalid
  value, more than 4 lights — assert in Debug/Development and do nothing (or clamp) in Release; never undefined
  behaviour or a GPU hang.
- **FR-30 No events.** The engine publishes no event for 3D (constitution IV): cameras, lights and stats are values
  and queries.
- **FR-31 Optional and light.** The 3D math header includes only `<nnh/math.hpp>` (and spec 022's light transform
  header for `sin_cos`); the 3D graphics header includes no SDL, citro or standard container header. A game that doesn't
  use 3D links no 3D code on either system (`examples/hello` keeps its size; the 3D platform code, shader and lighting
  tables are linked only when the game draws 3D, the way D32's hook keeps actions out of `hello`); 2D-only games keep
  their render targets, VRAM use and frame cost. The console's vertex shader is written in PICA200 assembly and built
  with devkitPro's own `picasso` through its CMake support (`ctr_add_shader_library`): no new tool. Build budgets are
  measured before and after (constitution III).

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform. Math: every operation of FR-1, conventions
  (FR-2: handedness, look-at down −z, column vectors), exact cases of FR-3, normalising zero. Meshes: code-built meshes
  and the three primitives (vertex counts, normals, bounds), quantisation round trips and its error bound, submesh limits,
  the `.nnhmesh` reader on valid and corrupted data. Materials: defaults, overrides, tint, invalid values. Recording:
  capacity and drops, draws outside a frame, stale meshes, 3D off. The pass: the world-layer split (2D before/after,
  debug last), opaque in call order then blended back to front with ties in call order, culling (inside, outside,
  straddling, stereo union), per-eye matrices (off-axis, focal plane at screen depth, slider 0 → one eye), orthographic
  stereo (same picture), lights (defaults, 4 max). `to_screen`/`screen_ray` round trips. Misuse cases (FR-29). The Linux
  build VM runs them with GCC 13/14 and clang 18 and with ASan/UBSan, as previous specs did.
- **AC-2 Golden values.** A test runs a fixed, seeded script of 50 000 operations (the engine's integer hash, no `rand`):
  transforms composed and inverted, quaternions built, multiplied and interpolated, look-at and projections built, points
  projected with `to_screen` and rays cast with `screen_ray`, frustum tests and a recorded frame's sorted order — and
  compares a checksum of every result's bits with a golden value. The same test passes on every host compiler and in the
  console self-check (AC-7).
- **AC-3 Accuracy.** Unit tests compare FR-1's operations on seeded inputs with a `double` reference: within FR-3's
  bounds; quarter turns exact.
- **AC-4 The converter.** Fixtures in `tests/data/` (all hand-made or generated by a script in the repository; no
  third-party models): a cube with a texture, a multi-node model with baked transforms, vertex colours, a model without
  normals (flat normals generated), cut-out and blended materials, a `.glb`; and one fixture per refusal of FR-6/FR-7
  with its expected `file: reason`. A model built in code and the same data converted from glTF give the same
  `.nnhmesh` bytes. Editing a model reconverts only that model (`ninja -d explain`), on PC and 3DS. The glTF reader and
  the `.nnhmesh` reader each have a libFuzzer target (and a mini-fuzz case in `nnh_tests`), run clean for 10 minutes on
  the Linux build VM with ASan.
- **AC-5 Simulator rasteriser.** Conformance tests read pixels back (headless): a triangle's coverage follows the fill
  rule (two triangles sharing an edge leave no gap and no double-drawn pixel), depth ordering of intersecting
  triangles, a perspective-correct checkerboard on a tilted plane, near-plane clipping, cut-out and blended pixels,
  back-face culling, nearest and bilinear filtering and the wrap modes, and lit pixels equal to FR-10's formula within
  ±1 per channel. The example's showcase screenshots (AC-6) have **golden hashes** that match on Windows (MinGW GCC) and
  on Linux (GCC 13/14, clang 18), with the GPU-device renderer, `NNH_SIM_RENDERER=sdl` and `SDL_VIDEO_DRIVER=offscreen`.
- **AC-6 Example** (`examples/models`, name fixed by the plan; original art and models only), modes chosen by an
  environment variable (`showcase` (default) | `check` | `bench`). The top screen: a spinning, lit, textured model
  converted from glTF in the assets, on a floor plane, with a code-built cube and sphere, a cut-out fence, a blended
  glass pane in front of other objects, a directional light and a point light circling the scene; a 2D sky gradient
  below the world layer and a HUD (title, frame time, stats) above it; a name tag over the model placed with
  `to_screen`. The Circle Pad orbits the camera, L/R zoom, a button switches perspective and orthographic. The bottom
  screen (2D): touch buttons that toggle the texture, lighting, the point light, culling, the debug draw and the
  material of the model (a tint flash), and the slider's value. PC screenshots of the showcase (left eye, right eye with
  the slider at 1, anaglyph view, orthographic, debug draw), checked by reading the PNGs.
- **AC-7 Console script.** The `check` mode runs AC-2's script and logs `3d: <n> sum <checksum>` every 10 000 operations
  and `3d: PASS <checksum>` or `3d: FAIL ...` at the end, against golden values recorded from the simulator; then it
  draws fixed probe scenes (flat colours, a textured quad, a lit sphere, a cut-out, a blend over depth, a 2D HUD over 3D
  and 2D behind it, stereo with the slider set to 1) and reads back fixed probe pixels of each eye, compared with
  FR-10's expected values within FR-24's tolerance: `3d pixels: PASS` or `3d pixels: FAIL <scene> <x>,<y> got … want …`.
  `examples/models/tests/check.nnhs` starts it, expects every line, sets the slider (FR-16), switches to the showcase,
  presses fixed inputs, expects logged `to_screen` answers (for example `tag: 212,96 depth 4.25`) and takes screenshots
  of both eyes. On an Old 3DS XL the maintainer only opens the Homebrew Launcher and presses Y, and `nnh run models
  --script examples/models/tests/check.nnhs` passes; the same script passes with `--pc`. The verdict, log and
  screenshots are in the saved log folder; the console's screenshots are compared with the PC's by reading them.
- **AC-8 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `3d bench: PASS <numbers>` (or FAIL), each case over 600 frames with the frame time's average and worst and the count
  of missed 60 Hz frames, mono and with the slider at 1. **Targets** (the plan confirms them before code; they predate any
  console measurement and are estimated from DMP's figure of 15.3 M polygons/s at 200 MHz — a theoretical peak never
  reached in practice — and from homebrew citro3d ports that draw about 600–1700 triangles per frame and are limited by
  the 268 MHz CPU building commands, not by the GPU; NNHGE keeps vertices static in linear memory, so the CPU's share is
  per draw, not per vertex):
  - **throughput**: one textured, lit (1 directional light) mesh of **12 000 triangles** filling about half the top
    screen, plus the HUD: **60 Hz, no missed frame** mono; **6 000 triangles** in stereo;
  - **draws**: **256 draws** of a 12-triangle cube (two materials, 1 directional + 2 point lights) plus the HUD: 60 Hz
    mono, with the CPU time of recording, culling, sorting and submitting **under 4 ms** (under 7 ms in stereo);
  - **CPU alone**: recording and culling **512 draws** (half of them outside the view) and sorting 128 blended ones:
    **under 1.0 ms**;
  - **fill**: three full-screen blended planes over an opaque scene: 60 Hz mono;
  - **2D unchanged**: spec 012's tile gate (4 layers + 300 sprites at 60 Hz) still passes in a 2D-only build.
  **Budgets set 2026-11-25** (mono, from console run 2 + ≈ 25 %, after the T10.5 optimisation round and the CPU/GPU
  overlap trial of D47; plan "CPU/GPU overlap tried (D47) and budgets set"): throughput and fill **60 Hz** (met);
  draws: frame average **under 35.5 ms** (measured 28.34; 60 Hz is out of reach on an Old 3DS: ≈ 65 µs of GPU per small
  draw) and CPU share **under 14.8 ms** (11.82); CPU alone **under 8.8 ms** (7.04). Stereo budgets come from 026b's
  console run (T13.5: the bench's `stereo` cases, logged "not judged" until then).
  If a target is missed, the implementation is optimised first; if it is still missed, the budget is set from the first
  console measurement with margin and written in the spec and the docs with the measured numbers (the maintainer's
  policy for benches, as in specs 019 to 022). The simulator's cost is measured too (pc-dev: the showcase's 3D pass under
  4 ms per frame on the maintainer's PC; same policy).
- **AC-9 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS and contains no 3D
  symbol (checked on the 3DS map file or symbol list); every existing example keeps its screenshots pixel-identical and
  its scripts passing (2D-only frames go through the old path); every earlier spec's tests pass unchanged; 2D-only games
  keep their render targets and VRAM use (checked by the memory report). Building a game `.cpp` that includes the 3D
  headers, a clean engine build and editing a model stay within constitution III's budgets (measured before and after).
- **AC-10 Docs.** New `docs/api/` pages for the 3D math and for 3D drawing (meshes, glTF export guidance from Blender,
  materials and the shading formula, lights, the camera, stereo and the slider, the world layer, order and
  transparency, culling, determinism and tolerance, costs, memory, misuse); a guide for exporting models; links from
  `docs/api/math.md`, `gfx.md`, `texture.md`, `assets.md`, `simulator.md` (slider keys, eye views) and `3ds-extensions.md`
  (the portable slider query beside `slider_3d()`); `docs/architecture/rendering.md` (the 3D pass, eyes, the PC
  rasteriser), `docs/tech/devkitpro-3ds.md` (render targets, shader, lighting tables, VRAM, measured costs) and
  `docs/tech/sdl3.md` updated; decision rows D41–D44; CLAUDE.md layout and ROADMAP updated; `docs/REFERENCES.md` citing
  the prior art below.

## Non-goals

Each is a follow-up spec or later work (see "Follow-up specs").

- **Skinned and morph animation**, bones, blending of 3D animations, and **glTF animations** (ignored by the converter).
- **3D hierarchies** (spec 022 is 2D): a game composes its own matrices; 3D nodes in the entity world come later.
- **3D collision**, raycasts against meshes, picking objects by touch (beyond `screen_ray`), physics.
- **Shadows** (shadow maps, the PICA's shadow textures), **fog**, normal maps, environment and cube maps, toon or
  custom lighting tables, spot lights, more than 4 lights, emission maps, physically based materials.
- **Custom shaders**, geometry shaders, procedural textures, render to texture, post effects, anti-aliasing, the 800-px
  wide mode.
- **3D on the bottom screen** (the API names the screen so it can come later).
- **Per-layer stereo depth for 2D games** and 2D sprites placed in 3D space (billboards).
- **Stereo for orthographic cameras** (both eyes see the same picture).
- **Particles in 3D** (spec 024 is 2D), lines and points in 3D (the GPU draws triangles; debug lines are projected
  into the 2D debug pass).
- **Mipmaps**, compressed textures (ETC1), texture formats other than today's, and textures embedded in models.
- **Sorting triangles** inside a blended mesh, order-independent transparency.
- **Instancing**, levels of detail, occlusion culling, batching several meshes into one draw.
- **Other model formats** (OBJ, FBX, Collada) and an editor or model viewer tool.
- **Pixel-identical output** between the simulator and the console (FR-24's tolerance instead).
- **Positional (3D) audio**.

## Constraints

- **I** — the same game code on PC and 3DS: the simulator implements the whole 3D API (FR-22); no `#ifdef` in games or
  public headers; the slider is a portable query (FR-16) next to the existing 3DS extension `slider_3d()`; the example
  and its script run on both.
- **II** — design for the Old 3DS: VRAM 6 MB (two 3 MB banks) holds the screens' targets, so the top screen's per-eye
  colour and 24-bit depth buffers (≈ 384 KB each at 400×240×4) are reserved only with 3D on (FR-26); vertex and index
  data live in the 32 MB linear heap, quantised to ≤ 20 bytes per vertex (≈ 130 KB for AC-8's 12 000-triangle mesh);
  textures keep the power-of-two 8–1024 rule; the simulator enforces the same limits (D4). Costs measured on an Old 3DS
  XL (AC-8); a New 3DS is not required (the maintainer has only an Old 3DS XL, and a 2DS has no stereo).
- **III** — light public headers; the console shader is built by devkitPro's own `picasso` (one more build step for
  the 3DS platform library, measured); the glTF converter lives in `nnh-assetc`, reusing its JSON reader; `hello`'s
  edit-build times unchanged (AC-9).
- **IV** — no events; state is queried (FR-30).
- **V** — no exceptions, RTTI or `virtual`; recording, culling, sorting and submission allocate nothing (FR-27).
- **VI** — math, meshes, recording, ordering and culling unit-tested first on the fake platform with a golden checksum
  and a `double` reference; the rasteriser by pixel-reading conformance tests; hardware behaviour by screenshots and the
  console script's probe pixels (AC-1 to AC-7).
- **VII** — math, mesh format and quantisation, primitives, materials, lights, camera and stereo, the draw list, the
  culling, the sort, the PC rasteriser's stages, the 3DS pass and shader in small separate files; one public header per
  module.
- **VIII** — header comments, `docs/api/` pages, the export guide, the example (AC-10).
- **IX** — written from published documentation (citro3d's headers and examples, 3dbrew, the glTF specification,
  rasterisation articles); no code copied from incompatible licences; the example's model and texture and every test
  fixture are original (no Khronos sample models, whose licences vary).
- **X** — two new parsers (glTF in the host converter, `.nnhmesh` on the console), both bounded, validated and fuzzed
  (FR-7); misuse loud in Debug, harmless in Release, never a GPU hang (FR-29).
- **XI, D25** — 3D is optional (FR-26, FR-31); the math is usable without graphics; meshes from code need no assets;
  models need no engine-specific tool beyond the build.
- **Under pressure:**
  - **D20** says "PC: SDL renderer on an `SDL_GPUDevice` … 3D will need a GPU device", and
    [rendering.md](../../docs/architecture/rendering.md) and [sdl3.md](../../docs/tech/sdl3.md) say the 3D renderer
    will share that device. The recommended PC backend (FR-22, D41) is a CPU rasteriser instead: 3D works headless and
    on SDL's default renderer, screenshots are golden, and no shader toolchain or new dependency arrives — at the price
    of CPU time in the simulator and pixels that are close to, not equal to, the console's. D20's row and those pages
    change when D41 is accepted; the GPU-device renderer stays for 2D.
  - **Constitution I/D4** ("the simulator enforces the same limits") holds for sizes and capacities but not for speed:
    the simulator's CPU rasteriser doesn't model the PICA200's throughput, so a scene can run at 60 Hz on the PC and not
    on an Old 3DS. AC-8's console bench and FR-28's triangle counts are the guard.
  - **"The same bits on PC and 3DS"** covers math and every value a game reads (FR-21), not 3D pixels: the PICA200's
    lighting tables and rasteriser are not the PC's. This is the first feature whose console screenshots are compared
    with a tolerance; the console check therefore reads probe pixels in the game (FR-24, AC-7) instead of comparing whole
    pictures, and the PC's pixels are golden on their own (FR-23).
  - **Spec 004's FR-13** (submit the recorded list once per eye) costs the top screen's 2D twice when stereo is on;
    measured in AC-8. Its "layer can get a stereo depth" stays a follow-up (FR-15).
  - **Spec 006's non-goal** (the slider and stereo preview) is taken over here (FR-16, FR-22).
  - **D5** ("stereoscopic 3D later on citro3d") is fulfilled; citro2d keeps the 2D, the 3D pass is raw citro3d between
    a 2D flush and citro2d's re-preparation, as the devkitPro `composite_scene` example does.
  - **Spec 022** must land first (its `sin_cos`); if it slips, the plan moves `sin_cos` into a shared light header
    without changing either spec's behaviour.
  - **AC-8's** targets predate any console measurement and fall back to the measurement-with-margin policy.

## Prior art

- **citro3d and devkitPro's examples** — render targets with `GPU_RB_DEPTH24_STENCIL8`, `C3D_FrameDrawOn`, uniforms,
  `Mtx_PerspStereoTilt` (stereo projection tilted for the 3DS's rotated screen), `C3D_LightEnv`/`C3D_Light` (8 hardware
  lights, lookup tables), six texture combiner stages; the `composite_scene` example draws citro3d and citro2d on the
  same per-eye targets (`iod = osGet3DSliderState() / 3`, the right eye only when `iod > 0`, `C2D_Prepare()` after the
  3D) — [3ds-examples graphics/gpu](https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu)
  (`composite_scene`, `fragment_light`, `stereoscopic_2d`, `textured_cube`), [citro3d](https://github.com/devkitPro/citro3d),
  [citro2d `base.c`](https://github.com/devkitPro/citro2d/blob/master/source/base.c) (`C2D_Prepare` re-binds its shader,
  attributes and buffers, sets a `GEQUAL` depth test and no culling; screen targets use a 16-bit depth buffer), installed
  headers `c3d/light.h`, `c3d/framebuffer.h`, `c3d/maths.h`, `3ds/gfx.h` (`gfxSet3D`), `3ds/os.h`
  (`osGet3DSliderState`), `cmake/Platform/Nintendo3DS.cmake` (`ctr_add_shader_library`). A forum report that mixing a
  shader with a geometry stage and one without in one frame hangs real hardware —
  [devkitPro forum t=9261](https://devkitpro.org/viewtopic.php?f=39&t=9261) — is one reason custom and geometry shaders
  are non-goals.
- **3dbrew** — [GPU/Fragment Lighting](https://3dbrew.org/wiki/GPU/Fragment_Lighting) (no pixel shader; dot products
  index lookup tables; Blinn-Phong with D0 on N·H), [GPU/Internal Registers](https://www.3dbrew.org/wiki/GPU/Internal_Registers)
  (six TexEnv stages, lighting LUT selectors), [Nintendo OpenGL](http://3dbrew.org/wiki/Nintendo_OpenGL),
  [GPU/Shader Instruction Set](https://www.3dbrew.org/wiki/GPU/Shader_Instruction_Set).
- **PICA200 figures** — DMP's 15.3 M polygons/s and 800 M pixels/s at 200 MHz, a theoretical peak —
  [Wikipedia: PICA200](https://en.wikipedia.org/wiki/PICA200). Homebrew practice: citro3d ports logging 600–1700
  triangles per frame, CPU-bound by command building
  ([a Diddy Kong Racing port's GPU bench](https://github.com/chrispelletier-gif/Diddy-Kong-Racing-3DS/issues/229),
  [issue 588](https://github.com/chrispelletier-gif/Diddy-Kong-Racing-3DS/issues/588)); citro3d's CPU time 1.2 ms
  median on an Old 3DS in [sh23ds PERFORMANCE.md](https://github.com/ankushKun/sh23ds/blob/main/port/PERFORMANCE.md);
  an engine limited to ~15 fps on an Old 3DS by a CPU path
  ([mgarcia.org blog](http://mgarcia.org/2018/08/24/nintendo-3ds-game-development-blog-002-2d-performance-and-health/)).
  No clean "triangles at 60 Hz" figure exists: AC-8 measures it.
- **glTF 2.0** — [Khronos glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) (right-handed,
  +y up, metres, flat normals when `NORMAL` is missing, `alphaMode`, `doubleSided`),
  [`KHR_materials_unlit`](https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_unlit).
  Parsers such as cgltf and tinygltf (MIT) are references only; `nnh-assetc` reads glTF with its own bounded JSON reader.
- **raylib** — the smallest familiar 3D API: `Camera3D` (position, target, up, fovy, projection), `LoadModel` from glTF,
  `DrawModel`, `GenMesh*` primitives, `GetWorldToScreen` —
  [cheatsheet](https://www.raylib.com/cheatsheet/raylib_cheatsheet_v4.0.pdf),
  [glTF loading example](https://www.raylib.com/examples/models/loader.html?name=models_loading_gltf). FR-5, FR-8,
  FR-12, FR-13 take this shape.
- **Godot and Unity** — opaque first, then transparent sorted back to front by object distance, no per-triangle sort;
  `StandardMaterial3D`'s transparency modes (opaque, alpha scissor, alpha) and cull modes; `Camera3D.unproject_position`
  / `project_ray_origin`; Unity's render queues and `Camera.WorldToScreenPoint` —
  [Godot: 3D rendering limitations (transparency sorting)](https://docs.godotengine.org/en/stable/tutorials/3d/3d_rendering_limitations.html),
  [Godot: Standard Material 3D](https://docs.godotengine.org/en/stable/tutorials/3d/standard_material_3d.html),
  [Unity: Camera.WorldToScreenPoint](https://docs.unity3d.com/ScriptReference/Camera.WorldToScreenPoint.html). FR-9,
  FR-13 and FR-19 follow them.
- **Software rasterisation** — Fabian Giesen, ["Triangle rasterization in practice"](https://fgiesen.wordpress.com/2013/02/08/triangle-rasterization-in-practice/)
  (fixed-point edge functions, the top-left fill rule, sub-pixel precision); Chris Hecker,
  [perspective texture mapping series](https://www.chrishecker.com/Miscellaneous_Technical_Articles) (perspective-correct
  interpolation). FR-22/FR-23 follow them.

## Decisions to take

Every question has a recommended answer; none is decided until the maintainer approves the spec.

- [x] **Scope of the first slice.** — **recommended: static meshes (code and glTF), materials with the FR-10 formula,
  ambient + 4 lights, a perspective/orthographic camera, slider stereo, the world-layer split with 2D, CPU culling, the
  PC rasteriser, the example** — everything else in Non-goals and the follow-up list. Small enough to verify on the
  console, enough for a simple 3D game (a racer, a puzzle, a model viewer).
- [x] **The PC backend (D41).** Options: (a) **the engine's own CPU rasteriser in the simulator** (deterministic, headless,
  no dependency, enforces limits, models the PICA's fixed function closely); (b) SDL 3's GPU API on the device the 2D
  renderer already uses (needs SPIR-V/DXIL/MSL shaders — a shader compiler such as SDL_shadercross or precompiled blobs
  per backend — no 3D headless or with `NNH_SIM_RENDERER=sdl`, pixels vary by GPU and driver, and its command buffers
  must be interleaved with SDL_Renderer's); (c) OpenGL (a new dependency path beside the GPU renderer); (d)
  `SDL_RenderGeometry` with CPU transforms and painter's sorting (no depth buffer, affine textures: visibly wrong). —
  **recommended: (a)**. At 400×240 per eye a CPU rasteriser is cheap on a desktop, and golden screenshots of 3D are worth
  more to this project's way of testing than GPU speed. (b) stays possible later behind the same contract if scenes
  outgrow the CPU.
- [x] **Where the rasteriser lives.** — **recommended: portable code shared by the PC and the fake platform** (tested
  without a window, FR-25), not inside core (games never call it) and not tied to SDL (it fills a buffer the PC
  platform shows). File placement fixed by the plan.
- [x] **Math conventions (D44).** — **recommended: right-handed, +y up, camera down −z, column vectors, column-major
  storage, radians, quaternions for rotations** (FR-2), glTF's conventions so models load unchanged.
- [x] **Math determinism.** Options: (a) **IEEE basic operations, square root and spec 022's `sin_cos` only** (FR-3);
  (b) libm (different bits per library). — **recommended: (a)**; `tan` for the projection is `sin/cos` of the half-angle.
- [x] **Model format (D42).** Options: (a) **glTF 2.0 only (`.gltf` + `.bin`, `.glb`)**; (b) glTF and OBJ; (c) OBJ
  only. — **recommended: (a)**. Blender, Maya, 3ds Max, Blockbench and most tools export it, it carries materials and
  vertex colours, and one format means one parser to bound and fuzz. OBJ can follow if asked.
- [x] **glTF features: read, ignored, refused.** — **recommended: as FR-6** — skins, animations, morph targets, cameras,
  lights and PBR maps ignored with a note (a Blender export with an armature still builds as a static mesh); embedded
  images, unknown required extensions, sparse accessors and non-triangle primitives refused.
- [x] **Node transforms.** Options: (a) **baked into one mesh at conversion**; (b) kept as a node list the game draws
  with its own matrices. — **recommended: (a)** for this slice; (b) comes with 3D hierarchies or animation.
- [x] **Vertex format.** Options: (a) **quantised** (16-bit positions over the bounds, 8-bit normals, 16-bit texture
  coordinates, 8-bit colours; ≤ 20 bytes) decoded by the vertex shader; (b) floats (≈ 48 bytes). — **recommended: (a)**:
  less than half the linear memory and bandwidth, precision far below a pixel for models of normal size; the PICA
  reads these formats natively.
- [x] **Textures of models.** Options: (a) **separate PNGs in the assets folder, converted as ordinary textures and
  owned by the mesh**; (b) images embedded in `.glb` extracted by the converter. — **recommended: (a)**; the texture
  rules and conversion stay in one place. (b) is a follow-up if exporters make it painful.
- [x] **Material model and lighting.** Options: (a) **FR-10's fixed formula, per-pixel Blinn-Phong through the PICA's
  fragment lighting, shininess from a fixed set of levels**; (b) per-vertex (Gouraud) lighting in the vertex shader;
  (c) PBR approximations. — **recommended: (a)**. The PICA lights per pixel in fixed function at no shader cost, a
  shininess per lookup table keeps the tables precomputed, and the formula is simple enough to reproduce on the PC.
- [x] **Lights.** — **recommended: ambient + up to 4, directional or point with a range; spot lights later; visible
  defaults** (FR-11). The hardware has 8; 4 keeps the per-pixel cost and the test matrix small.
- [x] **Fog.** — **recommended: follow-up** (the PICA's fog table is cheap; it goes with "lighting and atmosphere").
- [x] **Screens.** Options: (a) **top screen only now, the API naming the screen**; (b) both screens. —
  **recommended: (a)**, as the slice's scope; the bottom screen needs no new design later.
- [x] **Stereo (and how strong).** — **recommended: slider-driven off-axis stereo, eye separation = slider × the
  camera's stereo strength, a focal distance at screen depth, the right eye drawn only when the slider is above 0, 2D at
  screen depth in both eyes, orthographic mono** (FR-14, FR-15). The game can turn stereo off. Defaults chosen for
  comfort, fixed by the plan after a look on the console.
- [x] **The slider in the simulator and scripts.** — **recommended: a portable query, simulator keys + an environment
  variable, a `.nnhs` command that overrides the value in Debug/Development on both systems; an eye-view key with an
  anaglyph preview in the simulator** (FR-16, FR-22).
- [x] **The 3D pass among 2D layers (D43).** Options: (a) **a world layer per screen (default 8): 2D layers below it
  before the 3D, the rest after**; (b) 3D always under all 2D; (c) 3D always over 2D layers. — **recommended: (a)**, the
  split rendering.md promised; skies and HUDs both work without new 2D calls.
- [x] **Order and transparency.** — **recommended: opaque and cut-out in call order with depth write; blended sorted
  back to front by bounds centre with ties in call order, no depth write; no per-triangle sorting** (FR-19), as Godot and
  Unity.
- [x] **Culling.** — **recommended: CPU frustum culling per draw by bounds (union of both eyes) and per-material
  back-face culling** (FR-20). No occlusion culling.
- [x] **Opt-in mechanism (D43).** — **recommended: a 3D draw capacity in `AppConfig`, default 0** (FR-26): no VRAM,
  targets or shader for 2D games, and no 3D code linked (FR-31).
- [x] **Meshes in code and primitives.** — **recommended: yes, cube, plane and UV sphere** (FR-5): tests, prototypes and
  the example without assets, and constitution XI.
- [x] **3D hierarchy.** — **recommended: not in this spec**; the game composes matrices (FR-1). 3D nodes extend
  spec 022 later.
- [x] **How the console proves its pixels.** Options: (a) **probe pixels read back in the game's check mode and compared
  with the formula within a tolerance, logged PASS/FAIL; whole screenshots compared by reading them**; (b) a tolerant
  image comparison command in `nnh` (`compare a.png b.png`); (c) only screenshots read by a person. —
  **recommended: (a)**, the game shows its own verdict as the console tests always do; (b) is a follow-up for the next
  3D specs if probes are not enough.
- [x] **Events.** — **recommended: none** (FR-30).
- [x] **Example.** — **recommended: a new `examples/models` with showcase, check and bench modes** (AC-6); the name
  avoids a target starting with a digit (`3d`). The platformer is untouched.
- [x] **Old 3DS cost targets.** — **recommended: AC-8's targets, confirmed by the plan, set from the first console
  measurement with margin if still missed after optimisation** (the maintainer's bench policy).

## Open questions

- [ ] The exact quantisation layout and error (≤ 20 bytes per vertex proposed) and whether 16-bit positions over the
      bounds are enough for large levels drawn as one mesh — the plan measures it on the example's models; large levels
      can be split into submeshes or meshes.
- [ ] FR-24's tolerance (edge pixels, colour difference per channel) — proposed by the plan from the PICA's lighting
      table precision, confirmed by the first console run of AC-7.
- [ ] The number of shininess levels (8 proposed) and whether the point light's falloff curve fits one shared distance
      table — the plan decides from the PICA's table limits.
- [ ] The stereo defaults (strength, focal distance) — set after viewing the example on the Old 3DS XL.
- [ ] Whether the right eye is captured by the remote screenshot as a third PNG (`-top-right.png`) — the plan checks
      the console read-back path (spec 013) and extends it if needed.
- [ ] The simulator's slider keys and the eye-view key — the plan picks keys free in spec 006's map.

## Follow-up specs (proposed ROADMAP split of M8)

1. **3D on the bottom screen** and two 3D views (a map, a model viewer on the touch screen).
2. **Fog, spot lights and lighting tables** (toon shading, rim light), more than 4 lights if the cost allows.
3. **3D nodes and hierarchies** (spec 022's ideas in 3D: local/world transforms, a pass, glTF node lists kept).
4. **Skinned animation** (glTF skins and animation clips, bones in the vertex shader, blending).
5. **3D collision and queries** (rays and boxes against meshes and simple shapes, picking).
6. **Billboards and particles in 3D** (camera-facing sprites, spec 024's emitters in 3D space).
7. **Stereo depth for 2D layers** (2D games gaining depth from the slider, spec 004's FR-13).
8. **Mipmaps and texture formats** (ETC1, mip chains from the asset step).
9. **Shadows** (blob shadows first, then the PICA's shadow maps).
10. **A GPU path in the simulator** (SDL GPU) if scenes outgrow the CPU rasteriser.
