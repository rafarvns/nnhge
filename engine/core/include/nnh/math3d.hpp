#pragma once
/// @file nnh/math3d.hpp
/// 3D math as plain values (spec 026, decision D44): vectors, quaternions, 4×4 matrices, 3D transforms, rays, look-at
/// and projections. Usable without graphics. See docs/api/math3d.md.
///
/// Conventions: right-handed, +y up, a camera looks down its −z (glTF and OpenGL); angles in radians; matrices act on
/// column vectors (M × v) and are stored column-major (`m[column * 4 + row]`); clip space as OpenGL (depth −1..1).
///
/// Determinism: every operation uses only IEEE + − × ÷, a correctly rounded square root and nnh::sin_cos, in a fixed
/// order, compiled with -ffp-contract=off (src/math3d/). The same inputs give the same bits on PC and 3DS for values
/// whose magnitudes are 0 or within 2⁻⁶⁰…2⁶⁰. Lengths, determinants and norms below 2⁻¹⁰⁰ are "zero" (an exponent test,
/// never a subnormal compare). Exact: identities; quarter turns (the engine's kQuarterTurn constants) map axes to axes;
/// a translation-only transform moves a point by one addition per axis. Nothing allocates; no libm.
///
/// Light: only <nnh/math.hpp> and <nnh/transform.hpp> (sin_cos). The inline operators below are one IEEE operation per
/// component, so the game's compiler flags can't fuse them.
///
/// @code
/// const nnh::Transform3D ship{.position = {0, 1, 0}, .rotation = nnh::quat_axis_angle({0, 1, 0}, nnh::kQuarterTurn)};
/// const nnh::Mat4 model = nnh::to_mat4(ship);
/// const nnh::Vec3 nose = nnh::transform_point(model, {0, 0, -1}); // (-1, 1, 0): turned left about +y
/// @endcode

#include "nnh/math.hpp"
#include "nnh/transform.hpp"

namespace nnh {

/// A 3D vector (a point or a direction). 12 bytes, trivially copyable.
struct Vec3 {
    float x = 0;
    float y = 0;
    float z = 0;

    constexpr Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3& operator-=(Vec3 o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    friend constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend constexpr Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
    friend constexpr Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
    friend constexpr Vec3 operator*(float s, Vec3 a) { return {s * a.x, s * a.y, s * a.z}; }
    friend constexpr Vec3 operator/(Vec3 a, float s) { return {a.x / s, a.y / s, a.z / s}; }
    friend constexpr bool operator==(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
};

/// A 4D vector (homogeneous coordinates). 16 bytes.
struct Vec4 {
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 0;
    friend constexpr bool operator==(Vec4 a, Vec4 b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
};

/// A rotation as a quaternion (x, y, z, w); the default is the identity. Need not be unit: to_mat4 divides by |q|².
struct Quat {
    float x = 0;
    float y = 0;
    float z = 0;
    float w = 1;
    friend constexpr bool operator==(Quat a, Quat b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }
};

/// A 4×4 matrix, column-major (`m[column * 4 + row]`), acting on column vectors. The default is the identity.
struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    static constexpr Mat4 identity() { return {}; }
    constexpr float at(int row, int column) const { return m[column * 4 + row]; }
    friend constexpr bool operator==(const Mat4& a, const Mat4& b) {
        for (int i = 0; i < 16; ++i)
            if (a.m[i] != b.m[i]) return false;
        return true;
    }
};

/// A local 3D transform: scaled, then rotated, then moved. 40 bytes; the default is the identity.
struct Transform3D {
    Vec3 position{};
    Quat rotation{};
    Vec3 scale{1, 1, 1};
};

/// A ray: an origin and a direction (unit length when made by the engine).
struct Ray3 {
    Vec3 origin{};
    Vec3 direction{0, 0, -1};
};

float dot(Vec3 a, Vec3 b);                 ///< (a.x·b.x + a.y·b.y) + a.z·b.z
Vec3 cross(Vec3 a, Vec3 b);                ///< right-handed: cross(x, y) = z
float length(Vec3 v);                      ///< √(dot(v, v))
float length_squared(Vec3 v);              ///< dot(v, v)
/// `v` with length 1, each component divided by the length (axes stay exact). A length² below 2⁻¹⁰⁰ or not finite
/// gives (0, 0, 0) and asserts in Debug/Development.
Vec3 normalize(Vec3 v);

/// A rotation of `radians` about `axis` (normalised here; a zero axis gives the identity and asserts). Exact quarter
/// turns: half-angle sines and cosines of equal magnitude are made bit-identical.
Quat quat_axis_angle(Vec3 axis, float radians);
/// Yaw about +y, then pitch about +x, then roll about +z, applied to the object as Y(yaw) × X(pitch) × Z(roll).
Quat quat_euler(float yaw, float pitch, float roll);
/// The rotation whose −z points along `forward` and whose +y is as close to `up` as possible (a camera or an object
/// looking somewhere). `forward` parallel to `up` picks an up of ∓z; a zero `forward` gives the identity (asserts).
Quat look_rotation(Vec3 forward, Vec3 up = {0, 1, 0});
Quat operator*(Quat a, Quat b);            ///< `a` after `b` (Hamilton product)
Quat conjugate(Quat q);                    ///< the inverse rotation of a unit quaternion
/// `q` with norm 1; a norm² below 2⁻¹⁰⁰ or not finite gives the identity and asserts in Debug/Development.
Quat normalize(Quat q);
Vec3 rotate(Quat q, Vec3 v);               ///< = the 3×3 part of to_mat4(q) applied to `v` (exact quarter turns)
/// Normalised linear interpolation along the shortest arc: t = 0 gives normalize(a).
Quat nlerp(Quat a, Quat b, float t);
/// Spherical interpolation of unit quaternions along the shortest arc (Eberly's polynomial: no libm, error < 2⁻¹⁸).
/// t = 0 gives `a` exactly, t = 1 gives `b` or −b (the same rotation) exactly.
Quat slerp(Quat a, Quat b, float t);

/// The rotation matrix of `q` (any norm; each entry is a numerator over |q|²). A zero quaternion gives the identity.
Mat4 to_mat4(Quat q);
/// T(position) × R(rotation) × S(scale).
Mat4 to_mat4(const Transform3D& t);
Mat4 operator*(const Mat4& a, const Mat4& b); ///< `a` after `b`
Mat4 transpose(const Mat4& m);
/// The general inverse (cofactors). False (`out` untouched) when |determinant| < 2⁻¹⁰⁰ or it isn't finite.
bool inverse(const Mat4& m, Mat4& out);
/// The inverse of an affine matrix (bottom row 0, 0, 0, 1: rigid, scaled or sheared): 3×3 inverse and translation.
/// Faster than inverse(). False (`out` untouched) when the 3×3 part can't be inverted (same rule).
bool inverse_affine(const Mat4& m, Mat4& out);
/// M × (p, 1), divided by the result's w when it isn't exactly 1 (a projection); w = 0 gives (0, 0, 0).
Vec3 transform_point(const Mat4& m, Vec3 p);
Vec3 transform_direction(const Mat4& m, Vec3 d); ///< M × (d, 0): no translation
Vec4 operator*(const Mat4& m, Vec4 v);
/// A view matrix (world → camera) for a camera at `eye` looking at `target`, `up` giving the vertical (the
/// look_rotation rules for degenerate cases).
Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up = {0, 1, 0});
/// A perspective projection to OpenGL clip space (depth −1 at `near`, +1 at `far`): `fov_y` the vertical field of view in
/// radians (0 < fov_y < π), `aspect` = width / height, 0 < near < far. tan(fov/2) = sin/cos of sin_cos. Invalid values
/// give the identity and assert in Debug/Development.
Mat4 perspective(float fov_y, float aspect, float near, float far);
/// An orthographic projection showing `height` world units vertically and `height × aspect` horizontally, centred on
/// the camera's axis, depth −1 at `near` and +1 at `far` (near < far; near may be negative). Invalid values give the
/// identity and assert.
Mat4 orthographic(float height, float aspect, float near, float far);
/// False when any entry is infinite or NaN (an integer test on the bits).
bool is_finite(const Mat4& m);

} // namespace nnh
