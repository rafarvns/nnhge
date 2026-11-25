// A screen's pass with 3D (spec 026 FR-14/FR-15/FR-18/FR-19/FR-20, D43): 2D below the world layer, the 3D pass
// (culled once — both eyes' union in stereo —, opaque and cut-out parts in call order, blended parts back to front by
// the centre camera), 2D from the world layer up. The 2D slices are exactly the 2D-only submission's
// (gfx_submit_slice). In a stereo frame the whole sequence runs once per eye, each eye with its own view, projection
// and lights, the 2D slices identical; the tile and particle budgets are spent once.

#include "gfx/gfx_internal.hpp"
#include "gfx/gfx_state.hpp"
#include "gfx/particle_batches.hpp"
#include "gfx/world_split.hpp"
#include "gfx3d/pass3d.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/platform/video3d.hpp"
#include "nnh/profile.hpp"
#include "nnh/system.hpp"

namespace nnh::detail {
namespace {

/// Culls every record, marks blended ones and their view depths, and orders them.
Order prepare(State3D& g, const Mat4& view, const EyeCull& cull) {
#if defined(NNH_FLAVOR_DEVELOPMENT)
    NNH_PROFILE_SCOPE("gfx3d prepare"); // inside "gfx submit"; AC-8's CPU-alone case reads it (examples/models bench)
#endif
    for (int i = 0; i < g.count; ++i) {
        const DrawRecord& r = g.records[i];
        const MeshRecord* mesh = mesh_get(r.mesh_slot, r.mesh_generation);
        const Vec3 c = transform_point(view, g.spheres[i].center);
        g.visible[i] = mesh != nullptr && sphere_in_eyes(cull, c, g.spheres[i].radius);
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

/// Draws the parts of record `i` that are (or aren't) blended, with an eye's view.
void draw_parts(State3D& g, int i, bool blended_part, const Mat4& view, int eye) {
    const DrawRecord& r = g.records[i];
    const MeshRecord* mesh = mesh_get(r.mesh_slot, r.mesh_generation);
    if (mesh == nullptr) return;
    const int first = r.submesh < 0 ? 0 : r.submesh, last = r.submesh < 0 ? mesh->submesh_count : r.submesh + 1;
    for (int s = first; s < last; ++s) {
        if ((draw_material(r, *mesh, s).alpha == AlphaMode::Blend) != blended_part) continue;
        platform::Draw3D d;
        if (!make_draw(r, *mesh, s, view, d)) continue;
        platform::draw_3d(d);
        g.frame_triangles[eye] += static_cast<int>(d.index_count / 3);
    }
}

/// One eye's 3D pass: begin with its projection and lights, the ordered draws, end.
void eye_pass(State3D& g, Screen screen, const EyeView& e, int eye, const Order& order, bool below_2d) {
    platform::Pass3D p;
    p.eye = static_cast<std::uint8_t>(eye);
    for (int i = 0; i < 16; ++i) p.projection[i] = e.projection.m[i];
    lights_to_view(g.lighting[0], e.view, p);
    p.below_2d = below_2d;
    p.clear = clear_color(screen);
    platform::begin_3d(screen, p);
    for (int k = 0; k < order.opaque; ++k) draw_parts(g, g.order[k], false, e.view, eye);
    for (int k = 0; k < order.blended; ++k) draw_parts(g, g.order[order.opaque + k], true, e.view, eye);
    platform::end_3d();
}

} // namespace

bool gfx3d_submit_screen(Screen screen) {
    State3D& g = state3d();
    if (!g.ready || screen != Screen::Top) return false;
    if (g.count == 0) {
        platform::set_stereo(false); // a frame without 3D shows one picture
        return false;
    }
    gfx3d_debug_lines(screen); // into the 2D list before it is built (Debug/Development)
    const Pass2D pass = gfx_build_pass(screen);
    GfxState& g2 = gfx_state();
    const WorldSplit split = world_split(g2.pass, pass.items, g2.splits, pass.splits, g.world_layer[0]);
    const Camera3D& cam = g.cameras[0];
    const Eyes eyes = make_eyes(cam, system::stereo_slider());
    const Order order = prepare(g, camera_view(cam), make_eye_cull(cam, eyes)); // the centre camera's view
    const bool stereo = eyes.count == 2;
    platform::set_stereo(stereo);
    const TileBatches::Spent tiles = g2.tiles.spent();
    if (stereo && g_gfx_particle_ops != nullptr) g_gfx_particle_ops->budget_mark();
    int stale_tiles = 0;
    for (int eye = 0; eye < eyes.count; ++eye) {
        if (stereo) platform::select_eye(static_cast<std::uint8_t>(eye), clear_color(screen));
        if (eye == 1) { // the same 2D again: its budgets were spent by the first eye
            g2.tiles.rewind(tiles);
            if (g_gfx_particle_ops != nullptr) g_gfx_particle_ops->budget_rewind();
        }
        stale_tiles += gfx_submit_slice(screen, 0, split.item, 0, split.split);
        eye_pass(g, screen, eyes.eye[eye], eye, order, split.item > 0 || split.split > 0);
        stale_tiles += gfx_submit_slice(screen, split.item, pass.items, split.split, pass.splits);
    }
    if (stereo) platform::select_eye(0, clear_color(screen)); // what follows (the debug pass) starts on the left
    g.frame_eyes = eyes.count;
    gfx_check_stale(pass, stale_tiles);
    return true;
}

} // namespace nnh::detail
