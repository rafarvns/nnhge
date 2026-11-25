// The stereo self-check (self_check.hpp Script::Stereo, spec 026 AC-2, T12.3): seeded cameras with seeded stereo
// settings and slider values; each operation folds where a point lands in both eyes (gfx3d::to_screen per eye) or
// whether a sphere is kept by the culling union (gfx3d::in_view) into the checksum. Public, pure API only, so the
// console prints the same golden.

#include "nnh/gfx3d.hpp"
#include "self_check.hpp"

namespace models {

void SelfCheck::one_stereo() {
    using namespace nnh;
    const std::uint32_t kind = hash(0) % 4u;
    Camera3D cam;
    cam.position = Vec3{value(1, 4), value(2, 4) + 1.0f, 6.0f + value(3, 2)};
    gfx3d::point_at(cam, Vec3{value(4, 1), value(5, 1), value(6, 1)});
    cam.fov_y = 0.8f + value(7, 0.25f) + 0.25f;
    cam.stereo.focal_distance = 3.0f + value(8, 2) + 2.0f;
    cam.stereo.strength = (hash(9) & 1u) != 0 ? 0.0f : value(10, 0.5f) + 0.5f;
    if (kind == 3) {
        if ((hash(11) & 1u) != 0) cam.projection = Projection::Orthographic;
        else cam.stereo.enabled = false;
    }
    const float slider = (hash(12) & 3u) == 0 ? 0.0f : value(13, 0.5f) + 0.5f;
    const Vec3 p{value(14, 8), value(15, 4), value(16, 8)};
    if (kind == 2) { // the culling union
        const Sphere s{p, value(17, 0.5f) + 0.5f};
        mix(static_cast<std::uint32_t>(gfx3d::in_view(cam, s, slider)) |
            (static_cast<std::uint32_t>(gfx3d::in_view(cam, s, 0.0f)) << 1));
        return;
    }
    for (const Eye eye : {Eye::Left, Eye::Right}) {
        const ScreenPoint e = gfx3d::to_screen(cam, p, eye, slider);
        mix(e.pixel.x), mix(e.pixel.y), mix(e.depth);
        mix(static_cast<std::uint32_t>(e.in_front) | (static_cast<std::uint32_t>(e.in_view) << 1));
    }
}

} // namespace models
