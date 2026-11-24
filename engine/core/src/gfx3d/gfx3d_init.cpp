// 3D init and shutdown (gfx3d_state.hpp): the mesh table and the platform's 3D (stage 2); the frame's records join in
// stage 4.

#include "gfx3d/gfx3d_state.hpp"
#include "log/engine_channels.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/video3d.hpp"

namespace nnh::detail {
namespace {

bool g_ready = false;

} // namespace

bool gfx3d_init(const GfxConfig& config) {
    const int max_draws = config.max_draws_3d > 4096 ? 4096 : config.max_draws_3d;
    if (!mesh_table_init()) {
        NNH_LOG_ERROR(gfx, "not enough memory for the mesh table (3D)");
        return false;
    }
    if (!platform::video3d_init({.max_draws = max_draws})) {
        NNH_LOG_ERROR(gfx, "3D could not start: %s", platform::last_error());
        mesh_table_shutdown();
        return false;
    }
    g_ready = true;
    return true;
}

void gfx3d_shutdown() {
    if (!g_ready) return;
    mesh_table_shutdown();
    platform::video3d_shutdown();
    g_ready = false;
}

bool gfx3d_ready() { return g_ready; }

bool gfx3d_submit_screen(Screen) { return false; }
void gfx3d_frame_done() {}
bool gfx3d_overlay_line(char*, int) { return false; }

} // namespace nnh::detail
