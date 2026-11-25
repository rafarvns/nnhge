# gfx3d — 3D drawing on the top screen

Header: `<nnh/gfx3d.hpp>` · Spec: [026](../../specs/026-3d-foundation/spec.md) ·
Plan: [026](../../specs/026-3d-foundation/plan.md) · Decisions: [D41, D43, D44](../architecture/decisions.md) ·
Meshes and materials: [mesh.md](mesh.md) · Math: [math3d.md](math3d.md) · Rendering: [rendering.md](../architecture/rendering.md) ·
Example: `examples/models`

> Status: 026a (mono) implemented and checked on the Old 3DS; 026b (stereo: the 3D slider, per-eye drawing)
> implemented on the PC, console run pending ([plan 026](../../specs/026-3d-foundation/plan.md), "Stage 11/12 results").

A game sets a **3D camera** and **lights** per screen and records **draws of meshes** inside a frame, like 2D. At the
end of the frame the engine culls them, orders them and draws them **between the 2D layers**: layers below the
screen's **world layer** (default 8) first, then the 3D world, then the layers from it up, then the debug pass.

```cpp
nnh::init({.gfx = {.max_draws_3d = 256}});            // 3D is off by default
auto ship = nnh::load_mesh("models/ship");
while (nnh::frame()) {
    nnh::Camera3D cam;
    cam.position = {0, 2, 6};
    nnh::gfx3d::point_at(cam, {0, 0, 0});
    nnh::gfx3d::set_camera(nnh::Screen::Top, cam);
    nnh::gfx3d::draw(nnh::Screen::Top, ship.value(), nnh::Transform3D{.rotation = nnh::quat_axis_angle({0, 1, 0}, t)});
    nnh::gfx::draw_text(nnh::Screen::Top, {4, 4}, "HUD", {.layer = 9}); // above the world
}
```

## Turning 3D on

`AppConfig::gfx.max_draws_3d` (default **0 = off**, up to 4096) reserves, once at `init()`: about **120 bytes per
draw** (heap, tag `gfx3d`), the mesh table (4 KB), and on the 3DS 640 bytes of GPU command buffer per draw plus the top
screen's 24-bit depth target and the right eye's colour and depth target (VRAM, 768 000 B each). With 0 nothing is reserved, meshes can't be made and `draw` asserts. A game that
never calls the 3D functions links none of their code.

## Camera

`Camera3D` is a value: `position`, `rotation` (looks down its −z), `projection` (`Perspective` with `fov_y` in radians,
default 60°, or `Orthographic` with `ortho_height` world units visible), `near_plane` (0.1), `far_plane` (100),
`stereo` (see [Stereo](#stereo)). The aspect is the top screen's 400:240.

| Function | Does |
|----------|------|
| `point_at(camera, target, up)` | turns the camera to look at `target` |
| `set_camera(screen, camera)` / `camera(screen)` | the camera used when the frame ends; an invalid one asserts and is ignored |
| `to_screen(screen, point)` | where a world point lands: pixel ((0, 0) top-left), depth along the view axis, `in_front`, `in_view` |
| `screen_ray(screen, pixel)` | the world ray under a pixel (origin on the camera, unit direction) |
| `to_screen(camera, point)`, `screen_ray(camera, pixel)` | the same for any camera (pure: no engine needed) |
| `to_screen(camera, point, eye, slider)` | where the point lands in one eye's picture at a slider value (stereo) |
| `in_view(camera, sphere, slider)` | the engine's culling test: is a world sphere in the view (either eye in stereo)? |

`to_screen` and `screen_ray` are pure math: the same bits on PC and 3DS (a name tag over a character, picking later).

## Stereo

On a system with `Capability::Stereo3D` (a 3DS other than a 2DS, and the simulator), when the 3D slider
(`nnh::system::stereo_slider()`, [system.md](system.md#the-3d-slider)) is above 0 and the camera is perspective with
`stereo.enabled`, the top screen is drawn **once per eye**. Each eye sits half the eye separation to the side along the
camera's right — separation = slider × `stereo.strength` (world units; 0 = `focal_distance / 20`) — and sees through an
off-axis projection that puts the **focal plane** (`stereo.focal_distance` ahead, default 5) at screen depth: points on it
land on the same pixel in both eyes, nearer ones in front of the screen, farther ones behind. With the defaults and a
60° view, a point at infinity is about 10 px apart between the eyes at slider 1.

| `StereoSettings` | Default | Meaning |
|------------------|---------|---------|
| `enabled` | true | false: this camera always draws one picture |
| `strength` | 0 | eye separation at slider 1, world units; 0 = `focal_distance / 20` (≥ 0) |
| `focal_distance` | 5 | distance ahead that appears at screen depth (> 0) |

- **One picture** when the slider is at 0, on a 2DS, for orthographic cameras (no 3D depth for them), with
  `stereo.enabled = false`, and in frames without 3D draws: nothing extra is spent.
- **2D in stereo** (FR-15): the 2D layers below and above the world and the debug pass are drawn identically in both
  eyes, at screen depth. Tile and particle budgets are spent once per frame, not per eye.
- **Culling** keeps a draw when either eye sees it; blended draws are sorted once by the centre camera, so both eyes
  draw them in the same order. `stats().eyes` is 2 and `triangles[0]`/`[1]` count each eye.
- `to_screen(screen, point)` answers for the centre camera (the HUD sits at screen depth); `to_screen(camera, point,
  eye, slider)` gives one eye's pixel, and `in_view(camera, sphere, slider)` the culling decision — both pure, the same
  bits on PC and 3DS (the example's `3d-stereo` golden `B3550D40`).
- Invalid stereo values (a negative strength, a focal distance ≤ 0, not finite) make `set_camera` assert and keep the
  previous camera.
- **Comfort**: the defaults (focal 5, strength focal / 20) are provisional until the maintainer looks at the showcase on
  the Old 3DS XL (spec 026 T13.6). Keep what the player looks at near the focal distance.
- **Cost**: the 3DS draws the top screen's 2D and 3D twice (the per-draw GPU and submission cost doubles; recording and
  culling are done once). Stereo budgets come from the console (plan 026, T13.5).
- **The simulator**: F6/F7 move its slider, F8 shows the left eye, the right eye or a red/cyan anaglyph
  ([simulator.md](simulator.md#the-3d-slider-and-the-eyes)); `nnh run --script` sets the slider with `slider V`
  ([testing-on-the-console.md](../guides/testing-on-the-console.md)).

## Lights

`set_lighting(screen, lighting)`: an `ambient` colour and up to 4 `Light3D`s (`count`), each `Directional` (`vector`
points towards the light) or `Point` (`vector` is its position, falloff (1 − d/range)², zero at `range`). Lights are
given in world space. Default (nothing set): ambient 0.3 grey and one white directional light from the upper front
left. More than 4 asserts (clamped in Release).

## Drawing

`draw(screen, mesh, model, options)` with a `Mat4` (affine: bottom row 0, 0, 0, 1) or a `Transform3D`.
`DrawOptions3D`: `submesh` (−1 = all), `material` (replaces every drawn submesh's material; copied at the call),
`tint` (multiplies the base colour). Recording costs a few microseconds and allocates nothing; draws past the capacity
are dropped, counted and assert. Misuse — outside a frame, 3D off, the bottom screen (3D there is a follow-up), an empty
or destroyed mesh, a submesh out of range, a non-finite or non-affine matrix, an out-of-range material value — asserts
in Debug/Development and records nothing (or clamps) in Release.

## Materials and the shading formula

Each pixel: `surface = texture × vertex colour × base colour`; unlit: `surface`; lit:
`surface.rgb × min(ambient + Σ colourᵢ · max(N·Lᵢ, 0) · attᵢ, 1) + Σ colourᵢ · specular · (N·Hᵢ)^shininess · attᵢ`,
clamped, alpha from the surface; highlights only where N·L > 0. Blinn-Phong per pixel: the 3DS uses the PICA200's
fragment lighting (lookup tables) and texture combiners; the simulator computes the same formula with the same tables.

## Order, depth and transparency

Opaque and cut-out parts are drawn first, in call order, with depth test and write. Then the draws with blended
submeshes, **back to front** by the view depth of their bounding sphere's centre (ties in call order), depth tested
but not written. Triangles inside one blended draw are not sorted. The depth is cleared each frame; 2D drawn before
the world never hides it, 2D drawn after is never hidden by it.

**The world layer**: `set_world_layer(screen, 0..16)` (default 8). With 3D draws in a frame, 2D on layers below it
(text's default layer 0 included) is **under** the world: draw the HUD on layer 8 or above. A frame without 3D draws is
submitted exactly as before (byte-identical).

## Culling

Draws whose world bounding sphere is outside the view are skipped before any GPU work (`stats().culled`). Back faces
are culled per material; triangles crossing the near plane are clipped.

## Stats and debug

`stats()`: draws, culled, dropped, peak, triangles per eye, eyes, meshes and their bytes; the debug overlay shows a 3D
line. `set_debug_draw(true)` (Debug/Development) draws each draw's box and the world axes as 2D lines on layer 15.
`request_probes(screen, eye, points, n)` / `probe_results(out, n)` (Debug/Development): read back up to 32 pixels of the
screen (`Eye::Right`: the right eye's picture of a stereo frame), read three frame ends after the request, so keep drawing the same picture until `probe_results` answers — the
example's check compares them with the formula; it passes on the PC and on the Old 3DS (plan 026, "Console run 3").

## Determinism and tolerance

Everything a game reads (math, `to_screen`, `screen_ray`, culling, order, stats) gives the same values on PC and 3DS.
Pixels are each backend's: the simulator's CPU rasteriser (D41) gives the same PNG bytes on every PC (golden hashes),
the console's are close: probes at least 2 px from an edge within ±2 per channel (flat, nearest-textured), ±3
(blended), ±10 (lit) — proposed values, confirmed by the first console run.

## Costs

Old 3DS targets (AC-8, measured by `examples/models` bench mode, results in [plan 026](../../specs/026-3d-foundation/plan.md)):
12 000 textured lit triangles at 60 Hz, 256 draws with the CPU's share under 4 ms, 512 recorded and culled draws under
1 ms, three full-screen blended planes at 60 Hz; in stereo 6 000 triangles at 60 Hz and 256 draws with the CPU's share
under 7 ms (the bench's `stereo` cases log "not judged" until the console sets their budgets). The simulator doesn't
model the PICA200's speed.

Console run 2 (Old 3DS XL, 2026-11-25, after the optimisation round): 12 000 triangles and the fill case hold 60 Hz;
each small draw costs about 110 µs of frame time — recording ≈ 12 µs, submission ≈ 34 µs, the GPU ≈ 63 µs, which run
one after the other within a frame — so 256 separate draws take ≈ 28 ms. Running the CPU beside the GPU was tried and
dropped (the Old 3DS's CPU slows down beside it; D47). Budgets (measurement + ≈ 25 %): 256 draws at a frame average
under 35.5 ms with the CPU's share under 14.8 ms; 512 recorded and culled draws under 8.8 ms of CPU. Draw fewer, larger meshes where you
can; consecutive draws with the same mesh and material are the cheapest (the vertex buffer, texture and colour aren't
sent again).
