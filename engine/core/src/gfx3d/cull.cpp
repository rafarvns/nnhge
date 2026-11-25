// Frustum culling (pass3d.hpp, FR-20, plan P11): spheres in view space against the near, far and four side planes.
// Exact plane distances (tan from sin_cos, one sqrt per side at frame start); a straddling sphere is kept.
// -ffp-contract=off.

#include "gfx3d/pass3d.hpp"

namespace nnh::detail {

Frustum make_frustum(const Camera3D& c) {
    Frustum f;
    f.ortho = c.projection == Projection::Orthographic;
    f.near_plane = c.near_plane;
    f.far_plane = c.far_plane;
    const float aspect = 400.0f / 240.0f;
    if (f.ortho) {
        f.ty = c.ortho_height * 0.5f;
        f.tx = f.ty * aspect;
    } else {
        const SinCos h = sin_cos(c.fov_y * 0.5f);
        f.ty = h.sin / h.cos;
        f.tx = f.ty * aspect;
        f.kx = __builtin_sqrtf(1.0f + f.tx * f.tx);
        f.ky = __builtin_sqrtf(1.0f + f.ty * f.ty);
    }
    return f;
}

bool sphere_visible(const Frustum& f, Vec3 p, float r) {
    const float ahead = -p.z; // distance in front of the camera
    if (ahead + r < f.near_plane || ahead - r > f.far_plane) return false;
    if (f.ortho) {
        const float ax = p.x < 0 ? -p.x : p.x, ay = p.y < 0 ? -p.y : p.y;
        return ax - r <= f.tx && ay - r <= f.ty;
    }
    const float ay = p.y < 0 ? -p.y : p.y;
    if (f.sx != 0.0f) { // an eye's off-axis frustum: x − ahead·sx within ±ahead·tx, each side plane with its slope
        const float dx = p.x - ahead * f.sx;
        return dx - ahead * f.tx <= r * f.kr && -dx - ahead * f.tx <= r * f.kl && ay - ahead * f.ty <= r * f.ky;
    }
    // Side planes through the eye: a point is inside when |x| ≤ ahead · tx (and the same for y).
    const float ax = p.x < 0 ? -p.x : p.x;
    return ax - ahead * f.tx <= r * f.kx && ay - ahead * f.ty <= r * f.ky;
}

EyeCull make_eye_cull(const Camera3D& c, const Eyes& eyes) {
    EyeCull cull;
    cull.count = eyes.count;
    cull.frustum[0] = cull.frustum[1] = make_frustum(c);
    if (eyes.count < 2) return cull;
    for (int i = 0; i < 2; ++i) {
        Frustum& f = cull.frustum[i];
        cull.offset[i] = eyes.eye[i].offset;
        f.sx = -eyes.eye[i].offset / c.stereo.focal_distance; // the projection's shift: P[0][2] / P[0][0]
        const float right = f.tx + f.sx, left = f.tx - f.sx;
        f.kr = __builtin_sqrtf(1.0f + right * right);
        f.kl = __builtin_sqrtf(1.0f + left * left);
    }
    return cull;
}

bool sphere_in_eyes(const EyeCull& cull, Vec3 p, float r) {
    if (cull.count < 2) return sphere_visible(cull.frustum[0], p, r);
    return sphere_visible(cull.frustum[0], {p.x - cull.offset[0], p.y, p.z}, r) ||
           sphere_visible(cull.frustum[1], {p.x - cull.offset[1], p.y, p.z}, r);
}

} // namespace nnh::detail
