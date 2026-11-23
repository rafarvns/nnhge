# Plan 018: Camera helpers

Status: implemented (2026-11-22; T8.1 passed on VM 120, T9.2 on the Old 3DS XL); approved 2026-11-22
(maintainer pre-approved)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D33 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 002 (asserts, logs, profiler), spec 004 (`Camera2D`, the recorded draw list), spec 005 (text as sprites),
spec 012 (tile batches, D26; the platformer and its gate), spec 013 (scripts), spec 015 (the determinism rules of the
easings: libm-free, `-ffp-contract=off`)

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ⚠️→✅ | Pressure point: zoom (FR-9) touches the tile batch contract of D26. Answered by D33: core applies the zoom when it builds a pass, so sprites, shapes and text need no platform change; tile batches gain one destination tile size per batch and two orientation bits per quad (the quad stays 10 bytes). The camera helper is portable core code with no platform part |
| II 3DS constraint | ⚠️→✅ | Pressure point: zoom 0.5 shows four times the tiles. Bounded by `AppConfig::gfx.max_tiles` on every system (D26, unchanged); batches larger than the shared 1600-quad scratch are expanded in row chunks instead of growing the scratch. The rig is 128 bytes, no heap; an update is ≈ 100 float operations (target < 0.05 ms for two rigs, AC-7) |
| III Build speed | ✅ | `<nnh/camera.hpp>` includes `<cstdint>`, `nnh/build.hpp` and `nnh/gfx.hpp` (already light: `<cstddef>`, `<cstdint>`, `nnh/math.hpp`). Measured before/after (T0.1, T10.4) |
| IV Event-driven | ✅ | No events: the camera is a value the game updates and queries (spec non-goal); `Camera2D` stays a command (`gfx::set_camera`) |
| V No hidden costs | ✅ | No exceptions/RTTI/`virtual`, no allocation anywhere (not even at creation); zoom adds a few multiplies per draw item and nothing at zoom 1 on the tile path (the zoom-1 branch is today's loop) |
| VI Spec + test first | ✅ | The rig is pure logic tested on plain values; zoom is tested on the fake platform's recorded items and quads, with a zoom-1 regression test written and pinned **before** the renderer changes (AC-3); PC pixels by the conformance test and screenshots; the console by a script |
| VII Small files | ✅ | Nine camera files of one concept each (all planned ≤ 200 lines), one new tilemap file (zoom edges); one public header |
| VIII Documented API | ✅ | `docs/api/camera.md`, `gfx.md`, `tilemap.md`, `rendering.md`, header comments, `examples/camera`, the platformer |
| IX Clean and legal | ✅ | Own code from published ideas (Eiserloh's trauma, value noise, Godot drag margins); cited in `docs/REFERENCES.md`; original art made by a script |
| X Safe by default | ✅ | No external data is read. Misuse (NaN, ranges) asserts in Debug/Development and is clamped or ignored with one error log in Release (FR-15) |
| XI Opt-in, never in the way | ✅ | Optional module; `Camera2D` by hand keeps working and can zoom; camera and tilemap don't include each other (limits are a plain `Rect`, FR-7); `hello` links no camera code (checked with `nm`) |

## Design

### Overview

```
game scene update()                                   engine
  player.move(...)
  rig.update(player.box())   ── pure, one 60 Hz frame: target → desired (offset, axis locks, dead zone)
                                → clamp to limits → smoothing → trauma decay → noise shake → final clamp
  rig.apply()                ── gfx::set_camera(screen(s), {position, zoom})      (Camera2D = what is drawn)
game scene draw()
  gfx::draw / draw_tilemap   ── recorded as today
  rig.debug_draw()           ── Debug/Development: dead zone, limits, target (gfx shapes, layer 15)
end of frame (gfx_submit) per screen:
  DrawList::build(screen, Camera2D{position, zoom})   ── (p − position) × zoom, sizes and pivots × zoom,
                                                          snapped as today; ignore_camera items unscaled
  tile batch → expand(request{camera, zoom, first_row})  ── zoom 1: today's loop; else per-tile edges snapped,
                                                             quads with dst size + widen/taller bits, row chunks
  platform::draw_2d / draw_tiles(batch{dst_w, dst_h})
  debug pass (unchanged, never zoomed)
```

### Decision D33: zoom in `Camera2D`, applied by core; tile batches carry a destination size (FR-9)

**`Camera2D` gains `float zoom = 1`.** A camera-relative point lands at `screen = (world − position) × zoom`.
`position` keeps its meaning at every zoom — the world point at the screen's top-left corner — so every existing
caller and every value read back stays valid, and a zoom of 1 is the identity (IEEE `x × 1 = x` exactly, NaN and
infinities included). Zooming "about the centre" is the helper's job (it moves `position` when the zoom changes,
FR-9's last rule), not the renderer's. `gfx::set_camera` clamps the zoom to [0.5, 4] (asserting in Debug/Development,
FR-9) and turns NaN into 1; Release clamps silently, so `hello` gains no string.

**Sprites, shapes and text: no contract change.** `platform::DrawItem2D` already carries the destination position,
size and pivot in screen pixels. `DrawList::build` multiplies, for items that don't ignore the camera:

| Item | Today | With zoom `z` |
|------|-------|---------------|
| sprite | `x = snap(p.x − c.x)`, `w = region.w × |scale.x|`, `pivot_x = pivot.x × |scale.x|` | `x = snap((p.x − c.x) × z)`, `w = region.w × |scale.x| × z`, `pivot_x = pivot.x × |scale.x| × z` |
| fill / outline rect | `x = snap(a)`, `w = snap(a + size) − x` with `a = p − c` | `x = snap(a × z)`, `w = snap((a + size) × z) − x` |
| line | `(snap(a), snap(end − c))` | `(snap(a × z), snap((end − c) × z))` |
| text glyph | a sprite (scale = text scale) | a sprite: glyphs scale like sprites |

Outlines and lines stay **one screen pixel** thick (the platforms draw them so; a zoomed thickness would need a
contract change for no game need). `ignore_camera` items use `z = 1`, so the HUD is unchanged; the debug pass
(debug text, overlay) never goes through `build` and is never zoomed. Every multiplication by 1 is exact, so zoom 1
gives the same bits as today — proven by AC-3's regression test pinned before the change.

**Tile batches: the amendment to D26.** A quad was "a `tile_w × tile_h` square drawn unscaled at a whole-pixel
position". With zoom `z`, tiles must stay seamless at any `z` in [0.5, 4] (FR-9), so each tile's **edges** are snapped,
not each tile's size: for a layer whose snapped screen origin is `O` (computed once per batch, as today),

```
edge(i) = O + round_half_up(i × tile × z)        (per axis, i = cell index from the map's cell 0)
tile i covers [edge(i), edge(i + 1))             → no gap, no overlap, by construction
```

`edge(i + 1) − edge(i)` is `floor(tile × z)` or one more. So the contract gains:

```cpp
// nnh/platform/video.hpp
inline constexpr std::uint8_t kTileOrientWiden = 8;   ///< this quad is one pixel wider than dst_w
inline constexpr std::uint8_t kTileOrientTaller = 16; ///< this quad is one pixel taller than dst_h
struct TileQuad2D { /* unchanged: x, y, src_x, src_y, orient — 10 bytes */ };
struct TileBatch2D {
    // ... unchanged fields ...
    int dst_w = 0; ///< destination size of a tile in screen pixels: floor(tile_w × zoom); tile_w at zoom 1
    int dst_h = 0;
};
```

- **Zoom 1:** `dst_w = tile_w`, `dst_h = tile_h`, no new bit is ever set, the quads are today's bytes (the zoom-1
  branch of the expander *is* today's loop), so platforms make exactly today's calls.
- **Integer zooms** (2, 3, 4): every edge step is exactly `tile × z`, no widen bit: each texel is an exact square.
  Fractional zooms give uneven texel sizes, as any pixel-art renderer (documented).
- **Alignment with sprites** (FR-9): a sprite at world `X` lands at `snap((X − c) × z)`; the tile edge at the same
  world point is `snap((L − c) × z) + round(i × tile × z)`. They are equal for integer zooms and within one pixel for
  fractional ones ("within the renderer's whole-pixel rounding", FR-11). For parallax 1 and zoom 1 they are equal, as
  today.
- **Parallax with zoom:** Tiled's centre-based parallax uses the *view's* centre, which at zoom `z` is
  `camera + screen_size / z / 2`. A layer's world origin `L = offset + (camera + screen_size/z/2 − parallax_origin) ×
  (1 − parallax)`, its screen origin `O = snap((L − camera) × z)`. At `z = 1` this is today's formula bit for bit.
  A layer with parallax 0 (fixed backdrop) is therefore zoomed about the view's centre too (documented).
- **Visible cells** follow the zoomed view: exactly the cells whose `[edge(i), edge(i + 1))` meets the screen.
- **Row chunks.** The shared scratch holds 1600 quads, which is a full screen of 8×8 tiles at zoom 1 (51 × 31) but
  not at zoom < 1 (8×8 tiles at zoom 0.5: 101 × 61 = 6161). Instead of a 4× scratch (+48 KB for every tilemap
  game), the expander stops at the end of the last row that fits and reports the next row; `gfx` sends the chunk
  (`draw_tiles`) and asks again. Order is preserved (rows top to bottom, as today), and the `max_tiles` budget still
  drops tiles exactly as today when it runs out. At zoom ≥ 1 a batch never needs a second chunk, so the call
  sequence is unchanged.

Platforms:

- **3DS (citro2d):** each quad is still one `C2D_DrawImage` with angle 0 and no pivot; `params.pos.w/h` become
  `±(dst_w + widen)`, `±(dst_h + taller)` instead of `±tile_w`, `±tile_h`. Texture coordinates are unchanged (the
  source is still the `tile_w × tile_h` square; the diagonal flip transposes texcoords, which works for a non-square
  destination). Sampling stays `GPU_NEAREST`, so integer zooms replicate texels exactly.
- **PC (SDL3):** `dest = {x, y, dst_w + widen, dst_h + taller}`, source unchanged; the fast path tests
  `(orient & 7) == 0` instead of `orient == 0`. The diagonal path rotates the copy 90° about the destination's
  centre, so for `w ≠ h` (a widen bit without a taller bit, or the reverse) it passes the pre-swapped rectangle
  `{x + (w − h)/2, y + (h − w)/2, h, w}`, whose rotated footprint is exactly `{x, y, w, h}` (equal to `dest` when
  `w = h`, so zoom 1 is unchanged). The screens are native-size render targets (nearest), so the simulator's window
  scale doesn't interact with the zoom.
- **Fake:** nothing new: it records whole `TileBatch2D` values (now with `dst_w/dst_h`) and every quad (with its
  orientation bits), and `DrawItem2D`s with their zoomed positions and sizes. Tests read
  `platform_fake::state().last_frame_tiles` and `last_frame_items`.

Rejected:

- **GPU view transforms** (`C2D_ViewScale` on 3DS, `SDL_SetRenderScale` on PC): fractional zooms put tile edges at
  sub-pixel positions (seams), each GPU rounds differently (PC and 3DS would disagree), the fake platform couldn't
  see the zoom, and the debug pass would need the transform reset around it.
- **A 16-bit width/height per quad**: 10 → 14 bytes per quad (+40 % scratch, breaks the `≤ 12` assert) for the one
  bit of information per axis the edge rule needs.
- **A batch-level float scale, rounded by each platform**: the seam rule would be written once per platform and the
  PC and 3DS could round differently.
- **Zoom only inside the helper** (positions scaled, sprites not): the spec's rejected option (c).
- **A 4× scratch** instead of row chunks: +48 KB of heap for every tilemap game, used only at zoom < 1.

### Public API (sketch)

```cpp
// <nnh/gfx.hpp> (changed)
inline constexpr float kMinCameraZoom = 0.5f;
inline constexpr float kMaxCameraZoom = 4.0f;
/// The view of a screen. `position` is the world point at the screen's top-left corner at every zoom; a
/// camera-relative draw lands at (world − position) × zoom. ignore_camera draws and debug text are never zoomed.
struct Camera2D {
    Vec2 position{};
    float zoom = 1;   ///< kMinCameraZoom..kMaxCameraZoom; set_camera clamps (asserts in Debug/Development)
};

// <nnh/tilemap.hpp> (changed)
class Tilemap {
    // ...
    /// {0, 0, width() × tile_width(), height() × tile_height()} in world pixels; {} for an empty map.
    /// For CameraRig::set_limits (spec 018 FR-7). Cheap.
    Rect bounds() const;
};
```

```cpp
// <nnh/camera.hpp> (new): includes <cstdint>, "nnh/build.hpp", "nnh/gfx.hpp" (Screen, Camera2D; math types)
namespace nnh {

class CameraRig;
namespace detail { void camera_debug_draw(const CameraRig& rig); } // camera_debug.cpp

/// What a rig drives (FR-10). "Two views" = two rigs, each Single on its own screen.
enum class CameraLayout : std::uint8_t {
    Single,    ///< one screen (CameraSettings::screen); the other screen's camera untouched
    TallWorld, ///< both screens as one tall world: bottom under top, centred (40 px inset), `gap` apart
};

/// Where the view sits on an axis where the limits are smaller than the view (FR-6).
enum class CameraAlign : std::uint8_t { Start, Center, End };

inline constexpr float kCameraSnapDistance = 1.0f / 64; ///< FR-4: closer than this (world px) → exactly there
inline constexpr int kTallWorldInset = 40;              ///< (400 − 320) / 2 screen pixels

/// Every setting, with designated initializers. Validated by CameraRig::set (FR-15).
struct CameraSettings {
    Screen screen = Screen::Top;                   ///< Single: the screen it drives
    CameraLayout layout = CameraLayout::Single;
    CameraAlign small_world = CameraAlign::Center; ///< FR-6
    bool follow_x = true, follow_y = true;         ///< FR-2: off = that axis keeps its position
    bool has_limits = false;                       ///< FR-6
    bool shake_inside_limits = true;               ///< FR-8: false = the shake may pass the limits
    bool pixel_snap = false;                       ///< FR-13: applied position in whole screen pixels
    Vec2 offset{};                                 ///< FR-2: world px added to the target's centre
    Rect dead_zone{};                              ///< FR-3: screen px relative to the view's centre; empty = centred
    Vec2 smoothing{1, 1};                          ///< FR-4: fraction per frame per axis, (0, 1]; 1 = instant
    float zoom = 1;                                ///< FR-9: [0.5, 4]
    Rect limits{};                                 ///< FR-6: world px (e.g. map.bounds())
    float gap = 0;                                 ///< FR-10 TallWorld: world px hidden between the screens, ≥ 0
    Vec2 max_shake{8, 6};                          ///< FR-8: screen px at trauma 1
    float trauma_decay = 1.0f / 60;                ///< FR-8: per frame, > 0 (full trauma lasts 1 s)
    float shake_speed = 0.5f;                      ///< FR-8: noise cells per frame, (0, 1]
    std::uint32_t seed = 0x4E4E4843;               ///< FR-8: 'NNHC'
    /// A w × h dead zone centred on the view: `.dead_zone = CameraSettings::centered_zone(64, 48)`.
    static constexpr Rect centered_zone(float w, float h) { return {-w / 2, -h / 2, w, h}; }
};

/// A camera helper: a plain value the game keeps per view (FR-1). 128 bytes, no heap, no init(), copyable.
/// Call update() once per frame after moving the target, then apply() (or read position()).
class CameraRig {
public:
    CameraRig() = default;
    explicit CameraRig(const CameraSettings& settings);

    void set(const CameraSettings& settings);   ///< any time; a zoom change keeps the view's centre
    const CameraSettings& settings() const;
    void set_offset(Vec2 offset);
    void set_zoom(float zoom);                  ///< keeps the view's centre (FR-9)
    void set_limits(Rect limits);               ///< has_limits = true
    void clear_limits();
    void set_layout(CameraLayout layout, Screen screen = Screen::Top);

    void update(Vec2 target);                   ///< exactly one 60 Hz frame (FR-12)
    void update(Rect target);                   ///< follows the box's centre
    void snap(Vec2 target);                     ///< FR-5: centred (+ offset), limits kept, no smoothing
    void snap(Rect target);
    void add_trauma(float amount);              ///< FR-8: clamped to [0, 1]
    void stop_shake();                          ///< trauma 0, final = followed

    Vec2 followed() const;                      ///< FR-11: top-left of the primary screen, no shake
    Vec2 position() const;                      ///< FR-11: with shake (never pixel-snapped)
    float trauma() const;
    bool drives(Screen screen) const;
    Camera2D camera(Screen screen) const;       ///< what apply() writes for `screen` (pixel snap applied)
    Rect view(Screen screen) const;             ///< visible world rectangle
    Vec2 to_world(Screen screen, Vec2 screen_point) const;  ///< camera(screen).position + p / zoom
    Vec2 to_screen(Screen screen, Vec2 world_point) const;  ///< (w − camera(screen).position) × zoom

    void apply() const;                         ///< gfx::set_camera on the driven screen(s) (FR-13)
    /// FR-14: dead zone, limits and target on the driven screen(s); an empty inline function in Release.
    void debug_draw() const {
        if constexpr (build::kDevToolsEnabled) detail::camera_debug_draw(*this);
    }

private:
    CameraSettings settings_{};                 // 84 B
    Vec2 desired_{}, followed_{}, final_{}, target_{};
    float trauma_ = 0;
    std::uint32_t noise_time_ = 0;              // 16.16 noise cells
    std::uint8_t flags_ = 0;                    // misuse logged once
};
} // namespace nnh
```

Exact names may still change while building; stage notes in this file record any change.

### Events introduced

None (spec non-goal, constitution IV): the rig is queried; `Camera2D` is set by command.

### Semantics fixed by the plan

All positions are **top-left corners in world pixels** of the rig's primary screen (the one in `settings.screen`, or
the top screen in `TallWorld`), the form `Camera2D::position` uses, so a rig configured like the platformer's
hand-written camera produces the same bits (AC-4 step 1).

- **View size.** `Single`: `(screen_width / zoom, 240 / zoom)`. `TallWorld`: the combined view
  `(400 / zoom, 480 / zoom + gap)`; the bottom screen's camera is `position + (40 / zoom, 240 / zoom + gap)` (centred
  under the top at any zoom, the gap in world pixels). `half = view × 0.5f`.
- **Update order (FR-12)**, one 60 Hz frame, per axis unless noted:
  1. Target: point `t`, or a box's centre `x + w / 2` (the platformer's own expression). A non-finite target asserts
     (Release: the update is skipped and logged once). `c = t + offset`.
  2. Desired: axis not followed → `desired_` unchanged. Dead zone of width 0 on that axis (a point, the default) →
     `desired = c − half − dz.x / zoom` (with `dz.x = 0` exactly `c − half`). Otherwise the zone's world edges are
     `left = desired_ + half + dz.x / zoom`, `right = left + dz.w / zoom`; `c < left` → `desired = desired_ + (c − left)`;
     `c > right` → `desired = desired_ + (c − right)`; else unchanged. The zone is measured against the previous
     **desired** (unsmoothed) position, so it doesn't interact with smoothing (Godot's drag). A zone wider than the
     view is clamped to the view (logged once per rig in Debug/Development).
  3. Clamp `desired` to the limits (if any): `lo = L.x`, `hi = (L.x + L.w) − view`. `hi ≥ lo` → `min(max(d, lo), hi)`;
     else the small-world alignment: Start `lo`, Center `lo + (L.w − view) × 0.5f`, End `hi`. `desired_ = desired`.
  4. Smoothing: fraction `f ≥ 1` → `followed = desired` (an assignment, so instant following is exact); else
     `d = desired − followed`; `−kCameraSnapDistance < d < kCameraSnapDistance` → `followed = desired`; else
     `followed = followed + d × f`. A still target settles to `desired`'s exact bits.
  5. Trauma: `trauma = max(0, trauma − trauma_decay)` (reaches exactly 0); `noise_time += round(shake_speed × 65536)`.
  6. Shake: `trauma > 0` → `s = (max_shake / zoom) × trauma² × noise(axis, noise_time)`; `final = followed + s`; else
     `final = followed` (no addition: exactly the followed bits).
  7. Final clamp, when `has_limits && shake_inside_limits` and the axis's limits are at least the view: clamp `final`
     to `[min(lo, followed), max(hi, followed)]`. So the shake never shows past the limits, while a camera still
     gliding into new limits (smoothing on, limits just changed) keeps gliding instead of jumping. On an axis where
     the world is smaller than the view the shake is not clamped (the view already shows past the level there).
- **Noise (FR-8).** Value noise, per axis: lattice value `v(i) = hash32(seed, axis, i)` → 24 bits → `[-1, 1]`
  (`(h >> 8) × 2⁻²³ − 1`, exact); `i = noise_time >> 16`, `u = (noise_time & 0xFFFF) × 2⁻¹⁶` (exact);
  `noise = v(i) + (v(i+1) − v(i)) × (u × u × (3 − 2u))`. The hash is an integer mix (lowbias32-style multiply/xor-shift
  constants), so the same seed, calls and updates give the same offsets on every system. Trauma² is fixed (the spec's
  "exponent documented"); the docs show trauma³ as `add_trauma` on a game's own curve.
- **`snap(target)` (FR-5):** `desired = c − half` on both axes (dead zone ignored: the target is centred, as a level
  start expects), clamped (step 3); `followed = desired`; `final` recomputed from the current shake offset (the shake
  is kept) and clamped (step 7). Trauma and noise time don't advance. Call it once at start: a rig that was never
  snapped moves from (0, 0).
- **Zoom about the centre (FR-9):** `set_zoom`/`set` with a new zoom add `half_old − half_new` to `desired_`,
  `followed_` and `final_`. Layout changes keep the primary screen's top-left (the next update re-centres).
- **Queries (FR-11):** `followed()`, `position()` (= `final_`), `trauma()`. `camera(s)`: the screen's
  `{position, zoom}` (bottom offset in TallWorld), pixel-snapped when `pixel_snap` (`floor(p × zoom + 0.5) / zoom`,
  own `floor` on values bounded to ±2³⁰; never applied to `followed()`/`position()`). Conversions use `camera(s)`, so
  they match what `apply()` makes the renderer draw: `to_world(s, p) = cam + p / zoom`, `to_screen(s, w) =
  (w − cam) × zoom`. A screen the rig doesn't drive asserts and answers for the primary screen.
- **Misuse (FR-15):** zoom outside [0.5, 4] or NaN, a negative dead-zone size, smoothing outside (0, 1], trauma decay ≤
  0, `shake_speed` outside (0, 1], negative gap, non-finite offset/limits/targets, negative limit size, NaN trauma:
  assert in Debug/Development; Release clamps (or keeps the previous value for NaN) and logs one error per rig
  (channel `camera`).
- **Profile scope** `camera` around `update()` (compiled out of Release by the profiler).

### Determinism (FR-12)

The rules of spec 015 apply: only IEEE `+ − × ÷`, comparisons and float↔int conversions; no `<cmath>` (the module's
`floor` is its own, on bounded values); every float operation lives in the module's `.cpp` files, compiled with
`-ffp-contract=off` (GCC/Clang). libctru's FPSCR is round-to-nearest with flush-to-zero (verified in spec 015); the
rig produces no subnormals except from subnormal inputs (documented, as for tweens). The renderer's new multiplies
(`draw_list.cpp`, `visible_range.cpp`, `zoom_edges.cpp`, `tile_expand.cpp`) get the same flag, so PC and 3DS place
zoomed draws on the same pixels. AC-2's golden script runs in `nnh_tests` on WinLibs GCC, GCC 13/14 and clang 18
(VM 120) and on the console (AC-6).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/camera.hpp` | new | the public API above (aim ≤ 220 lines with comments) |
| `engine/core/include/nnh/gfx.hpp` | changed | `Camera2D::zoom`, `kMin/kMaxCameraZoom`, comments |
| `engine/core/include/nnh/tilemap.hpp` | changed | `Tilemap::bounds()` |
| `engine/core/contract/nnh/platform/video.hpp` | changed | D33: `TileBatch2D::dst_w/dst_h`, `kTileOrientWiden/Taller`, comments ("drawn at dst size") |
| `engine/core/src/camera/camera_internal.hpp` | new | pure per-axis functions shared by the module (desired, clamp, smooth, final clamp, noise, view sizes, own `floor`) |
| `engine/core/src/camera/camera_rig.cpp` | new | construction, `set`/validation/misuse logs, setters, zoom about the centre, layout change |
| `engine/core/src/camera/camera_follow.cpp` | new | target centre + offset, axis locks, dead zone, smoothing (FR-2..FR-4) |
| `engine/core/src/camera/camera_limits.cpp` | new | limit clamp with alignment, final clamp (FR-6) |
| `engine/core/src/camera/camera_shake.cpp` | new | trauma, hash, value noise (FR-8) |
| `engine/core/src/camera/camera_update.cpp` | new | `update`, `snap`, `stop_shake`, profile scope (FR-5, FR-12) |
| `engine/core/src/camera/camera_layout.cpp` | new | views per layout, `camera(s)`, `view`, conversions, pixel snap (FR-10, FR-11, FR-13) |
| `engine/core/src/camera/camera_apply.cpp` | new | `apply()`: the module's only `gfx::set_camera` call |
| `engine/core/src/camera/camera_debug.cpp` | new | `detail::camera_debug_draw` (FR-14) |
| `engine/core/src/gfx/draw2d.cpp` | changed | `set_camera` clamps the zoom |
| `engine/core/src/gfx/draw_list.hpp` / `.cpp` | changed | `build(screen, Camera2D, …)`; zoom for sprites, shapes, text |
| `engine/core/src/gfx/gfx_frame.cpp` | changed | passes the `Camera2D`; tile chunk loop; `dst_w/dst_h` in the batch |
| `engine/core/src/gfx/tile_batches.hpp` / `.cpp` | changed | request `zoom`, `dst_w/dst_h`, `first_row`; result `next_row`; `detail::zoomed_tile_size()` |
| `engine/core/src/tilemap/visible_range.hpp` / `.cpp` | changed | `LayerPlacement::zoom`; zoomed parallax centre and origin; cells from the zoomed edges; zoom 1 = today's code |
| `engine/core/src/tilemap/zoom_edges.hpp` / `.cpp` | new | per-axis edge tables `edge(i)` for the visible cells (≤ 128 entries per axis, on the stack) |
| `engine/core/src/tilemap/tile_expand.hpp` / `.cpp` | changed | zoomed loop (edges → `x/y`, widen/taller bits), row resume; zoom-1 loop untouched |
| `engine/core/src/tilemap/map_draw.cpp` | changed | forwards zoom, `first_row`, `next_row` |
| `engine/core/src/tilemap/tilemap.cpp` | changed | `bounds()` |
| `engine/platforms/3ds/src/draw_tiles_3ds.cpp` | changed | `pos.w/h` from `dst_w/dst_h` + bits |
| `engine/platforms/pc/src/draw_tiles_pc.cpp` | changed | `dest` from `dst_w/dst_h` + bits; masked fast path; pre-swapped diagonal rectangle |
| `engine/core/CMakeLists.txt` | changed | camera sources; `-ffp-contract=off` on `src/camera/*` and the renderer files above |
| `tests/gfx/test_zoom1_regression.cpp` | new (stage 0) | pinned hash of every recorded item and quad of a rich scene at zoom 1 |
| `tests/gfx/test_draw_list_zoom.cpp`, `tests/gfx/test_camera_zoom_clamp.cpp`, `tests/gfx/test_tile_chunks.cpp` | new | zoom in `build`, `set_camera` clamp, row chunks and budget |
| `tests/tilemap/test_zoom_edges.cpp`, `test_visible_range_zoom.cpp`, `test_tile_expand_zoom.cpp`, `test_map_draw_zoom.cpp`, `test_map_bounds.cpp` | new | edges, visible cells, quads, end-to-end seams and sprite alignment, `bounds()` |
| `tests/platform_conformance/test_draw_tiles_pc.cpp` | changed | pixels of zoomed quads (×2 exact, widen, diagonal with `w ≠ h`) |
| `tests/camera/test_*.cpp` | new | see Test strategy |
| `tests/camera_example/test_self_check.cpp` | new | AC-2's script (the example's `self_check.cpp`) against its golden checksums |
| `tests/platformer/test_camera_follow.cpp` → `test_level_camera.cpp` | changed | step 1: a frozen copy of the old formula against the rig, bit for bit; step 2: the new feel's settings |
| `tests/CMakeLists.txt` | changed | the new files; the example's `self_check.cpp`; `camera_follow.cpp` out |
| `examples/camera/` | new | `main.cpp`, `camera_scene.{hpp,cpp}` (top view, controls, modes), `world.{hpp,cpp}` (map built in code, the moving squares), `hud.{hpp,cpp}`, `demos.{hpp,cpp}` (`CAMERA_DEMO`), `self_check.{hpp,cpp}` (pure script + checksums), `self_check_scene.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `art/make_art.py` + `assets/art/tiles.png` (original), `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(camera)` |
| `examples/platformer/level_camera.{hpp,cpp}` | new | the rig's settings (step 1: identical; step 2: the new feel) |
| `examples/platformer/camera_follow.{hpp,cpp}` | removed | replaced by the rig (AC-4) |
| `examples/platformer/level_play.{hpp,cpp}` | changed | a `CameraRig` member; `snap` in `restart()`, `update` + `apply` in `update()`; landing shake (step 2) |
| `examples/platformer/bench.{hpp,cpp}`, `gate.{hpp,cpp}` | changed | B cycles the bench zoom (1, 0.5, 2); a measure-only gate at zoom ≠ 1; map 64 × 40 cells, zoom-1 camera range unchanged |
| `examples/platformer/tests/level.nnhs` | changed | step 2: regenerated golden lines |
| `examples/platformer/tests/zoom_gate.nnhs` | new | the zoom-1 gate, then the measured limits at zooms 0.5 and 2 |
| docs (stage 10) | changed/new | `docs/api/camera.md` (new), `gfx.md`, `tilemap.md`, `math.md` if it lists `Camera2D`; `docs/architecture/rendering.md` (status → implemented), `decisions.md` (D33 note); `docs/tech/devkitpro-3ds.md`, `docs/tech/sdl3.md` (zoomed tiles); `docs/guides/levels.md` (limits from `bounds()`); `docs/REFERENCES.md`; `CLAUDE.md` layout; `docs/ROADMAP.md`; `specs/README.md` |

### Data and memory

| Item | Size (3DS and PC: only 4-byte and 1-byte fields) | Notes |
|------|---------------------------------------------------|-------|
| `CameraSettings` | 84 B | 8 one-byte fields first, then 19 floats/`uint32` |
| `CameraRig` | 128 B (84 + four `Vec2` 32 + trauma 4 + noise time 4 + flags 1, padded) | `static_assert(sizeof(CameraRig) <= 128)`; trivially copyable |
| `Camera2D` | 8 → 12 B | `GfxState::cameras[2]`: +8 B of bss |
| `TileBatch2D` | +8 B | built on the stack per call |
| `TileQuad2D` | 10 B, unchanged | scratch unchanged (1600 quads) |
| zoomed expansion | ≤ 2 × 128 `int16` edges on the stack (512 B) | only in the zoomed branch |

No heap anywhere in the module; the zoom adds no heap to `gfx`. Visible tiles: at zoom `z` a full layer shows about
`(400 / (tile × z) + 1) × (240 / (tile × z) + 1)` quads on the top screen: 16×16 tiles → 416 at zoom 1, 1581 at 0.5,
120 at 2. The platformer's `max_tiles` budget decides how many layers fit at zoom 0.5 (measured, AC-7).

### Platform work

- **pc:** `draw_tiles_pc.cpp` (above). No change to `draw2d_pc.cpp` (items arrive zoomed). Conformance test extended.
- **3ds:** `draw_tiles_3ds.cpp` (above). No change to `draw2d_3ds.cpp`. Verified by the console screenshots of
  `check.nnhs` (zoom 2, 0.5, 1.5, tall world) read back on the PC and by the gate (AC-7).
- **fake:** none: it already copies `TileBatch2D` (now with `dst_w/dst_h`) and every quad and item. Recorded limits
  (64 items, 16 batches, 512 quads) shape the tests: small maps, counts read from `quad_count`/`batch_count`.

### Performance (AC-7)

- **Rig:** per update ≈ 60 float operations for following/limits/smoothing (both axes) and ≈ 40 for the shake (two
  hashes and two smoothsteps). Divisions: the view size and the dead zone's `/ zoom` (about six VFP divides, ≈ 120
  cycles; kept as divisions rather than a reciprocal so zoom 1 stays exact). Estimate ≈ 600 cycles ≈ 2–3 µs on an
  Old 3DS: two rigs + apply ≈ 0.006 ms, under the 0.05 ms target with an order of magnitude to spare. Measured by
  `examples/camera`'s bench (each frame 16 updates + applies of a tall-world pair with dead zone, smoothing and
  trauma; 600 frames; Development build; the logged cost is per pair).
- **Renderer at zoom 1:** sprites/shapes gain one multiply per coordinate (exact); the tile path takes today's branch.
  The spec 012 gate (4 layers + 300 sprites) must still pass (AC-7). At zoom ≠ 1 per quad: two table reads and two
  compares more. The limits at zooms 0.5 and 2 are measured by `zoom_gate.nnhs` and documented (no pass threshold).
- **`hello` (AC-8):** no camera symbol (`nm`). Growth from FR-9 (`set_camera` clamp, `build` multiplies, chunk loop,
  `draw_tiles` fields): target ≤ 320 B of text on 3ds-release and ≤ 512 B on pc-release against stage 0. If over,
  the zoomed parts of `build` and `submit_tiles` move behind a `zoom != 1` branch into separate functions.

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows and VM 120). The rig needs no `nnh::init()`; tests
that apply or draw use the fake platform's `init`/`frame()`.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `tests/camera/test_camera_value.cpp`: size ≤ 128 (`static_assert` + test), trivially copyable, copies behave independently, works without `init()`, no allocation (`alloc_counter`, `mem::live_allocations`) over 1000 updates/applies/conversions |
| FR-2 | `test_camera_follow.cpp`: point, box centre, offset (changed mid-run), per-axis lock keeps the axis, last-standing-height use case |
| FR-3 | `test_camera_dead_zone.cpp`: target inside → still; leaving each edge → moved by exactly the overshoot; asymmetric zone; zone at zoom 2 is the same on screen; zone larger than the view clamped + one log (log capture) |
| FR-4 | `test_camera_smoothing.cpp`: fraction table, per axis, snap threshold (1/64 − ε vs + ε), still target settles to the exact bits, fraction 1 is an assignment (huge/small value pairs where `c + (d − c) ≠ d`) |
| FR-5 | `test_camera_snap.cpp`: centred + offset, limits kept, dead zone ignored, shake kept, `stop_shake` |
| FR-6 | `test_camera_limits.cpp`: clamp before smoothing (no ease into a wall), after shake, Start/Center/End on small worlds, limits changed at run time with smoothing (glide, final clamp keeps the followed range) and without (cut), `clear_limits` |
| FR-7 | `tests/tilemap/test_map_bounds.cpp`: code-built and loaded maps, empty map → `{}`; `nm` of a camera-only test object shows no tilemap symbol (checked once in T3.4) |
| FR-8 | `test_camera_shake.cpp` / `test_camera_noise.cpp`: add/clamp, linear decay to exactly 0, offset ≤ `max_shake / zoom`, same seed → same offsets, different seeds differ, final == followed once trauma is 0, shake inside limits and with the clamp off, small-world axis not clamped, noise continuous (|Δ| bounded per tick), noise golden values |
| FR-9 (helper) | `test_camera_zoom.cpp`: view size, zoom about the centre (centre bits kept within 1 ulp), dead zone in screen px, limits with the zoomed view |
| FR-9 (renderer) | `tests/gfx/test_zoom1_regression.cpp` (pinned before the change); `test_draw_list_zoom.cpp` (sprites incl. pivot/rotation/flip/scale, rects, outlines, lines, text at zooms 2 and 0.5; `ignore_camera` and debug pass unchanged); `test_camera_zoom_clamp.cpp`; `tests/tilemap/test_zoom_edges.cpp`, `test_visible_range_zoom.cpp` (cells follow the zoomed view; parallax centre), `test_tile_expand_zoom.cpp` (widen/taller bits, integer zooms bit-free), `test_map_draw_zoom.cpp` (no gaps/overlaps at 0.5, 0.75, 1.3, 1.5, 2, 3; parallax-1 layer aligned with sprites; 4× quads at 0.5); `tests/gfx/test_tile_chunks.cpp` (8×8 at 0.5: several `draw_tiles` calls, rows in order, totals right; budget drops as before); `test_draw_tiles_pc.cpp` (pixels) |
| FR-10 | `test_camera_layouts.cpp`: Single top/bottom, other screen untouched by `apply`, TallWorld bottom placement (inset 40/zoom, gap), combined limits and dead zone, a point crossing the screens continuously, two rigs as two views |
| FR-11 | `test_camera_convert.cpp`: round trips for both screens × Single/TallWorld × zooms 0.5, 1, 2, 3; `to_world` under a sprite drawn there (fake items: the sprite's item covers the point); `view()` |
| FR-12 | `test_camera_update.cpp`: one step per call (no wall time: fake clock advanced by 1 s changes nothing), not calling `update` freezes it; AC-2 golden on every compiler |
| FR-13 | `test_camera_apply.cpp`: `apply` writes the driven screen(s) only, zoom included, pixel snap rounds the applied camera but not `followed()`/`position()`; conversions use the snapped camera |
| FR-14 | `test_camera_debug.cpp`: Development build: shapes recorded on layer 15 with the documented colours on the driven screens, nothing outside a frame; Release `camera.elf` has no camera debug strings (`check_release_strings.sh`, T9.1) |
| FR-15 | `test_camera_misuse.cpp` (assert hook): each misuse asserts; Release path (`NNH_ASSERT` test hook) clamps/ignores and logs once per rig; cost: profile scope present; bench (AC-7) |
| FR-16 | T3.4/T7.5: `hello` sizes vs stage 0 and `nm` (no camera symbol), gate at zoom 1, AC-8 screenshots pixel-identical |
| AC-2/AC-6 | `tests/camera_example/test_self_check.cpp` + `examples/camera/tests/check.nnhs` (`--pc`, then the console) |
| AC-4 | `tests/platformer/test_level_camera.cpp`; ctest `platformer_replay_matches_script`; `level.nnhs --pc` (step 1 unchanged lines; step 2 regenerated) and on the console |
| AC-5 | PC screenshots (`CAMERA_DEMO=zoom1|zoom2|zoom05|tall|two`), read back: no seams, square continuous across the screens |
| AC-7 | `examples/camera` bench line; `zoom_gate.nnhs` on the console |
| AC-8 | stage 0 reference screenshots of `sprites`, `scenes`, `tweens`, `text`, platformer bench and level 1 compared byte for byte (`cmp`) after stages 4, 5 and 7 |

## Risks and alternatives

- **3DS texel bleeding at fractional zooms.** The PICA200 interpolates texcoords with limited precision; at a
  non-integer destination size a pixel centre near a tile's source edge could sample the neighbouring atlas texel.
  Integer zooms and zoom 1 are safe (texel centres). Mitigation if the console screenshots show it: inset the source
  rectangle by 1/256 texel **only** when `dst ≠ tile` (zoom 1 stays byte-identical). Not planned unless seen.
- **PC and 3DS may double different texel columns at fractional zooms** (rasterization rules differ). Positions and
  quads are identical (core computes them); the spec asks for positions, not identical pixels at fractional zooms.
  Documented; integer zooms are pixel-exact on both.
- **Zoom-1 regressions.** Closed by the pinned regression test (stage 0) and the AC-8 screenshot comparisons; the
  tile path keeps today's loop as its zoom-1 branch.
- **`hello` growth over the budget** → the zoomed code moves out of line behind a `zoom != 1` test (see Performance).
- **The 128-byte rig budget is full.** A new setting would need packing the bools into bits (designated initializers
  still work with C++20 bit-field initializers) or a 16-bit seed. Recorded, not needed now.
- **Platformer step 1 not bit-identical.** The plan's update order reproduces `follow_camera()`'s expressions
  (`x + w / 2`, `− view / 2` as `× 0.5f` of 400, clamp order irrelevant when `hi ≥ lo`, Start alignment = 0, an
  offset of +0 changes no bit, trauma 0 adds nothing); `test_level_camera.cpp` compares a frozen copy of the old
  formula over a dense sweep before the old file is removed, and the replay ctest must pass unchanged.
- **Step 2 golden lines on the console.** The shake makes the replay's camera depend on the noise; AC-6's self-check
  (run first in the console stage) proves the rig bit-identical on the console before `level.nnhs`.
- **Interpretation choices** (listed below) follow the spec's intent; the maintainer can revisit them in review.

## Decisions taken (pre-approved)

Small decisions the plan would normally ask about; each takes the recommended option.

1. **Names:** module `camera`, header `<nnh/camera.hpp>`, type `nnh::CameraRig` (it drives `Camera2D`s; "Camera"
   alone would collide with the future 3D camera), settings `CameraSettings`, `Tilemap::bounds()`.
2. **Two layout values** (`Single`, `TallWorld`); the spec's third mode, "two views", is two rigs in `Single`, named
   and pictured in the docs and shown by the example.
3. **`Camera2D` zoom is anchored at `position`** (top-left): `screen = (world − position) × zoom`. The rig keeps the
   centre.
4. **Outlines and lines stay one screen pixel thick** under zoom; rectangles' sizes scale.
5. **`set_camera` in Release clamps silently** (NaN → 1), keeping `hello` free of a new string.
6. **Dead zone**: a `Rect` relative to the view's centre in screen pixels (`centered_zone(w, h)` helper), measured
   against the unsmoothed desired position; a zero-width zone is a point (target held there).
7. **`snap` centres the target** (dead zone ignored) and keeps the shake.
8. **Trauma**: decays first, then the shake uses the decayed trauma (so `trauma() == 0` ⇒ `position() == followed()`);
   trauma² fixed; `max_shake` in screen pixels; noise speed in cells per frame (16.16 fixed point); the noise clock
   always advances.
9. **Final clamp** keeps the followed position's own range (a glide into new limits stays a glide), and doesn't
   apply on small-world axes.
10. **Instant following is an assignment**; the snap threshold is 1/64 world pixel.
11. **Conversions use the applied (pixel-snapped) camera**; `followed()`/`position()` are never snapped.
12. **TallWorld**: bottom camera = `position + (40 / zoom, 240 / zoom + gap)`; a layout change keeps the primary
    screen's top-left.
13. **Misuse in Release** logs one error per rig (a flag bit), so a NaN every frame doesn't flood the log.
14. **Debug drawing** uses `gfx` shapes with `ignore_camera` on layer 15 (≤ 10 draws per screen, counted in
    `max_draws`): dead zone yellow, limits red, target green cross.
15. **Tile chunks at row boundaries** instead of a larger scratch; `-ffp-contract=off` also on the four renderer files.
16. **`hello` budget**: ≤ 320 B of text on 3ds-release, ≤ 512 B on pc-release.
17. **Platformer bench**: B cycles zoom 1 → 0.5 → 2; the bench map grows to 64 × 40 cells so zoom 0.5 shows full
    layers, while the zoom-1 camera range stays the same (its screenshots stay pixel-identical, AC-8); a new
    `zoom_gate.nnhs` measures the zoomed limits; `gate.nnhs` is unchanged.
18. **Console stage**: three scripted runs, each started with Homebrew Launcher → Y (camera `check.nnhs`, platformer
    `level.nnhs`, platformer `zoom_gate.nnhs`); nothing else is asked of the maintainer.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | D33: zoom computed by core for every system; the contract change is two `int`s per batch and two orientation bits; PC and 3DS apply the same quads; `-ffp-contract=off` on the rig and the zoomed renderer code |
| II | ✅ | 128 B per rig, no heap; zoom-out bounded by `max_tiles`, row chunks instead of a bigger scratch; costs measured on the console (AC-7) |
| III | ✅ | Light header (gfx, build, `<cstdint>`); measured T0.1/T10.4 |
| IV | ✅ | No events; queries and one command (`apply`) |
| V | ✅ | No allocation, no `virtual`, no RTTI, no exceptions; misuse never UB |
| VI | ✅ | Every FR has unit tests written first; the zoom-1 regression is pinned before the renderer changes |
| VII | ✅ | Nine small camera files, one new tilemap file, one public header |
| VIII | ✅ | API pages, header comments, a new example, the platformer, the architecture page and D33 |
| IX | ✅ | Own code, prior art cited |
| X | ✅ | No external data; misuse loud in Debug/Development, harmless in Release |
| XI | ✅ | Optional module; `Camera2D` by hand (with zoom) fully supported; camera and tilemap independent; `hello` links nothing of it |

No constitution exception is requested. D33 amends D26 (see [decisions.md](../../docs/architecture/decisions.md)).

## Stage 0 results

Measured 2026-11-22 on the maintainer's Windows machine (WinLibs GCC, devkitARM), before any change to the renderer.

- `hello` sizes (`size`, text / data / bss): pc-release `hello.exe` 130 024 / 21 988 / 211 728; pc-debug 197 872 /
  26 756 / 214 384; 3ds-release `hello.elf` 216 484 / 28 328 / 106 696 (`.3dsx` 261 864 bytes); 3ds-dev `hello.elf`
  233 264 / 32 848 / 109 304.
- Build times (3 runs): edit `examples/hello/main.cpp` → simulator (pc-debug) 1.64, 1.61, 1.54 s; → `.3dsx`
  (3ds-release) 2.89, 2.85 s (a first run that reconfigured took 9.6 s); clean `nnh_core` (pc-debug, parallel) 5.95,
  5.65, 5.73 s.
- Header cost (`g++ -std=c++20 -fsyntax-only` of a one-line TU): `<nnh/gfx.hpp>` 768 preprocessed lines, 0.30–0.36 s.
- T0.2: `tests/gfx/test_zoom1_regression.cpp` pinned at `0x276C0CD5` (19 items, 6 batches and 148–166 quads per frame
  over three frames with fractional cameras on both screens; text with shadow and scale 2, sprites with every option,
  two maps with 8×8 and 16×16 tiles, parallax, offsets, flips incl. diagonal, an animated tile).
- T0.3: reference screenshots in `build/ref-018/` (not committed): `hello`, `sprites`, `scenes`, `text` at frame 30,
  `tweens` at 60, platformer `PLATFORMER_DEMO=bench` and level 1 at 90 (pc-debug, offscreen SDL software renderer).
  `build/ref-018/compare.sh` rebuilds and compares them byte for byte; the bench is compared on its top screen only
  (its bottom screen shows measured frame times, which differ from run to run).

## Stage 1–3 refinements and results

- **`CameraRig::aim()`** (public, read-only): the last target plus the offset, the point the dead zone holds. The
  plan's private `target_` slot is this value; `debug_draw` needs it for the green cross. The rig is 128 bytes as
  planned (settings 84 + four `Vec2` + trauma + noise clock + flags).
- **Smoothing below `kCameraMinSmoothing` (1/1024)** counts as misuse (the spec's "(0, 1]" has no smallest value a
  Release build could clamp to); Release clamps to it, or to 1 for NaN. `max_shake` (negative or non-finite) was added
  to the misuse list. Release keeps the previous value for a non-finite or negative dead zone, limits, offset, decay
  and speed ≤ 0, and clamps zoom, smoothing above 1, speed above 1 and a negative gap.
- **`snap()` places both axes**, also one that isn't followed: that is how a locked axis gets its position (a
  horizontal shooter snaps once, then follows x only).
- **The dead zone is clamped per axis at each update** (it depends on the zoom and the layout), with one warning per
  rig in Debug/Development (`dead zone larger than the view`); Release clamps silently.
- **The zoom range** lives in `camera_internal.hpp` until stage 4 adds `kMinCameraZoom`/`kMaxCameraZoom` to
  `<nnh/gfx.hpp>`.
- **Debug drawing**: the dead zone is drawn around the drawn (shaken) view's centre, so it sits where the player sees
  it; the limits only when they meet the screen; the target cross on every driven screen (the renderer clips it). At
  most 4 shape draws per screen.
- **`set_layout`** changes layout and screen only (no validation needed; the primary top-left is kept).
- The camera log channel `camera` is defined in `camera_rig.cpp` (linked only with the rig).
- T3.4 (opt-in): after stage 3, pc-release `hello.exe` text/data/bss 130 024 / 21 988 / 211 728 and 3ds-release
  `hello.elf` 216 484 / 28 328 / 106 696 — identical to stage 0; `nm` finds no camera symbol in either. The camera
  objects reference no tilemap symbol and the tilemap objects no camera symbol (`nm -C` of the pc-release objects; the
  camera's only outside references are `gfx::set_camera`, `gfx::draw_rect`, `gfx::draw_line` and the logger).
- T3.5 (platformer step 1): `examples/platformer/level_camera.{hpp,cpp}` (Start alignment, limits `map.bounds()`, no
  dead zone, no smoothing); `tests/platformer/test_level_camera.cpp` compares a frozen copy of `follow_camera()` with
  the rig over a dense sweep (five worlds incl. smaller than the view and exactly the view; `update` and `snap`): no
  mismatching bit. The ctest `platformer_replay_matches_script` passes and `nnh run platformer --pc --no-build --script
  examples/platformer/tests/level.nnhs` → `nnh: PASS (73 lines)`, both with the **unchanged** checksum lines.

## Stage 4 refinements and results

- **The cameras moved out of `GfxState`** into their own `Camera2D g_cameras[2]` (`detail::gfx_cameras()`, reset with
  the state at init and shutdown). With the zoom's default of 1 inside `GfxState`, the whole state (≈ 640 bytes) moved
  from `.bss` to `.data` and grew `hello`'s file by as much; apart, only the two cameras (24 bytes) are in `.data`.
- `DrawList::build` takes a `Camera2D`; the old `Vec2` form stays as a constrained template (zoom 1), so
  `tests/gfx/test_draw_list.cpp` is unchanged and `build(screen, {}, ...)` still means a default camera.
- `draw_list.cpp` is compiled with `-ffp-contract=off` (`_nnh_zoom_sources` in `engine/core/CMakeLists.txt`):
  `snap((p − c) × z)` must never become a fused multiply-add.
- `set_camera` in Release clamps silently (NaN → 1); the assert message exists only in Debug/Development.
- `hello` after stage 4 (Release): 3DS text 216 612 (+128), data 28 352 (+24), bss 106 680 (−16), `.3dsx` 262 640;
  PC text 130 232 (+208), data 22 020 (+32), bss 211 696 (−32). No camera symbol. Within the text budget (≤ 320 B
  3DS, ≤ 512 B PC); the remaining stages add the tile path's zoomed branch.
- T4.3: `test_zoom1_regression` passes unchanged; `build/ref-018/compare.sh`: every reference screenshot identical
  (`hello`, `sprites`, `scenes`, `tweens`, `text`, and the platformer's bench and level 1).

## Stage 5 refinements and results

- **Tile edges in 16.16 fixed point.** `edge(i) = O + ((i × tile × zq + 32768) >> 16)` with `zq = round(zoom × 65536)`
  (`tilemap/zoom_edges.hpp`), instead of a float product per edge. With float products a step could be one less than
  `floor(tile × zoom)` when `tile × zoom` is a hair above an integer, which the two orientation bits can't express;
  in integers every step is exactly `floor` or `floor + 1` by construction, for every zoom, and PC and 3DS compute the
  same edges. For every zoom with at most 16 fractional bits (0.5, 0.75, 1.25, 1.5, 2, 3, …) this is exactly the
  plan's `round_half_up(i × tile × z)`; other zooms are quantized to 1/65536 (invisible). The visible cells come from
  the same integers (`zoom_axis_range`, one 64-bit division per axis and batch).
- **Zoom 1 is decided on `zoom_q == 65536`**, so `visible_range` and `expand_tiles` take today's code for any zoom
  within 1/131072 of 1 (sprites still use the exact float zoom).
- **The destination size comes from the expand function** (`TileExpandResult::dst_w/dst_h`, filled by the tilemap's
  `expand_batch` with the same fixed-point rule as the edges), not from gfx: gfx never computes the rounding, and the
  code stays out of games that draw no tilemap. `detail::zoomed_tile_size` (gfx) became `detail::zoomed_size`
  (tilemap, `zoom_edges.hpp`).
- **The zoomed loop is its own file**, `tilemap/tile_expand_zoom.cpp` (`expand_zoomed`), called by `expand_tiles`
  when the view's `zoom_q` isn't 65536; the zoom-1 loop is untouched. A row is taken back when the scratch fills
  (`chunk`), resumed with `first_row`; when the frame's budget is the limit (`left < scratch`), tiles are dropped as at
  zoom 1. `-ffp-contract=off` covers `draw_list.cpp`, `visible_range.cpp`, `tile_expand*.cpp` and `zoom_edges.cpp`.
- `TileBatches::expand` takes a `Camera2D` and a `first_row` and returns `next_row` and the destination size;
  `gfx_frame.cpp` loops `expand → draw_tiles` until `next_row` is −1 (one call at zoom ≥ 1).
- PC backend: the fast path tests `(orient & 7) == 0`; the diagonal quad passes the pre-swapped rectangle. 3DS: `pos.w/h`
  from `dst_w/dst_h` plus the bits. The conformance test checks zoom 2 texel by texel and that a widened quad and a
  widened diagonal quad cover exactly 13 × 12 pixels; three hand mutants of the PC backend are killed.
- `hello` after stage 5 (Release): 3DS text 216 748 (**+264** against stage 0, budget 320), data 28 352 (+24); PC text
  130 732 (**+708**, budget 512: over). x86-64 code and 16-byte function alignment in the PE make the PC count about
  2.7 times the ARM one; moving `zoomed_tile_size` out of line made both larger, moving the rounding to the tilemap side
  saved 64 bytes (3DS). The 3DS budget, the one that matters for the console (constitution II), holds; the PC overrun
  is recorded here as a deviation.
- T5.6: `test_zoom1_regression` passes (dst = tile size at zoom 1); `build/ref-018/compare.sh`: platformer bench and
  level 1 identical. The zoomed screenshots of the platformer bench (B cycles the zoom) are part of T7.2; the zoomed
  tiles are first checked visually with `examples/camera` (stage 6).

## Stage 6 refinements and results

- **The zoom cycle is 1 → 1.5 → 2 → 3 → 0.5** (X): the spec's (0.5, 1, 2, 3) plus 1.5, so the console script can
  photograph a fractional zoom (T6.4, T9.2 ask for `zoom15`; fractional zooms are where texel bleeding would show).
- **L toggles a 24-pixel gap** in the tall world (the gap is a setting, FR-10; 0 by default so the square is
  continuous across the screens in the screenshot).
- **Files**: `main.cpp`, `world.{hpp,cpp}` (the map built in code: 2 × 2 rooms of 40 × 24 tiles = 1280 × 768 pixels,
  the player's square, the automatic target), `camera_scene.{hpp,cpp}`, `hud.{hpp,cpp}`, `self_check.{hpp,cpp}`,
  `self_check_scene.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `demos.hpp` (no `demos.cpp`: `demo_is` lives in `main.cpp`),
  `art/make_art.py` → `assets/art/tiles.png` (64 × 64, eight 16 × 16 tiles with a one-pixel frame on the top and left
  edges, so a seam would show as a doubled or a missing line). The log channel is `example` (the engine owns
  `camera`). `max_tiles` 8192 (two layers on both screens at zoom 0.5).
- **Self-check script** (600 updates): a target that runs, stops and teleports (with `snap`), a dead zone 48 × 32,
  smoothing (0.25, 0.125), limits narrowed at 230 and restored at 400, trauma at 60, 260 and 470, zooms 2 / 0.5 /
  1.5, an offset, the tall-world layout from 300, the bottom screen from 520, limits cleared at 560. Every factor of
  the target path is a short binary fraction, so the path is exact and the example's own `.cpp` needs no
  `-ffp-contract=off`. Golden `0x5201E6CF` (pc-debug and pc-release); ctest `camera_self_check_pc`.
- **Bench** (PC simulator, Debug): `camera: bench avg 0.0013 ms worst 0.0043 ms per pair (target < 0.05 ms) PASS`.
- **Screenshots read back** (`CAMERA_DEMO=zoom1|zoom2|zoom05|tall|two`, and the script's `zoom15`, `shake`, `tall`,
  `two`): no gap or doubled line between tiles at 0.5, 1.5 and 2 (the clear colour never shows through; at 1.5 the
  frame lines are one or two pixels wide, the documented uneven texel size); in the tall world the square stands in
  the doorway across the two screens, the brick rows continue from the top screen to the bottom one 40 pixels in; the
  marker lands under the touch; two views show the automatic target at zoom 0.5 below.
- `nnh run camera --pc --no-build --script examples/camera/tests/check.nnhs` → `nnh: PASS (86 lines, frames
  1–1623)`, twice.

## Stage 7 refinements and results

- **Platformer step 2** (`level_camera.{hpp,cpp}`): dead zone 32 × 48 screen pixels, smoothing (1, 0.15) — y only, so
  runs stay crisp and jumps don't jerk the view —, `max_shake` (4, 6), map limits with Start alignment; a look-ahead
  of 32 pixels in the facing direction that moves 1 pixel per frame when the player turns (`look_ahead_step`, the
  rig's offset); a landing at full fall speed (6 px/frame, `kHardLandingSpeed`) adds trauma 0.5 (`landing_trauma`).
  Tests: the frozen old camera against a rig configured as in step 1 (kept), the step 2 settings, the settling at the
  edges, the look-ahead (64 frames for a full turn), the landing threshold.
- **`restart()` assigns a fresh rig.** The first regeneration of the replay lines passed standalone
  (`PLATFORMER_DEMO=replay`) but `level.nnhs` failed at frame 360: the script plays level 1 before the replay, and the
  shake's noise clock (which `stop_shake()` and `snap()` don't reset) had advanced, so the replay's landing shake
  sampled other noise. A new `CameraRig` in `restart()` (trauma 0, noise clock 0) makes every restart — and the
  replay — start the same. Documented in `docs/api/camera.md` ("assign a fresh rig").
- **New replay lines** (`examples/platformer/tests/level.nnhs`, regenerated in the same commit as the feel): frame 60
  is unchanged (the camera is still at the level's left/bottom edge), then `120 … sum 712D6080` … `840 … sum
  23D5768C`, `replay: done 900 frames sum 7CA9B256: 60 Hz PASS`. ctest `platformer_replay_matches_script` passes and
  `nnh run platformer --pc --no-build --script examples/platformer/tests/level.nnhs` → `nnh: PASS (74 lines)`.
- **Bench zoom** (T7.2): B cycles 1 → 0.5 → 2 (`next_bench_zoom`), the bench sets `Camera2D` by hand (no rig, XI);
  map 64 × 40 cells; the wandering camera's range is (288, 96) at zoom 1 — spec 012's, so the bench screenshot is
  identical (`compare.sh`) — and keeps the view on the map at the other zooms (`bench_camera_max`); `max_tiles` =
  12 × 51 × 31 (full layers at zoom 0.5; the scratch is unchanged, only the budget grows). A at zoom ≠ 1 runs the same
  trials, measure only: `bench: zoom Z limits L layers (300 sprites), S sprites (4 layers)` then `bench: zoom Z done`
  (`format_zoom_limits`/`format_zoom_done`, unit-tested), and the screens say MEASURED instead of a verdict.
- **`zoom_gate.nnhs`** (T7.3) with `--pc`: `nnh: PASS (50 lines, frames 1–18101)`; zoom 1: `bench: PASS 4 layers + 300
  sprites, avg 16.7 ms, worst 17.6 ms` (limits 12 layers, 2000 sprites: the PC's maximum); zoom 0.5: limits 12 layers,
  1500 sprites; zoom 2: limits 3 layers, 200 sprites. The PC's zoom-2 numbers are an artefact of the simulator's
  renderer (scaled texture copies cost more there than 1:1 copies; zoom 2 draws a quarter of the tiles: 408 for 4
  layers); the console numbers are the ones that count (stage 9). Screenshots read back: zoom 0.5 and 2 without seams.
- T7.4: `hello` as in stage 5 (Release 3DS text 216 748, PC 130 732; no camera symbol); `build/ref-018/compare.sh`:
  every reference screenshot identical after stage 7 (platformer level 1 too: at frame 90 the camera is still at the
  level's edge).

## Stage 8: Linux checks (left to the maintainer)

Not run in this session (the maintainer runs VM 120). What to compare on GCC 13, GCC 14 and clang 18 (ASan/UBSan):
`nnh_tests` and ctest pass; `noise_golden_values` (six bit patterns in `tests/camera/test_camera_noise.cpp`),
`zoom_1_draws_the_same_items_batches_and_quads_as_before_zoom` (`0x276C0CD5`) and
`the_camera_self_check_reaches_the_golden_checksum` (`0x5201E6CF`) pass unchanged.

## Stage 9: console run (prepared)

T9.1 was done on the PC: `scripts/dkp.sh cmake --build --preset 3ds-release` and the Development builds
(`3ds-dev`) of `camera`, `platformer` and `hello` build without a warning; `scripts/check_release_strings.sh
build/3ds-release/examples/camera/camera.elf` → ok (the Release `camera.elf` has no `camera_debug_draw` and none of
the Debug-only strings; the misuse error text stays, as FR-15 wants a Release log). 3ds-release `camera.elf` text
256 716; 3ds-dev `hello.elf` text 233 560 (stage 0: 233 264).

T9.2, on an Old 3DS, nobody touching it after Y. Three runs, each: open the Homebrew Launcher, press **Y**, then on the
PC (`nnh` builds the Development `.3dsx`, sends it and plays the script; logs and screenshots in `build/logs/`):

1. `build/pc-debug/tools/nnh/nnh run camera --script examples/camera/tests/check.nnhs` (about 30 s). Expected, as on
   the PC: `camera: zoom 1.50`, `2.00`, `3.00`, `0.50`, `1.00`; `camera: mode tall world`, `camera: marker at …`,
   `camera: mode two views`, `camera: mode single view`; `camera: self-check started`; `camera: 60 sum C1C4E2C9`,
   `120 sum 50E077AD`, `180 sum 6FE473D5`, `240 sum FBE97331`, `300 sum FFE1EC52`, `360 sum 27C62865`,
   `420 sum 3F549B0D`, `480 sum E3526DCC`, `540 sum CDCE34AF`, `camera: PASS 5201E6CF`; `camera: bench avg X ms worst
   Y ms per pair (target < 0.05 ms) PASS`; `nnh: PASS (86 lines, …)`. Then read the screenshots `zoom15`, `zoom05`,
   `zoom2`, `tall` (top and bottom): no seams between tiles, no texel bleeding (a stray line of the neighbouring
   tile's colour at a tile's edge; if seen at 1.5 or 0.5, apply the plan's mitigation), the square continuous across
   the screens in `tall`.
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (about 40 s). Expected:
   `replay: 60 level1 x 121 y 242 coins 1 sum 0CF7E4CE`, `120 … sum 712D6080`, `180 … sum B81241F7`,
   `240 … sum 334BB1A5`, `300 … sum 47219250`, `360 … sum CC2AA785`, `420 … sum 27D1EDDA`, `480 … sum A8162475`,
   `540 … sum 1100B98D`, `600 … sum 8E234F2C`, `660 bonus … sum CFF54129`, `720 … sum A1A13061`, `780 … sum 8607A3AD`,
   `840 … sum 23D5768C`, `replay: done 900 frames sum 7CA9B256: 60 Hz PASS`; `nnh: PASS`.
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/zoom_gate.nnhs` (up to about
   7.5 minutes). Expected: `bench: PASS 4 layers + 300 sprites, …` (zoom 1: spec 012's gate still passes), `bench:
   zoom 0.50 limits L layers (300 sprites), S sprites (4 layers)`, `bench: zoom 0.50 done`, `bench: zoom 2.00 limits
   …`, `bench: zoom 2.00 done`, `nnh: PASS`. Record L and S at both zooms here and in `docs/api/tilemap.md#zoom`.

#### Stage 9 results (2026-11-22, Old 3DS XL, Development)

- `camera/tests/check.nnhs`: `nnh: PASS (86 lines)`; every checkpoint and `camera: PASS 5201E6CF` equal the PC's;
  bench 16 pairs per frame, **0.0315 ms** avg, 0.0413 ms worst per pair (target < 0.05 ms) PASS. Screenshots
  (`build/logs/camera-20261122-221548/`): zoom 1, 1.5, 2 and tall world without seams. At zoom 0.5 there are no gaps
  either, but the 3DS keeps fragments of the tiles' 1-pixel border lines that the PC drops (both minify by nearest
  sampling and pick different texel columns: the plan's second risk). Cosmetic; the 1/256-texel source inset is the
  candidate fix if a game needs clean minified pixel art.
- `platformer/tests/level.nnhs`: `nnh: PASS (74 lines)`; the 15 regenerated replay lines equal the PC's,
  `replay: done 900 frames sum 7CA9B256: 60 Hz PASS (level1 worst 19.0 ms, missed 0/599; bonus worst 19.4 ms, missed
  0/299)`.
- `platformer/tests/zoom_gate.nnhs`: `nnh: PASS (50 lines)`. Zoom 1: 4 layers + 300 sprites PASS (avg 16.7 ms, worst
  17.7 ms, missed 0/600), limits 6 layers / 600 sprites (as before). **Zoom 0.5: 2 full layers with 300 sprites; 4
  layers don't fit.** **Zoom 2: 12 layers (the most the bench has) with 300 sprites; 900 sprites with 4 layers.**

## Mutation testing

66 hand mutants (`build/ref-018/mutants.txt`, applied one at a time by a script: rebuild the test program, run the
whole `nnh_tests` — or `nnh_conformance_pc` for the PC backend —, restore the file) of `src/camera/*`,
`tilemap/zoom_edges.*`, `tile_expand_zoom.cpp`, the zoomed parts of `visible_range.cpp`, `draw_list.cpp`,
`tile_batches.cpp`, `gfx_frame.cpp`, `map_draw.cpp`, `Tilemap::bounds()`, the platformer's `level_camera.cpp` and the
example's self-check script: **64/66 killed** on the first run. The two survivors exposed test gaps, closed with
new checks: a dead zone lying entirely before the view must clamp to size 0, not a negative size
(`zone_axis_clamps_only_what_passes_the_view`), and the debug dead zone must be drawn in screen pixels at zoom 2
(`debug_draw_shows_the_dead_zone_in_screen_pixels_at_any_zoom`); both mutants are killed now (**66/66**). Plus three
mutants of the PC tile backend (no pre-swapped diagonal rectangle, widen bit ignored, tile size instead of the
destination size): 3/3 killed by the conformance test. Covered among others: dead-zone edges and point zones, the
fraction-1 assignment, the 1/64 snap, small-world alignment, both clamps, smoothstep, hash and lattice values, the
noise step, trauma² and its decay and clamp, zoom about the centre, the tall-world inset and gap, pixel snap,
conversions, the 16.16 edge rounding, the visible range's first/last cells, widen/taller bits, chunk rollback and
resume, zoomed parallax, sprite/pivot/rect/line zoom, `ignore_camera`, the chunk flag, the destination size, the
camera reset at init, the look-ahead and the landing threshold.

## Stage 10 results

- Header cost (`g++ -std=c++20 -fsyntax-only` of a one-line TU, 3 runs): `<nnh/camera.hpp>` 956 preprocessed lines,
  0.30–0.33 s; `<nnh/gfx.hpp>` 779 lines (+11 for the zoom), 0.31–0.33 s (stage 0: 768 lines, 0.30–0.36 s). The
  camera header costs what `gfx.hpp` costs (`<cstdint>`, `build.hpp`, `gfx.hpp`).
- Build times (3 runs): edit `hello` → simulator 1.63, 1.58, 1.68 s (stage 0: 1.54–1.64); → `.3dsx` 2.83, 2.91 s (a
  first run with a reconfigure 5.4 s); clean `nnh_core` (pc-debug, parallel) 5.98, 5.67, 5.85 s (stage 0: 5.65–5.95).
- `hello` Release: 3DS text 216 748 (+264), data 28 352 (+24), bss 106 680 (−16), `.3dsx` 262 156; PC text 130 732
  (+708, over the 512 budget, see stage 5), no camera symbol.
- `nnh_tests` (pc-debug): 1849 tests, 0 failures; ctest pc-debug 258/258, pc-release 252/252; every example script
  passes with `--pc` (`input`, `tweens`, `animation`, `actions`, `data`, `camera`, platformer `level`, `gate`,
  `zoom_gate`). Note: the test runner's `kMaxTests` is 2048 (199 left).
