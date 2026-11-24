// 3D vectors (spec 026 FR-1): dot, cross, length, normalise. Fixed operation order; -ffp-contract=off.

#include "math3d/math3d_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {

float dot(Vec3 a, Vec3 b) {
    const float xy = a.x * b.x + a.y * b.y;
    return xy + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float length_squared(Vec3 v) { return dot(v, v); }

float length(Vec3 v) { return detail::sqrt_ieee(dot(v, v)); }

Vec3 normalize(Vec3 v) {
    const float l2 = dot(v, v);
    if (!detail::usable(l2)) {
        NNH_ASSERT(false, "nnh::normalize(): a zero, tiny or non-finite vector");
        return {};
    }
    const float l = detail::sqrt_ieee(l2);
    return {v.x / l, v.y / l, v.z / l};
}

} // namespace nnh
