// Screen and world (nnh/gfx3d.hpp, FR-13): where a world point lands on the top screen (centre eye) and the world ray
// under a pixel. Pure math with the camera's view and projection: the same bits everywhere. -ffp-contract=off.

#include "gfx3d/gfx3d_state.hpp"

namespace nnh::gfx3d {
namespace {

constexpr float kWidth = 400.0f, kHeight = 240.0f;

} // namespace

ScreenPoint to_screen(const Camera3D& camera, Vec3 world_point) {
    ScreenPoint s;
    if (!detail::camera_valid(camera)) return s;
    const Vec3 v = transform_point(detail::camera_view(camera), world_point);
    const Vec4 c = detail::camera_projection(camera) * Vec4{v.x, v.y, v.z, 1};
    s.depth = -v.z;
    s.in_front = s.depth >= camera.near_plane;
    if (!s.in_front) return s;
    const float nx = c.x / c.w, ny = c.y / c.w;
    s.pixel = {(nx + 1.0f) * (0.5f * kWidth), (1.0f - ny) * (0.5f * kHeight)};
    s.in_view = s.depth <= camera.far_plane && nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f;
    return s;
}

Ray3 screen_ray(const Camera3D& camera, Vec2 pixel) {
    if (!detail::camera_valid(camera)) return {};
    const float nx = pixel.x * (2.0f / kWidth) - 1.0f, ny = 1.0f - pixel.y * (2.0f / kHeight);
    const Mat4 r = to_mat4(camera.rotation);
    const float aspect = kWidth / kHeight;
    if (camera.projection == Projection::Orthographic) {
        const float hh = camera.ortho_height * 0.5f;
        const Vec3 offset = transform_direction(r, {nx * hh * aspect, ny * hh, 0});
        return {camera.position + offset, transform_direction(r, {0, 0, -1})};
    }
    const SinCos h = sin_cos(camera.fov_y * 0.5f);
    const float t = h.sin / h.cos;
    const Vec3 d = transform_direction(r, {nx * t * aspect, ny * t, -1});
    return {camera.position, normalize(d)};
}

ScreenPoint to_screen(Screen screen, Vec3 world_point) {
    return to_screen(detail::state3d().cameras[screen == Screen::Top ? 0 : 1], world_point);
}

Ray3 screen_ray(Screen screen, Vec2 pixel) {
    return screen_ray(detail::state3d().cameras[screen == Screen::Top ? 0 : 1], pixel);
}

} // namespace nnh::gfx3d
