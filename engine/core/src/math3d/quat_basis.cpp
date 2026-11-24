// Orientation from directions (spec 026 FR-1): the look basis, look_rotation by Shepperd's method (one square root,
// branch on the largest of the trace and the diagonal), and the eighth-turn snap. -ffp-contract=off.

#include "math3d/math3d_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {

SinCos detail::snap_eighth(SinCos sc) {
    const float s = sign_bit(sc.sin) ? -sc.sin : sc.sin;
    const float c = sign_bit(sc.cos) ? -sc.cos : sc.cos;
    const float d = s - c;
    if (d > 9.5367432e-7f || d < -9.5367432e-7f) return sc; // 2⁻²⁰
    const float m = s > c ? s : c;
    return {sign_bit(sc.sin) ? -m : m, sign_bit(sc.cos) ? -m : m};
}

bool detail::look_basis(Vec3 forward, Vec3 up, Vec3& x, Vec3& y, Vec3& z) {
    if (!usable(dot(forward, forward))) return false;
    z = normalize(-forward);
    Vec3 side = cross(up, z);
    if (!usable(dot(side, side))) side = cross(Vec3{0, 0, sign_bit(z.y) ? 1.0f : -1.0f}, z); // parallel: north up
    x = normalize(side);
    y = cross(z, x);
    return true;
}

Quat look_rotation(Vec3 forward, Vec3 up) {
    Vec3 x, y, z;
    if (!detail::look_basis(forward, up, x, y, z)) {
        NNH_ASSERT(false, "nnh::look_rotation(): a zero or non-finite forward");
        return {};
    }
    // The rotation matrix has columns x, y, z: m[row][col] with m00 = x.x, m10 = x.y, m01 = y.x, …
    const float trace = (x.x + y.y) + z.z;
    Quat q;
    if (trace >= x.x && trace >= y.y && trace >= z.z) {
        const float r = detail::sqrt_ieee(1.0f + trace);
        const float f = 0.5f / r;
        q = {(y.z - z.y) * f, (z.x - x.z) * f, (x.y - y.x) * f, r * 0.5f};
    } else if (x.x >= y.y && x.x >= z.z) {
        const float r = detail::sqrt_ieee(((1.0f + x.x) - y.y) - z.z);
        const float f = 0.5f / r;
        q = {r * 0.5f, (y.x + x.y) * f, (z.x + x.z) * f, (y.z - z.y) * f};
    } else if (y.y >= z.z) {
        const float r = detail::sqrt_ieee(((1.0f + y.y) - x.x) - z.z);
        const float f = 0.5f / r;
        q = {(y.x + x.y) * f, r * 0.5f, (z.y + y.z) * f, (z.x - x.z) * f};
    } else {
        const float r = detail::sqrt_ieee(((1.0f + z.z) - x.x) - y.y);
        const float f = 0.5f / r;
        q = {(z.x + x.z) * f, (z.y + y.z) * f, r * 0.5f, (x.y - y.x) * f};
    }
    return q;
}

} // namespace nnh
