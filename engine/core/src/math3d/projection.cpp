// View and projection matrices (spec 026 FR-1/FR-2): look-at from the look basis, perspective with tan = sin/cos of
// sin_cos (eighth turns snapped, so 90° is exact), orthographic. OpenGL clip space. -ffp-contract=off.

#include "math3d/math3d_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {

Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 x, y, z;
    Mat4 v;
    if (!detail::look_basis(target - eye, up, x, y, z)) {
        NNH_ASSERT(false, "nnh::look_at(): the eye is on the target");
        return v;
    }
    v.m[0] = x.x, v.m[4] = x.y, v.m[8] = x.z;
    v.m[1] = y.x, v.m[5] = y.y, v.m[9] = y.z;
    v.m[2] = z.x, v.m[6] = z.y, v.m[10] = z.z;
    v.m[12] = -dot(x, eye);
    v.m[13] = -dot(y, eye);
    v.m[14] = -dot(z, eye);
    return v;
}

Mat4 perspective(float fov_y, float aspect, float near, float far) {
    Mat4 p;
    // Positive comparisons are false for NaN: every invalid value fails one of them.
    const bool ok = fov_y > 1e-4f && fov_y < 3.1414f && aspect > 1e-6f && aspect < 1e6f && near > 1e-6f &&
                    far > near && detail::finite(far) && detail::usable(far - near);
    if (!ok) {
        NNH_ASSERT(false, "nnh::perspective(): needs 0 < fov_y < pi, aspect > 0, 0 < near < far");
        return p;
    }
    const SinCos h = detail::snap_eighth(sin_cos(fov_y * 0.5f));
    const float f = h.cos / h.sin;
    const float d = near - far;
    p.m[0] = f / aspect;
    p.m[5] = f;
    p.m[10] = (far + near) / d;
    p.m[11] = -1.0f;
    p.m[14] = 2.0f * far * near / d;
    p.m[15] = 0.0f;
    return p;
}

Mat4 orthographic(float height, float aspect, float near, float far) {
    Mat4 o;
    const bool ok = height > 1e-6f && aspect > 1e-6f && aspect < 1e6f && detail::finite(height * aspect) &&
                    far > near && detail::finite(near) && detail::finite(far) && detail::usable(far - near);
    if (!ok) {
        NNH_ASSERT(false, "nnh::orthographic(): needs height > 0, aspect > 0, near < far");
        return o;
    }
    const float d = far - near;
    o.m[0] = 2.0f / (height * aspect);
    o.m[5] = 2.0f / height;
    o.m[10] = -2.0f / d;
    o.m[14] = -(far + near) / d;
    return o;
}

} // namespace nnh
