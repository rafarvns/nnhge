// The 3D debug draw (nnh/gfx3d.hpp set_debug_draw, FR-28, plan P18): each draw's box (its mesh's local bounds under
// its model matrix) and the world axes, projected with to_screen into 2D lines on layer 15 (screen pixels). Debug and
// Development only; Release compiles it to nothing.

#include "gfx3d/gfx3d_state.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/build.hpp"

namespace nnh {
namespace detail {
namespace {

void line(const Camera3D& cam, Vec3 a, Vec3 b, Color c) {
    const ScreenPoint p = gfx3d::to_screen(cam, a), q = gfx3d::to_screen(cam, b);
    if (p.in_front && q.in_front) gfx::draw_line(Screen::Top, p.pixel, q.pixel, c, {.layer = 15, .ignore_camera = true});
}

void box(const Camera3D& cam, const DrawRecord& r, const MeshRecord& mesh) {
    Mat4 m;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col) m.m[col * 4 + row] = r.model[row * 4 + col];
    Vec3 c[8];
    for (int i = 0; i < 8; ++i)
        c[i] = transform_point(m, {(i & 1) ? mesh.bounds.max.x : mesh.bounds.min.x,
                                   (i & 2) ? mesh.bounds.max.y : mesh.bounds.min.y,
                                   (i & 4) ? mesh.bounds.max.z : mesh.bounds.min.z});
    const Color yellow{255, 220, 0, 255};
    for (int i = 0; i < 8; ++i)
        for (int bit = 1; bit < 8; bit <<= 1)
            if ((i & bit) == 0) line(cam, c[i], c[i | bit], yellow);
}

} // namespace

void gfx3d_debug_lines(Screen) {
    if constexpr (!build::kDevToolsEnabled) return;
    const State3D& g = state3d();
    if (!g.debug_draw) return;
    const Camera3D& cam = g.cameras[0];
    line(cam, {0, 0, 0}, {1, 0, 0}, {255, 60, 60, 255});
    line(cam, {0, 0, 0}, {0, 1, 0}, {60, 255, 60, 255});
    line(cam, {0, 0, 0}, {0, 0, 1}, {80, 120, 255, 255});
    for (int i = 0; i < g.count; ++i) {
        const MeshRecord* mesh = mesh_get(g.records[i].mesh_slot, g.records[i].mesh_generation);
        if (mesh != nullptr) box(cam, g.records[i], *mesh);
    }
}

} // namespace detail

void gfx3d::set_debug_draw(bool on) {
    if constexpr (build::kDevToolsEnabled) detail::state3d().debug_draw = on;
    else (void)on;
}

} // namespace nnh
