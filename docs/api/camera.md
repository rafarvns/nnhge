# camera — camera helpers: follow, dead zone, limits, shake, zoom, two screens

Header: `<nnh/camera.hpp>` · Spec: [018](../../specs/018-camera-helpers/spec.md) ·
Plan: [018](../../specs/018-camera-helpers/plan.md) · Example: [`examples/camera`](../../examples/camera/main.cpp) ·
In a game: [`examples/platformer`](../../examples/platformer/level_play.cpp) · Zoom in the renderer:
[gfx.md](gfx.md#zoom)

A `CameraRig` is the camera that makes scrolling feel good: it follows a point or a box with an optional **dead
zone** and **smoothing**, never shows past the level's **limits**, **shakes** from trauma and settles back exactly,
**zooms** about the view's centre, and drives one screen, both screens as **one tall world**, or — with two rigs —
**two views**. It is a plain value the game keeps (one per view), advances once per frame and writes into the
screens' `Camera2D`. Every update is one 60 Hz frame computed with plain float operations in a fixed order, so the
same calls give the **same bits on PC and 3DS** (replays and console checks compare exactly). Nothing allocates.

The module is optional (constitution XI): a game that doesn't include the header links none of it, and setting
`Camera2D` by hand with `gfx::set_camera` stays fully supported, zoom included.

```cpp
class Level final : public nnh::Scene {
    void on_enter() override {
        rig_.set_limits(map_.bounds());
        rig_.snap(player_.box());     // level start: no glide from (0, 0)
        rig_.apply();
    }
    void update() override {
        player_.update();             // move the target first...
        rig_.update(player_.box());   // ...then the camera: it shows this frame's position
        rig_.apply();
    }
    void draw() override {
        nnh::gfx::draw_tilemap(nnh::Screen::Top, map_);
        rig_.debug_draw();            // dead zone, limits, target (Debug/Development only)
    }
    nnh::CameraRig rig_{{.dead_zone = nnh::CameraSettings::centered_zone(48, 32), .smoothing = {1, 0.15f}}};
    nnh::Tilemap map_;
    Player player_;
};
```

## The rig and its settings

`CameraRig rig{settings}` or `CameraRig rig;` then `rig.set(settings)`. A rig is 128 bytes, trivially copyable (a
copy is an independent rig), needs no `nnh::init()` for its computations (only `apply()` and `debug_draw()` talk
to the engine) and has no scene binding: the game calls `update()` where it wants — normally in its scene's
`update()` after moving the target. Not calling `update()` freezes the camera (a pause menu over the level freezes
it with no extra code). To reset a rig completely (no trauma, the shake's noise clock at 0), assign a fresh one:
`rig = nnh::CameraRig(settings);`.

`CameraSettings` (designated initializers, every field optional):

| Field | Default | Meaning |
|-------|---------|---------|
| `screen` | `Top` | the screen a single view drives |
| `layout` | `Single` | `Single` or `TallWorld` ([screens](#the-two-screens)) |
| `small_world` | `Center` | where the view sits on an axis where the limits are smaller than the view: `Start`, `Center`, `End` |
| `follow_x`, `follow_y` | true | false: that axis keeps its position (a horizontal shooter locks y) |
| `has_limits` | false | set by `set_limits()` |
| `shake_inside_limits` | true | false: the shake may show up to `max_shake` past the limits |
| `pixel_snap` | false | the applied camera in whole screen pixels (pixel art that must not shimmer) |
| `offset` | (0, 0) | world pixels added to the target's centre (a look-ahead) |
| `dead_zone` | empty | screen pixels around the view's centre; `CameraSettings::centered_zone(w, h)` |
| `smoothing` | (1, 1) | fraction of the remaining distance per frame, per axis, (0, 1]; 1 = instant |
| `zoom` | 1 | 0.5–4, about the view's centre |
| `limits` | — | world rectangle the view stays inside, e.g. `map.bounds()` |
| `gap` | 0 | tall world: world pixels hidden between the screens |
| `max_shake` | (8, 6) | screen pixels of shake at trauma 1 |
| `trauma_decay` | 1/60 | trauma lost per frame (full trauma lasts one second) |
| `shake_speed` | 0.5 | noise cells per frame, (0, 1] |
| `seed` | `'NNHC'` | the shake's noise seed: same seed, same shake |

Setters for what games change at run time: `set_offset(v)`, `set_zoom(z)` (keeps the view's centre),
`set_limits(rect)`, `clear_limits()`, `set_layout(layout, screen)` (keeps the primary screen's top-left; the next
update re-centres). `set()` replaces everything at once (a new zoom keeps the centre too).

## Following

`update(Vec2 point)` or `update(Rect box)` (followed by its centre `x + w / 2`). The **desired** view centre is the
target plus `offset`. Turn an axis off with `follow_x`/`follow_y`; a platformer that should only move vertically on
the ground passes the last standing height as the target's y.

- **Dead zone** (Godot's drag margins): a rectangle around the view's centre, in **screen pixels** (it looks the same
  at any zoom). Inside it the target moves without moving the camera; leaving it, the camera moves just enough to
  bring the target back to the zone's edge. Empty (the default): the target is always centred. A zone of width 0 at
  `x ≠ 0` holds the target at that point. A zone larger than the view is clamped to it (one warning per rig in
  Debug/Development).
- **Smoothing**: each update the camera covers a fixed fraction of the remaining distance per axis; closer than
  `kCameraSnapDistance` (1/64 pixel) it lands exactly on the desired position, so a still target leaves the camera
  still, bit for bit. Fraction 1 is an assignment. Per frame, never per second: it can't depend on the frame rate.
- **`snap(target)`**: the target centred (plus the offset) at once, inside the limits, no smoothing, dead zone
  ignored, both axes placed (also one that isn't followed: that is how a locked axis gets its position). Call it at
  level start, respawn and teleport; a rig that was never snapped moves from (0, 0). It keeps the shake
  (`stop_shake()` clears it).

## Limits

`set_limits(rect)` keeps the view inside a world rectangle (Godot's `limit_*`, a rectangular Cinemachine confiner);
`map.bounds()` gives a tilemap's (`{0, 0, width × tile_width, height × tile_height}`, [tilemap.md](tilemap.md)).
The desired position is clamped **before** smoothing (the camera never eases into a wall and bounces back), the
shaken one again after. On an axis where the limits are smaller than the view, `small_world` places the view.
Changing the limits at run time (rooms): with smoothing on the camera glides into the new limits; for a cut, call
`snap()` after `set_limits()`.

## Trauma shake

`add_trauma(0.4f)` adds trauma, clamped to [0, 1]; it decays by `trauma_decay` per frame to exactly 0. Each frame the
shake is `max_shake / zoom × trauma² × noise` per axis (Eiserloh's trauma model), where `noise` is the engine's own
seeded smooth value noise (an integer hash and a smoothstep, no `<cmath>`), moving `shake_speed` noise cells per
frame. The shake is added on top of the followed position, which it never moves: once trauma is 0, `position() ==
followed()` exactly. By default the shaken position stays inside the limits (at an edge the shake is one-sided);
`shake_inside_limits = false` lets it pass them. `stop_shake()` sets trauma to 0 at once.

Recipes:

```cpp
rig.add_trauma(0.3f); // a hit
rig.add_trauma(1);    // an explosion: one second of shake

// "Shake for N frames": full trauma that decays in N frames.
nnh::CameraSettings s = rig.settings();
s.trauma_decay = 1.0f / 20;
rig.set(s);
rig.add_trauma(1); // 20 frames
```

The shake grows with trauma²; for a steeper feel, add trauma on your own curve (e.g. `add_trauma(hit * hit)`).

The noise clock keeps running across `stop_shake()` and `snap()`; a replay that must start identically assigns a
fresh rig (as the platformer's `restart()` does).

## Zoom

`zoom` (0.5–4) is written into `Camera2D::zoom` by `apply()`; the renderer scales sprites, text, shapes and tile
layers ([gfx.md](gfx.md#zoom)). The rig zooms **about the view's centre**: `set_zoom()` moves the position so the
centre stays put. The view covers `screen size / zoom` world pixels; the dead zone stays in screen pixels; limits
and conversions use the zoomed view. Animating the zoom is the game's choice: a [tween](tween.md) through a setter
calling `set_zoom`, or a new value each frame.

## The two screens

| Mode | How | Layout |
|------|-----|--------|
| **Single view** | one rig, `layout = Single`, `screen` = Top or Bottom | the other screen's camera is untouched (HUD, menus) |
| **One tall world** | one rig, `layout = TallWorld` | the bottom screen continues the top one |
| **Two views** | two rigs, each `Single` on its own screen | game above, map or a second target below |

The tall world treats both screens as one view 400 wide and `480 / zoom + gap` tall:

```
world  ┌───────────── 400 / zoom ─────────────┐  ← position() (top-left)
       │              top screen              │  240 / zoom
       └──────────────────────────────────────┘
          ┄┄┄┄┄┄┄┄┄┄┄┄ gap (world pixels) ┄┄┄┄┄┄┄┄  hidden between the physical screens
           ┌──────── 320 / zoom ────────┐         ← camera(Bottom) = position() + (40 / zoom, 240 / zoom + gap)
           │       bottom screen        │  240 / zoom
           └────────────────────────────┘
       ← 40 / zoom →  (centred: (400 − 320) / 2 screen pixels each side)
```

Following, the dead zone and the limits use the combined view; both screens share the zoom and the shake; a sprite
crossing from one screen to the other moves continuously (it appears on the bottom screen exactly where it would if
the screens touched, plus the gap). `drives(screen)` tells which screens a rig writes.

## Queries, conversions, applying

| Call | Returns |
|------|---------|
| `followed()` | the followed position (no shake), top-left of the primary screen in world pixels |
| `position()` | with the shake; never pixel-snapped |
| `trauma()` | current trauma |
| `aim()` | the last target plus the offset (what the dead zone holds) |
| `camera(screen)` | the `Camera2D` `apply()` writes for that screen (tall world: the bottom offset; `pixel_snap` applied) |
| `view(screen)` | the world rectangle that screen shows |
| `to_world(screen, p)` | the world point under screen point `p`: `camera(screen).position + p / zoom` (a touch → world) |
| `to_screen(screen, w)` | `(w − camera(screen).position) × zoom` (a world point → a marker on screen) |

Conversions match what is drawn: a sprite drawn at `to_world(screen, p)` appears under `p` (within the renderer's
whole-pixel rounding). Asking about a screen the rig doesn't drive asserts and answers for its primary screen.

`apply()` calls `gfx::set_camera` on the driven screen(s) — the module's only renderer call. Applying is separate
from updating, so a game can read and adjust the result first, or use `position()` with its own drawing.
`pixel_snap` rounds the applied camera to whole screen pixels at the current zoom; the reported positions stay
exact.

## Debug drawing

`debug_draw()` (inside a frame) draws on the driven screen(s), layer 15, ignoring the camera: the dead zone in
**yellow** (around the drawn view's centre), the limits in **red** (when visible), the target as a **green** cross;
at most four shape draws per screen, counted in `max_draws`. In Release it is an empty inline function: no code, no
strings.

## Determinism and cost

`update()` advances exactly one frame: dead zone, smoothing, trauma decay and the noise clock move by one step,
never by measured time. The order is fixed (target → desired with the dead zone → clamp → smoothing → trauma decay →
shake → final clamp) and uses only `+ − × ÷`, comparisons and float↔int conversions, compiled with
`-ffp-contract=off`; the same settings and calls give the same positions, bit for bit, on every supported compiler
and on the 3DS. `examples/camera`'s self-check plays a 600-update script and compares a checksum of every position
with the simulator's (`nnh run camera --script examples/camera/tests/check.nnhs`). Subnormal inputs (positions
below 1e-38) are the only values whose bits could differ (libctru flushes them to zero).

An update is a few dozen float operations (no loop over the world, no allocation, no `virtual`, no logging on the
normal path) inside the profile scope `camera`. Measured: PC simulator (Debug) 0.0013 ms per pair of rigs (a
tall-world rig and a bottom-screen rig, each updated and applied); Old 3DS: see
[plan 018, stage 9](../../specs/018-camera-helpers/plan.md) (target < 0.05 ms per pair).

## Misuse

Zoom outside 0.5–4 or NaN, a negative or non-finite dead zone, smoothing outside [1/1024, 1], `trauma_decay` ≤ 0,
`shake_speed` outside (0, 1], a negative gap, non-finite offsets, limits or targets, negative limit sizes, a negative
or non-finite `max_shake`, NaN trauma: **assert** in Debug/Development. Release clamps the value (or keeps the
previous one for NaN; a bad target skips that update) and logs one error per rig on the channel `camera`.
