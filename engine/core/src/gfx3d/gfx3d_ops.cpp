// The 3D module's side of the hook (gfx/gfx3d_hook.hpp, plan P14): the operations table and its registrar. This object
// is linked only through gfx3d_link(), which every mesh and 3D entry point calls, so a 2D-only game links no 3D code.

#include "gfx/gfx3d_hook.hpp"
#include "gfx/gfx_internal.hpp"
#include "gfx3d/gfx3d_state.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/platform/video3d.hpp"

namespace nnh::detail {
namespace {

void after_frame() {
    gfx3d_probes_after_frame();
    gfx3d_frame_done();
    mesh_retire_released();
}

int eyes_this_frame() { return state3d().frame_eyes; }

void select_eye(int eye) { platform::select_eye(static_cast<std::uint8_t>(eye), clear_color(Screen::Top)); }

constexpr Gfx3DOps kOps{
    .init = &gfx3d_init,
    .shutdown = &gfx3d_shutdown,
    .submit_screen = &gfx3d_submit_screen,
    .after_frame = &after_frame,
    .gpu_idle = &mesh_free_retired,
    .overlay_line = &gfx3d_overlay_line,
    .eyes_this_frame = &eyes_this_frame,
    .select_eye = &select_eye,
};

struct Registrar {
    Registrar() { g_gfx_3d_ops = &kOps; }
};
const Registrar g_registrar;

} // namespace

void gfx3d_link() {}

} // namespace nnh::detail
