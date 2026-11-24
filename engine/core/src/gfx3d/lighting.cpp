// Lights (nnh/gfx3d.hpp, FR-11, plan P10): set by value in world space, converted to view space per eye for the
// platform. -ffp-contract=off.

#include "gfx3d/pass3d.hpp"
#include "nnh/assert.hpp"

namespace nnh {
namespace detail {

void lights_to_view(const Lighting& l, const Mat4& view, platform::Pass3D& pass) {
    pass.ambient[0] = l.ambient.r / 255.0f;
    pass.ambient[1] = l.ambient.g / 255.0f;
    pass.ambient[2] = l.ambient.b / 255.0f;
    pass.light_count = l.count < 0 ? 0 : (l.count > 4 ? 4 : l.count);
    for (int i = 0; i < pass.light_count; ++i) {
        const Light3D& w = l.lights[i];
        platform::Light3DGpu& g = pass.lights[i];
        const bool point = w.kind == LightKind::Point;
        g.kind = point ? 1 : 0;
        Vec3 v = point ? transform_point(view, w.vector) : transform_direction(view, w.vector);
        if (!point) {
            const float l2 = dot(v, v);
            v = l2 > 1e-20f ? normalize(v) : Vec3{0, 0, 1};
        }
        g.vector[0] = v.x, g.vector[1] = v.y, g.vector[2] = v.z;
        g.color[0] = w.color.r / 255.0f, g.color[1] = w.color.g / 255.0f, g.color[2] = w.color.b / 255.0f;
        g.inv_range = point && w.range > 1e-6f ? 1.0f / w.range : 0.0f;
    }
}

} // namespace detail

namespace gfx3d {

void set_lighting(Screen screen, const Lighting& lighting) {
    NNH_ASSERT(lighting.count >= 0 && lighting.count <= 4, "gfx3d::set_lighting(): %d lights (0..4)", lighting.count);
    Lighting l = lighting;
    l.count = l.count < 0 ? 0 : (l.count > 4 ? 4 : l.count);
    detail::state3d().lighting[screen == Screen::Top ? 0 : 1] = l;
}

Lighting lighting(Screen screen) { return detail::state3d().lighting[screen == Screen::Top ? 0 : 1]; }

} // namespace gfx3d
} // namespace nnh
