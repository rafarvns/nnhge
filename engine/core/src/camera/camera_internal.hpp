#pragma once
// Pure per-axis pieces of the camera rig (spec 018, plan "Semantics fixed by the plan"), shared by the module's
// files and unit tested through CameraRig and directly (tests/camera/). Only + − × ÷, comparisons and float↔int
// conversions: every float operation lives in src/camera/*.cpp, compiled with -ffp-contract=off (FR-12).

#include <cstdint>

#include "nnh/camera.hpp"
#include "nnh/log.hpp"

namespace nnh {
NNH_LOG_CHANNEL_EXTERN(camera);
} // namespace nnh

namespace nnh::detail::cam {

inline constexpr float kMinZoom = 0.5f;
inline constexpr float kMaxZoom = 4.0f;
inline constexpr float kScreenHeight = 240.0f;
inline constexpr float kTallWidth = 400.0f; // the top screen's width (TallWorld's combined view)

// CameraRig::flags_ bits.
inline constexpr std::uint8_t kFlagMisuseLogged = 1;
inline constexpr std::uint8_t kFlagZoneLogged = 2;

/// False for NaN and ±infinity.
inline bool finite(float v) { return v - v == 0.0f; }
inline bool finite(Vec2 v) { return finite(v.x) && finite(v.y); }

/// The world size the primary view covers: Single (screen_width / zoom, 240 / zoom); TallWorld the combined view
/// (400 / zoom, 480 / zoom + gap).
Vec2 view_size(const CameraSettings& s);

/// The primary screen (Single: settings.screen; TallWorld: Top).
Screen primary(const CameraSettings& s);

/// A target box's centre, `x + w / 2` per axis (the platformer's own expression).
Vec2 box_centre(Rect r);

/// One axis of a dead zone in world pixels from the view's centre: [pos, pos + size). Clamped to the view's half
/// extent `half`; `clamped` is set when that changed something.
struct Zone {
    float pos = 0;
    float size = 0;
};
Zone zone_axis(float zone_pos, float zone_size, float zoom, float half, bool& clamped);

/// FR-3: the new desired position on one axis. `prev` = the previous desired position, `c` = the target's centre
/// plus offset, `half` = half the view. A zone of size 0 holds the target at its point (`c − half − pos`); otherwise
/// the target moves inside the zone freely and drags the view by its overshoot.
float desired_axis(float prev, float c, float half, Zone zone);

/// FR-6: `d` clamped so [d, d + view) stays inside [lo, lo + size); on a small axis (size < view) the alignment.
float clamp_axis(float d, float lo, float size, float view, CameraAlign align);

/// FR-6 final clamp: `v` (followed + shake) kept inside [min(lo, followed), max(hi, followed)] when the axis fits the
/// view (hi ≥ lo); unchanged on a small axis.
float final_clamp_axis(float v, float followed, float lo, float size, float view);

/// FR-4: one frame of smoothing: fraction `f` ≥ 1 → `desired` (an assignment); closer than kCameraSnapDistance →
/// `desired`; else `followed + (desired − followed) × f`.
float smooth_axis(float followed, float desired, float f);

/// FR-8 noise: the integer hash of a lattice point.
std::uint32_t noise_hash(std::uint32_t seed, int axis, std::uint32_t cell);
/// The lattice value of a hash in [-1, 1): `(h >> 8) × 2⁻²³ − 1` (exact).
float lattice_value(std::uint32_t hash);
/// Smooth value noise in [-1, 1] at 16.16 time `t` on `axis` (0 = x, 1 = y).
float noise(std::uint32_t seed, int axis, std::uint32_t t);
/// The noise clock's step per frame: round(speed × 65536), at least 1.
std::uint32_t noise_step(float speed);
/// The shake offset in world pixels: `(max_shake / zoom) × trauma² × noise` per axis; (0, 0) at trauma 0.
Vec2 shake_offset(const CameraSettings& s, float trauma, std::uint32_t t);

/// floor(v + 0.5) for |v| < 2³⁰ (own code: no libm in the module).
float round_half_up(float v);

} // namespace nnh::detail::cam
