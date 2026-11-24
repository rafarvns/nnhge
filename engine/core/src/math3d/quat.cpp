// Quaternions (spec 026 FR-1/FR-3, plan P4): axis-angle with the eighth-turn snap, Euler order, product, conjugate,
// normalise, and the matrix as numerators over |q|² so quarter turns give exactly 0 and ±1. -ffp-contract=off.

#include "math3d/math3d_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {

Quat quat_axis_angle(Vec3 axis, float radians) {
    const float l2 = dot(axis, axis);
    if (!detail::usable(l2)) {
        NNH_ASSERT(false, "nnh::quat_axis_angle(): a zero, tiny or non-finite axis");
        return {};
    }
    const float l = detail::sqrt_ieee(l2);
    const SinCos h = detail::snap_eighth(sin_cos(radians * 0.5f));
    return {axis.x / l * h.sin, axis.y / l * h.sin, axis.z / l * h.sin, h.cos};
}

Quat quat_euler(float yaw, float pitch, float roll) {
    return quat_axis_angle({0, 1, 0}, yaw) * quat_axis_angle({1, 0, 0}, pitch) * quat_axis_angle({0, 0, 1}, roll);
}

Quat operator*(Quat a, Quat b) {
    Quat r;
    r.w = ((a.w * b.w - a.x * b.x) - a.y * b.y) - a.z * b.z;
    r.x = ((a.w * b.x + a.x * b.w) + a.y * b.z) - a.z * b.y;
    r.y = ((a.w * b.y - a.x * b.z) + a.y * b.w) + a.z * b.x;
    r.z = ((a.w * b.z + a.x * b.y) - a.y * b.x) + a.z * b.w;
    return r;
}

Quat conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }

namespace {

float norm2(Quat q) {
    const float xy = q.x * q.x + q.y * q.y;
    const float xyz = xy + q.z * q.z;
    return xyz + q.w * q.w;
}

} // namespace

Quat normalize(Quat q) {
    const float n2 = norm2(q);
    if (!detail::usable(n2)) {
        NNH_ASSERT(false, "nnh::normalize(): a zero, tiny or non-finite quaternion");
        return {};
    }
    const float n = detail::sqrt_ieee(n2);
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}

Mat4 to_mat4(Quat q) {
    const float n = norm2(q);
    Mat4 m;
    if (!detail::usable(n)) return m; // a zero quaternion: the identity
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, ww = q.w * q.w;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    m.m[0] = ((ww + xx) - yy - zz) / n;
    m.m[1] = 2.0f * (xy + wz) / n;
    m.m[2] = 2.0f * (xz - wy) / n;
    m.m[4] = 2.0f * (xy - wz) / n;
    m.m[5] = ((ww - xx) + yy - zz) / n;
    m.m[6] = 2.0f * (yz + wx) / n;
    m.m[8] = 2.0f * (xz + wy) / n;
    m.m[9] = 2.0f * (yz - wx) / n;
    m.m[10] = ((ww - xx) - yy + zz) / n;
    return m;
}

Vec3 rotate(Quat q, Vec3 v) { return transform_direction(to_mat4(q), v); }

} // namespace nnh
