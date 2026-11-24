// A screen's pass with 3D (spec 026 FR-18/FR-19/FR-20, D43): 2D below the world layer, the 3D pass (culled once,
// opaque and cut-out parts in call order, blended parts back to front), 2D from the world layer up. The 2D slices
// are exactly the 2D-only submission's (gfx_submit_slice). Mono in spec 026a: one eye.

#include "gfx/gfx_internal.hpp"
#include "gfx/gfx_state.hpp"
#include "gfx/world_split.hpp"
#include "gfx3d/pass3d.hpp"
#include "mesh/mesh_internal.hpp"

namespace nnh::detail {
namespace {

/// Culls every record, marks blended ones and their view depths, and orders them.
Order prepare(State3D& g, const Camera3D& cam, const Mat4& view) {
    const Frustum f = make_frustum(cam);
    for (int i = 0; i < g.count; ++i) {
        const DrawRecord& r = g.records[i];
        const MeshRecord* mesh = mesh_get(r.mesh_slot, r.mesh_generation);
        const Vec3 c = transform_point(view, g.spheres[i].center);
        g.visible[i] = mesh != nullptr && sphere_visible(f, c, g.spheres[i].radius);
        g.frame_culled += mesh != nullptr && !g.visible[i] ? 1 : 0;
        g.keys[i] = c.z;
        bool blend = false;
        if (mesh != nullptr) {
            const int first = r.submesh < 0 ? 0 : r.submesh, last = r.submesh < 0 ? mesh->submesh_count : r.submesh + 1;
            for (int s = first; s < last; ++s) blend = blend || draw_material(r, *mesh, s).alpha == AlphaMode::Blend;
        }
        g.blended[i] = blend;
    }
    return order_draws(g.visible, g.blended, g.keys, g.count, g.order, g.scratch);
}

/// Draws the parts of record `i` that are (or aren't) blended.
void draw_parts(State3D& g, int i, bool blended_part, const Mat4& view) {
    const DrawRecord& r = g.records[i];
    const MeshRecord* mesh = mesh_get(r.mesh_slot, r.mesh_generation);
    if (mesh == nullptr) return;
    const int first = r.submesh < 0 ? 0 : r.submesh, last = r.submesh < 0 ? mesh->submesh_count : r.submesh + 1;
    for (int s = first; s < last; ++s) {
        if ((draw_material(r, *mesh, s).alpha == AlphaMode::Blend) != blended_part) continue;
        platform::Draw3D d;
        if (!make_draw(r, *mesh, s, view, d)) continue;
        platform::draw_3d(d);
        g.frame_triangles[0] += static_cast<int>(d.index_count / 3);
    }
}

} // namespace

bool gfx3d_submit_screen(Screen screen) {
    State3D& g = state3d();
    if (!g.ready || screen != Screen::Top || g.count == 0) return false;
    gfx3d_debug_lines(screen); // into the 2D list before it is built (Debug/Development)
    const Pass2D pass = gfx_build_pass(screen);
    const GfxState& g2 = gfx_state();
    const WorldSplit split = world_split(g2.pass, pass.items, g2.splits, pass.splits, g.world_layer[0]);
    const Camera3D& cam = g.cameras[0];
    const Mat4 view = camera_view(cam);
    const Order order = prepare(g, cam, view);
    int stale_tiles = gfx_submit_slice(screen, 0, split.item, 0, split.split);
    platform::Pass3D p;
    p.eye = 0;
    const Mat4 proj = camera_projection(cam);
    for (int i = 0; i < 16; ++i) p.projection[i] = proj.m[i];
    lights_to_view(g.lighting[0], view, p);
    p.below_2d = split.item > 0 || split.split > 0;
    p.clear = clear_color(screen);
    platform::begin_3d(screen, p);
    for (int k = 0; k < order.opaque; ++k) draw_parts(g, g.order[k], false, view);
    for (int k = 0; k < order.blended; ++k) draw_parts(g, g.order[order.opaque + k], true, view);
    platform::end_3d();
    g.frame_eyes = 1;
    stale_tiles += gfx_submit_slice(screen, split.item, pass.items, split.split, pass.splits);
    gfx_check_stale(pass, stale_tiles);
    return true;
}

} // namespace nnh::detail
