// Per-eye cameras (pass3d.hpp make_eyes, spec 026 FR-14, plan P12). An eye at `d` along the camera's right sees the
// world through view_e = T(−d) × view (one subtraction on the view's x translation) and an off-axis projection: the
// mono perspective with P[0][2] = −P[0][0] × d / focal, so a point on the focal plane (view z = −focal) lands on the
// same NDC x in both eyes; nearer points cross (in front of the screen), farther ones don't (behind it).
// -ffp-contract=off: the same bits on PC and 3DS.

#include "gfx3d/pass3d.hpp"

namespace nnh::detail {

Eyes make_eyes(const Camera3D& c, float slider) {
    Eyes e;
    e.eye[0].view = camera_view(c);
    e.eye[0].projection = camera_projection(c);
    const float strength = c.stereo.strength > 0.0f ? c.stereo.strength : c.stereo.focal_distance / 20.0f;
    const float half = slider * strength * 0.5f;
    if (!(slider > 0.0f) || !c.stereo.enabled || c.projection != Projection::Perspective || !(half > 0.0f)) return e;
    e.count = 2;
    e.eye[1] = e.eye[0];
    for (int i = 0; i < 2; ++i) {
        EyeView& v = e.eye[i];
        v.offset = i == 0 ? -half : half;
        v.view.m[12] = v.view.m[12] - v.offset;
        v.projection.m[8] = -(v.projection.m[0] * v.offset) / c.stereo.focal_distance;
    }
    return e;
}

} // namespace nnh::detail
