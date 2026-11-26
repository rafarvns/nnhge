// The held frame's replay (see gfx_replay.hpp). Development only: reached from Debug/Development branches.

#include "gfx/gfx_replay.hpp"

#include "gfx/gfx3d_hook.hpp"
#include "gfx/gfx_state.hpp"

namespace nnh::detail {

bool g_gfx_held = false;

void gfx_begin_recording() {
    if (!g_gfx_held) return;
    g_gfx_held = false;
    GfxState& g = gfx_state();
    g.draws.clear();
    g.tiles.clear();
    if (g_gfx_particle_ops != nullptr) g_gfx_particle_ops->end_frame();
}

bool gfx_replay(bool scenes_changed) {
    GfxState& g = gfx_state();
    if (!g.ready || !g_gfx_held || scenes_changed) return false;
    for (Screen screen : {Screen::Top, Screen::Bottom}) {
        const Pass2D pass = gfx_build_pass(screen); // released textures are skipped (counted in pass.stale)
        gfx_submit_slice(screen, 0, pass.items, 0, pass.splits);
    }
    return true;
}

} // namespace nnh::detail
