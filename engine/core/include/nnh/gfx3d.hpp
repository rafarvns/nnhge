#pragma once
/// @file nnh/gfx3d.hpp
/// 3D drawing on the top screen (spec 026, decision D43): a 3D camera, an ambient colour and up to 4 lights per screen,
/// and draws of meshes (<nnh/mesh.hpp>) recorded like 2D and submitted at the end of the frame between the 2D layers:
/// layers below the screen's world layer (default 8) are drawn first (a sky), then the 3D world, then the layers from
/// it up (the HUD), then the debug pass. See docs/api/gfx3d.md.
///
/// 3D is enabled by a capacity: AppConfig::gfx.max_draws_3d (default 0 = off). Recording is O(1) and allocates
/// nothing; culling, order (opaque and cut-out in call order, then blended back to front) and submission happen at the
/// end of the frame. Everything a game reads (to_screen, screen_ray, stats) gives the same bits on PC and 3DS.
/// The bottom screen draws no 3D in this version: draws there assert and do nothing.
///
/// Note: 2D drawn on layers below the world layer (text's default layer 0 included) is under the 3D world in frames
/// with 3D draws; draw the HUD on layer 8 or above.
///
/// @code
/// nnh::Camera3D cam;
/// cam.position = {0, 2, 6};
/// nnh::gfx3d::point_at(cam, {0, 0, 0});
/// nnh::gfx3d::set_camera(nnh::Screen::Top, cam);
/// nnh::gfx3d::draw(nnh::Screen::Top, ship, nnh::Transform3D{.rotation = nnh::quat_axis_angle({0, 1, 0}, t)});
/// @endcode

#include <cstddef>
#include <cstdint>

#include "nnh/mesh.hpp"

namespace nnh {

enum class Eye : std::uint8_t { Left, Right };
enum class Projection : std::uint8_t { Perspective, Orthographic };

/// Stereo settings of a camera (used from spec 026's stereo part; orthographic cameras are always mono).
struct StereoSettings {
    bool enabled = true;
    float strength = 0;        ///< eye separation at slider 1, world units; 0 = focal_distance / 20
    float focal_distance = 5;  ///< points this far ahead appear at screen depth
};

/// A 3D camera: looks down its −z with +y up. Plain value.
struct Camera3D {
    Vec3 position{0, 0, 5};
    Quat rotation{};
    Projection projection = Projection::Perspective;
    float fov_y = 1.04719758f;  ///< vertical field of view, radians (60°), 0 < fov_y < π
    float ortho_height = 10;    ///< world units shown vertically (orthographic)
    float near_plane = 0.1f;    ///< > 0
    float far_plane = 100.0f;   ///< > near_plane
    StereoSettings stereo{};
};

enum class LightKind : std::uint8_t { Directional, Point };

/// A light in world space. Directional: `vector` points towards the light. Point: `vector` is its position and the
/// light falls off as (1 − d/range)², zero at `range`.
struct Light3D {
    LightKind kind = LightKind::Directional;
    Vec3 vector{-1, 1, 1};
    Color color = colors::white;
    float range = 10;
};

/// A screen's lights: an ambient colour and up to 4 lights (`count` of `lights`). The default (also with nothing set):
/// ambient 0.3 grey and one white directional light from the upper front left.
struct Lighting {
    Color ambient{77, 77, 77, 255};
    Light3D lights[4] = {Light3D{}};
    int count = 1;
};

/// Per-draw options: one submesh (−1 = all), a material replacing every drawn submesh's, a tint multiplied in.
struct DrawOptions3D {
    int submesh = -1;
    const Material3D* material = nullptr; ///< copied at the call
    Color tint = colors::white;
};

/// Where a world point lands on the screen (centre eye).
struct ScreenPoint {
    Vec2 pixel{};          ///< screen pixels, (0, 0) top-left
    float depth = 0;       ///< distance along the camera's view axis (negative behind it)
    bool in_front = false; ///< at least near_plane ahead
    bool in_view = false;  ///< in front, between the planes and inside the screen
};

struct Stats3D {
    int draws = 0;          ///< recorded last frame
    int culled = 0;         ///< skipped last frame (outside the view)
    int dropped_total = 0;  ///< past max_draws_3d since init
    int peak_draws = 0;
    int max_draws = 0;
    int triangles[2] = {0, 0}; ///< submitted last frame per eye
    int eyes = 0;           ///< eyes drawn last frame (0 without 3D)
    int meshes = 0;
    std::size_t mesh_bytes = 0;
};

/// A pixel to read back (request_probes).
struct Probe {
    std::int16_t x = 0;
    std::int16_t y = 0;
};

namespace gfx3d {

/// Turns `camera` to look at `target` (look_rotation of target − position).
void point_at(Camera3D& camera, Vec3 target, Vec3 up = {0, 1, 0});
/// The camera of `screen` for this frame's draws (used when the frame ends). An invalid camera (not finite, bad
/// planes or field of view, zero rotation) asserts and keeps the previous one.
void set_camera(Screen screen, const Camera3D& camera);
Camera3D camera(Screen screen);
/// The screen's lights (count clamped to 0..4, asserts above 4).
void set_lighting(Screen screen, const Lighting& lighting);
Lighting lighting(Screen screen);
/// The 2D layer the 3D world is drawn at (0..16, default 8): layers below it are drawn before the 3D, the rest after.
void set_world_layer(Screen screen, int layer);
int world_layer(Screen screen);

/// Records a draw of `mesh` with `model` (an affine matrix: bottom row 0, 0, 0, 1). Inside a frame, with 3D on, on
/// the top screen. Misuse (outside a frame, 3D off, a stale mesh, a submesh out of range, a non-finite or non-affine
/// matrix, an invalid material) asserts in Debug/Development and records nothing in Release. Full: dropped, counted,
/// asserts. No allocation.
void draw(Screen screen, const Mesh& mesh, const Mat4& model, const DrawOptions3D& options = {});
void draw(Screen screen, const Mesh& mesh, const Transform3D& transform, const DrawOptions3D& options = {});

/// Where `world_point` lands with the screen's current camera (pure math, the same bits everywhere).
ScreenPoint to_screen(Screen screen, Vec3 world_point);
/// The world ray under a screen pixel (origin on the camera, unit direction).
Ray3 screen_ray(Screen screen, Vec2 pixel);
/// The same for any camera on the top screen (400×240): usable without init (tools, tests, self-checks).
ScreenPoint to_screen(const Camera3D& camera, Vec3 world_point);
Ray3 screen_ray(const Camera3D& camera, Vec2 pixel);

/// Last frame's numbers (FR-28).
Stats3D stats();
/// Debug/Development: draws each draw's bounds and the world axes as 2D lines on layer 15. Release: nothing.
void set_debug_draw(bool on);
/// Debug/Development: asks for up to 32 pixels of `eye` after this frame is drawn; read them with probe_results() from
/// the next frame. @return false (nothing asked) in Release or when the request is invalid.
bool request_probes(Screen screen, Eye eye, const Probe* points, int count);
/// The colours of the last answered request; @return how many (0 before an answer).
int probe_results(Color* out, int capacity);

} // namespace gfx3d
} // namespace nnh
