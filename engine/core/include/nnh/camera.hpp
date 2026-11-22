#pragma once
/// @file nnh/camera.hpp
/// Camera helpers (spec 018): a CameraRig follows a target with a dead zone and smoothing, stays inside
/// limits, shakes from trauma, zooms, and drives one screen or both screens as one tall world. It is a plain
/// value the game owns (one per view) and updates once per frame; it writes its result into gfx::set_camera.
/// Optional: a game that sets Camera2D by hand keeps doing so and links none of this. See docs/api/camera.md.
///
/// @code
/// nnh::CameraRig rig{{.dead_zone = nnh::CameraSettings::centered_zone(48, 32), .smoothing = {1, 0.2f}}};
/// rig.set_limits(map.bounds());
/// rig.snap(player.box());       // level start: no glide
/// // every frame, in the scene's update(), after moving the player:
/// rig.update(player.box());
/// rig.apply();
/// @endcode

#include "nnh/build.hpp"
#include "nnh/gfx.hpp"

#include <cstdint>

namespace nnh {

class CameraRig;
namespace detail {
void camera_debug_draw(const CameraRig& rig); // camera_debug.cpp (Debug/Development)
} // namespace detail

/// What a rig drives (FR-10). "Two views" (game above, map below) is two rigs, each Single on its own screen.
enum class CameraLayout : std::uint8_t {
    Single,    ///< one screen (CameraSettings::screen); the other screen's camera is untouched
    TallWorld, ///< both screens as one tall world: the bottom view under the top one, centred (40 px inset),
               ///< `gap` world pixels apart
};

/// Where the view sits on an axis where the limits are smaller than the view (FR-6).
enum class CameraAlign : std::uint8_t {
    Start,  ///< the view's left/top edge at the limits'
    Center, ///< the limits centred in the view
    End,    ///< the view's right/bottom edge at the limits'
};

/// Smoothing snaps to the desired position when closer than this, in world pixels (FR-4).
inline constexpr float kCameraSnapDistance = 1.0f / 64;
/// TallWorld: the bottom screen's view starts this many screen pixels right of the top's ((400 − 320) / 2).
inline constexpr int kTallWorldInset = 40;
/// Smallest smoothing fraction accepted (a smaller or non-positive one asserts; Release clamps to it).
inline constexpr float kCameraMinSmoothing = 1.0f / 1024;

/// A rig's settings, for designated initializers. Validated by CameraRig::set (misuse: see CameraRig).
struct CameraSettings {
    Screen screen = Screen::Top;                   ///< Single: the screen it drives (TallWorld: both)
    CameraLayout layout = CameraLayout::Single;    ///< FR-10
    CameraAlign small_world = CameraAlign::Center; ///< FR-6: placement on an axis where the limits are small
    bool follow_x = true;                          ///< FR-2: false = that axis keeps its position
    bool follow_y = true;
    bool has_limits = false;         ///< FR-6: set by set_limits()
    bool shake_inside_limits = true; ///< FR-8: false = the shake may show up to max_shake past the limits
    bool pixel_snap = false; ///< FR-13: the applied camera in whole screen pixels (shimmer-free pixel art)
    Vec2 offset{};           ///< FR-2: world pixels added to the target's centre (a look-ahead)
    /// FR-3: screen pixels relative to the view's centre (the same feel at any zoom); empty (the default)
    /// keeps the target centred. A zone wider or taller than the view is clamped to it (logged once in
    /// Debug/Development).
    Rect dead_zone{};
    Vec2 smoothing{1, 1}; ///< FR-4: fraction of the remaining distance per frame per axis, (0, 1]
    float zoom = 1;       ///< FR-9: kMinCameraZoom..kMaxCameraZoom, about the view's centre
    Rect limits{};        ///< FR-6: world pixels the view stays inside (has_limits), e.g. map.bounds()
    float gap = 0;        ///< FR-10 TallWorld: world pixels hidden between the screens, ≥ 0
    Vec2 max_shake{8, 6}; ///< FR-8: screen pixels of shake at trauma 1
    float trauma_decay = 1.0f / 60;  ///< FR-8: trauma lost per frame, > 0 (full trauma lasts one second)
    float shake_speed = 0.5f;        ///< FR-8: noise cells per frame, (0, 1] (higher = faster shake)
    std::uint32_t seed = 0x4E4E4843; ///< FR-8: the noise's seed ('NNHC'); same seed = same shake
    /// A `w` × `h` dead zone centred on the view: `.dead_zone = nnh::CameraSettings::centered_zone(64, 48)`.
    static constexpr Rect centered_zone(float w, float h) { return {-w / 2, -h / 2, w, h}; }
};

/// A camera helper (FR-1): a plain value the game keeps per view. 128 bytes inline, no heap, no init() needed
/// for its computations (apply() and debug_draw() need the engine), trivially copyable (a copy is an
/// independent rig).
///
/// Each frame, in the scene's update() after moving the target: update(target), then apply() (or read
/// position() and set Camera2D yourself). update() advances exactly one 60 Hz frame (dead zone, smoothing,
/// trauma decay, noise), never by measured time, using only + − × ÷ and comparisons: the same settings and
/// calls give the same positions, bit for bit, on every compiler and on the 3DS (replays and console checks
/// compare exactly). Not calling update() freezes the camera. Every call is cheap (a few dozen float
/// operations), never allocates, publishes no event and is safe per frame.
///
/// Positions are top-left corners in world pixels of the primary screen (`settings().screen`, or the top
/// screen in TallWorld), as Camera2D::position.
///
/// Misuse (zoom outside 0.5..4 or NaN, a negative dead zone size, smoothing outside [kCameraMinSmoothing, 1],
/// trauma_decay ≤ 0, a negative or non-finite max_shake,
/// shake_speed outside (0, 1], a negative gap, non-finite offsets, limits or targets, negative limit sizes,
/// NaN trauma, asking for a screen the rig doesn't drive) asserts in Debug/Development; Release clamps the
/// value (or keeps the previous one for NaN, or skips the update for a bad target) and logs one error per rig
/// (channel `camera`).
class CameraRig {
  public:
    CameraRig() = default;
    explicit CameraRig(const CameraSettings& settings);

    /// Replaces every setting (validated). Any time, also between updates; a zoom change keeps the view's
    /// centre, a layout change keeps the primary screen's top-left (the next update re-centres).
    void set(const CameraSettings& settings);
    const CameraSettings& settings() const { return settings_; }
    void set_offset(Vec2 offset);                                      ///< FR-2, e.g. driven by a tween
    void set_zoom(float zoom);                                         ///< FR-9: keeps the view's centre
    void set_limits(Rect limits);                                      ///< FR-6: has_limits = true
    void clear_limits();                                               ///< FR-6: has_limits = false
    void set_layout(CameraLayout layout, Screen screen = Screen::Top); ///< FR-10

    /// FR-12: one 60 Hz frame towards `target` (a point, or a box followed by its centre `x + w / 2`).
    void update(Vec2 target);
    void update(Rect target);
    /// FR-5: the target centred (plus the offset) at once, inside the limits; no smoothing, the dead zone
    /// ignored, both axes placed (even one not followed: that is how a locked axis gets its position). Keeps
    /// the shake (see stop_shake()); trauma and the noise clock don't advance. Call it at level start,
    /// respawn, teleport.
    void snap(Vec2 target);
    void snap(Rect target);
    /// FR-8: adds trauma, clamped to [0, 1] (negative amounts lower it). The shake is
    /// `max_shake / zoom × trauma² × noise` per axis, on top of the followed position, which it never moves.
    void add_trauma(float amount);
    /// Trauma 0 now: position() == followed().
    void stop_shake();

    Vec2 followed() const { return followed_; } ///< FR-11: the followed position, without shake
    Vec2 position() const { return final_; }    ///< FR-11: with shake; never pixel-snapped
    float trauma() const { return trauma_; }
    /// Whether apply() writes `screen`'s camera (Single: its screen; TallWorld: both).
    bool drives(Screen screen) const;
    /// What apply() writes for `screen` (the bottom view's offset in TallWorld, pixel_snap applied).
    Camera2D camera(Screen screen) const;
    /// The world rectangle `screen` shows: camera(screen).position and the screen's size / zoom.
    Rect view(Screen screen) const;
    /// FR-11: the world point under a screen point (a touch), and back: `camera(s).position + p / zoom`,
    /// `(w − camera(s).position) × zoom`. They match what apply() makes the renderer draw.
    Vec2 to_world(Screen screen, Vec2 screen_point) const;
    Vec2 to_screen(Screen screen, Vec2 world_point) const;

    /// FR-13: gfx::set_camera on the driven screen(s). Needs the engine (nnh::init()).
    void apply() const;
    /// FR-14: draws the dead zone (yellow), the limits where visible (red) and the target (green cross) on
    /// the driven screen(s), layer 15, ignoring the camera (≤ 4 shape draws per screen). Only inside a frame.
    /// An empty inline function in Release.
    void debug_draw() const {
        if constexpr (build::kDevToolsEnabled) detail::camera_debug_draw(*this);
    }

    /// The last target given to update()/snap() plus the offset (what the dead zone holds), for debug
    /// drawing.
    Vec2 aim() const { return aim_; }

  private:
    void place(Vec2 target, bool instant);
    bool misuse(bool ok, const char* what);
    void move_all(Vec2 delta);

    CameraSettings settings_{};
    Vec2 desired_{};  // unsmoothed, clamped
    Vec2 followed_{}; // smoothed
    Vec2 final_{};    // with shake
    Vec2 aim_{};      // last target + offset
    float trauma_ = 0;
    std::uint32_t noise_time_ = 0; // 16.16 noise cells
    std::uint8_t flags_ = 0;       // misuse logged once
};

static_assert(sizeof(CameraRig) <= 128, "a camera rig stays small (spec 018 FR-15)");

} // namespace nnh
