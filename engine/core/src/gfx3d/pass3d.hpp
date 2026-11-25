#pragma once
// The 3D pass's pieces (spec 026): lights in view space, culling, order, draws for the platform. Internal; pure
// functions of their inputs (tested in tests/gfx3d/).

#include <cstdint>

#include "gfx3d/gfx3d_state.hpp"
#include "nnh/platform/video3d.hpp"

namespace nnh::detail {

/// eye_setup.cpp (FR-14, plan P12): the pictures a frame draws for a camera at a slider value (0..1). One eye — the
/// camera's own matrices, bit for bit — when the slider is 0, the camera is orthographic or its stereo is off; else
/// two, moved by ∓separation/2 along the camera's right (separation = slider × strength, strength 0 = focal / 20),
/// each with the camera's perspective made off-axis so the focal plane lands on the same pixels in both eyes.
struct EyeView {
    Mat4 view;
    Mat4 projection;
    float offset = 0; ///< along the camera's right, world units (left eye negative)
};
struct Eyes {
    int count = 1;
    EyeView eye[2];
};
Eyes make_eyes(const Camera3D& camera, float slider);

/// lighting.cpp: ambient and lights (directions normalised, positions moved) into the pass, in view space.
void lights_to_view(const Lighting& lighting, const Mat4& view, platform::Pass3D& pass);

/// cull.cpp: a view-space frustum of a camera; a sphere (view-space centre) inside it or straddling it.
struct Frustum {
    bool ortho = false;
    float near_plane = 0, far_plane = 0;
    float tx = 0, ty = 0; ///< perspective: tan(half fov) × aspect, tan(half fov); ortho: half width, half height
    float kx = 1, ky = 1; ///< perspective: √(1 + tx²), √(1 + ty²)
    // An eye's off-axis frustum (stereo): x − ahead·sx lies within ±ahead·tx; the side planes' √(1 + slope²).
    float sx = 0;         ///< 0 for the centre camera (then the symmetric test, bit for bit)
    float kl = 1, kr = 1;
};
Frustum make_frustum(const Camera3D& camera);
bool sphere_visible(const Frustum& f, Vec3 view_center, float radius);

/// The frame's culling (FR-20, plan P11): the centre camera's frustum in mono; in stereo each eye's off-axis frustum,
/// a sphere kept when either eye sees it. Spheres are given in the centre camera's view space.
struct EyeCull {
    int count = 1;
    Frustum frustum[2];
    float offset[2] = {0, 0}; ///< each eye's x in the centre view
};
EyeCull make_eye_cull(const Camera3D& camera, const Eyes& eyes);
bool sphere_in_eyes(const EyeCull& cull, Vec3 view_center, float radius);

/// order.cpp: writes the submission order of `count` draws: every visible draw in call order into `opaque` (its
/// opaque and cut-out parts), then the blended ones (`blended[i]`) sorted back to front by `depth` (view z, most
/// negative first), ties in call order. @return the number of blended entries written after `opaque_count`.
struct Order {
    int opaque = 0;  ///< order[0, opaque): call order
    int blended = 0; ///< order[opaque, opaque + blended): back to front
};
Order order_draws(const bool* visible, const bool* blended, const float* depth, int count, std::uint16_t* order,
                  std::uint16_t* scratch);

/// submit3d.cpp: the platform draw of `submesh` of a record: model-view with the quantisation decode folded in,
/// normal matrix, uv decode, material resolved (override or the submesh's, tint, texture handle). @return false when
/// the mesh or texture is gone (the draw is skipped; a stale texture draws untextured instead).
struct MeshRecord;
bool make_draw(const DrawRecord& record, const MeshRecord& mesh, int submesh, const Mat4& view, platform::Draw3D& out);
/// The material a record draws `submesh` with.
const Material3D& draw_material(const DrawRecord& record, const MeshRecord& mesh, int submesh);

} // namespace nnh::detail
