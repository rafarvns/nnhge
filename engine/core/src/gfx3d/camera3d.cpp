// 3D cameras and the world layer (nnh/gfx3d.hpp, FR-12/FR-18): validation, view (rigid inverse of the camera's
// placement) and projection matrices for the top screen's 400:240 aspect. -ffp-contract=off.

#include <bit>

#include "gfx3d/gfx3d_state.hpp"
#include "nnh/assert.hpp"

namespace nnh {
namespace detail {
namespace {

constexpr float kTopAspect = 400.0f / 240.0f;

bool finite(float f) { return (std::bit_cast<std::uint32_t>(f) & 0x7F800000u) != 0x7F800000u; }

int screen_index(Screen s) { return s == Screen::Top ? 0 : 1; }

} // namespace

bool camera_valid(const Camera3D& c) {
    const Quat q = c.rotation;
    const float n = ((q.x * q.x + q.y * q.y) + q.z * q.z) + q.w * q.w;
    const bool finite_all = finite(c.position.x) && finite(c.position.y) && finite(c.position.z) && finite(n) &&
                            finite(c.near_plane) && finite(c.far_plane) && finite(c.fov_y) && finite(c.ortho_height);
    return finite_all && n > 1e-20f && c.near_plane > 1e-6f && c.far_plane > c.near_plane &&
           (c.projection == Projection::Orthographic ? c.ortho_height > 1e-6f
                                                     : (c.fov_y > 1e-4f && c.fov_y < 3.1414f));
}

Mat4 camera_view(const Camera3D& c) {
    const Mat4 r = to_mat4(c.rotation); // columns: the camera's axes in the world
    Mat4 v;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) v.m[col * 4 + row] = r.m[row * 4 + col]; // transpose
    const Vec3 x{r.m[0], r.m[1], r.m[2]}, y{r.m[4], r.m[5], r.m[6]}, z{r.m[8], r.m[9], r.m[10]};
    v.m[12] = -dot(x, c.position);
    v.m[13] = -dot(y, c.position);
    v.m[14] = -dot(z, c.position);
    return v;
}

Mat4 camera_projection(const Camera3D& c) {
    return c.projection == Projection::Orthographic
               ? orthographic(c.ortho_height, kTopAspect, c.near_plane, c.far_plane)
               : perspective(c.fov_y, kTopAspect, c.near_plane, c.far_plane);
}

} // namespace detail

namespace gfx3d {

void point_at(Camera3D& camera, Vec3 target, Vec3 up) { camera.rotation = look_rotation(target - camera.position, up); }

void set_camera(Screen screen, const Camera3D& camera) {
    if (!detail::camera_valid(camera)) {
        NNH_ASSERT(false, "gfx3d::set_camera(): an invalid camera (planes, field of view, rotation or not finite)");
        return;
    }
    detail::state3d().cameras[detail::screen_index(screen)] = camera;
}

Camera3D camera(Screen screen) { return detail::state3d().cameras[detail::screen_index(screen)]; }

void set_world_layer(Screen screen, int layer) {
    NNH_ASSERT(layer >= 0 && layer <= 16, "gfx3d::set_world_layer(%d): 0..16", layer);
    detail::state3d().world_layer[detail::screen_index(screen)] = layer < 0 ? 0 : (layer > 16 ? 16 : layer);
}

int world_layer(Screen screen) { return detail::state3d().world_layer[detail::screen_index(screen)]; }

} // namespace gfx3d
} // namespace nnh
