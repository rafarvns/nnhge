#pragma once
/// @file nnh/transform.hpp
/// 2D transforms as plain values (spec 022, decision D37): a local Transform2D (position, rotation, scale), the affine
/// map of a world transform and its drawing values, and the math between them — composition, points, directions,
/// inverses, keep-world reparenting, axis-aligned bounds — with the engine's deterministic sine and cosine. The same
/// inputs give the same bits on PC and 3DS. Usable without a world: a game with its own arrays composes its own
/// structs. The hierarchy (<nnh/hierarchy.hpp>) builds on these values. See docs/api/transform.md.
///
/// Light: only <nnh/math.hpp>, declarations only. Every float operation lives in src/transform/*.cpp, compiled with
/// -ffp-contract=off (inline math here would be compiled with the game's flags). Nothing allocates; no libm.
///
/// @code
/// const nnh::Transform2D ship{.position = {100, 60}, .rotation = nnh::kQuarterTurn};
/// const nnh::Transform2D gun{.position = {12, -4}};             // relative to the ship
/// const nnh::WorldTransform2D ship_w = nnh::to_world(ship);
/// const nnh::WorldTransform2D gun_w = nnh::compose(ship_w, gun);  // (104, 72): turned with the ship
/// const nnh::Vec2 muzzle = nnh::transform_point(gun_w.map, {10, 0}); // where a bullet starts
/// @endcode

#include "nnh/math.hpp"

namespace nnh {

/// A quarter turn in radians, float(π/2): sin_cos gives exactly (1, 0). Angles are clockwise on the y-down screen,
/// as DrawOptions::rotation.
inline constexpr float kQuarterTurn = 1.57079637f;
inline constexpr float kHalfTurn = 2.0f * kQuarterTurn;         ///< exact doubling: sin_cos gives (0, −1)
inline constexpr float kThreeQuarterTurn = 3.0f * kQuarterTurn; ///< rounded once: (−1, 0)
inline constexpr float kFullTurn = 4.0f * kQuarterTurn;         ///< exact: (0, 1)

/// A sine and a cosine.
struct SinCos {
    float sin = 0;
    float cos = 1;
};

/// The engine's sine and cosine of `radians`: IEEE + − × and float↔int conversions only (spec 015's polynomial), so the
/// same bits on every system and compiler. Exactly 0 and ±1 at the quarter-turn constants and their small float
/// multiples (an angle within 2⁻²⁰ quarter turn of a whole quarter snaps to it); error < 2·10⁻⁶ elsewhere. A
/// non-finite angle gives {0, 1} (asserts in Debug/Development); huge angles have defined bits. No allocation.
SinCos sin_cos(float radians);

/// A local transform: scaled, then rotated (clockwise on screen), then moved, about its own origin. A negative scale
/// component mirrors on that axis. The default is the identity. 20 bytes, trivially copyable.
struct Transform2D {
    Vec2 position{};
    float rotation = 0; ///< radians, clockwise on the y-down screen
    Vec2 scale{1, 1};
};

/// An affine map: the images of the unit x and y axes and of the origin (Godot's Transform2D). Exact for points and
/// directions under any chain, sheared ones included. 24 bytes.
struct Affine2D {
    Vec2 x{1, 0};
    Vec2 y{0, 1};
    Vec2 origin{};
};

/// A world transform: the affine map (points, directions) and the drawing values — an accumulated rotation and a
/// product of scales (Unity's lossyScale) — which are exact when the chain has no shear (every non-uniform scale sits on
/// a node whose descendants aren't rotated relative to it, or it is a mirror). 36 bytes.
struct WorldTransform2D {
    Affine2D map{};
    float rotation = 0; ///< accumulated; a mirrored parent turns its children the other way
    Vec2 scale{1, 1};   ///< product of scales; a negative component = mirrored
    constexpr Vec2 position() const { return map.origin; }
};

/// A root's world transform: the drawing values are `root`'s bits, the map's origin its position (FR-3).
WorldTransform2D to_world(const Transform2D& root);
/// The world transform of a child with local transform `local` under a parent whose world transform is `parent`. A
/// local with rotation +0 and scale exactly (1, 1) is a pure translation: the parent's axes and drawing values are
/// kept and only the origin is computed (a child of an unmoved parent sits at exactly parent + local position).
WorldTransform2D compose(const WorldTransform2D& parent, const Transform2D& local);
/// A local point (a muzzle, a box corner) in world space.
Vec2 transform_point(const Affine2D& map, Vec2 local_point);
/// A local direction (a velocity) in world space: no translation.
Vec2 transform_direction(const Affine2D& map, Vec2 local_direction);
/// The inverse map. False (`out` untouched) when |determinant| < 2⁻¹⁰⁰ (a zero scale in the chain) or the map isn't
/// finite: an exponent test, the same on every system.
bool inverse(const Affine2D& map, Affine2D& out);
/// A world point in the map's local space. False (`out` untouched) when the map can't be inverted.
bool to_local(const Affine2D& map, Vec2 world_point, Vec2& out);
/// The local transform that puts a node at `world` under `parent` (keep-world reparenting): the position within 1/32
/// px, the rotation and scale from the drawing values (exact without shear). False when `parent` can't be inverted.
bool relative(const WorldTransform2D& parent, const WorldTransform2D& world, Transform2D& out);
/// The axis-aligned box of rectangle `local` under `map` (FR-14): unrotated maps give the mapped rectangle (mirrored
/// ones normalised to a positive size), rotated ones the box enclosing the four mapped corners.
Rect bounds(const Affine2D& map, Rect local);
/// False when any of `t`'s five floats is infinite or NaN (an integer test on the bits).
bool is_finite(const Transform2D& t);

} // namespace nnh
