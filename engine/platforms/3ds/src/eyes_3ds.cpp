// Nintendo 3DS: stereo (contract nnh/platform/video3d.hpp set_stereo/select_eye, spec 026 FR-14/FR-26, plan "Platform
// work"). video3d_3ds.cpp made a second top target on the right output (RGBA8 + DEPTH24_STENCIL8, 768 000 B of VRAM);
// select_eye swaps ctx.targets[0] between the two, so citro2d's 2D and the 3D pass draw on the selected eye. The right
// target is cleared (colour, and depth to 0 for the GREATER test) at its first selection in a frame: C2D_TargetClear
// flushes and splits the frame first, so the clear lands after what came before it. gfxSet3D follows the frame's eye
// count, called only when it changes; citro3d copies the right target to the screen only in frames that drew on it.

#include <3ds.h>
#include <citro2d.h>

#include "context_3ds.hpp"
#include "nnh/platform/video3d.hpp"

namespace nnh::platform {
namespace {

bool g_stereo = false;       // this frame draws two eyes
bool g_shown = false;        // gfxSet3D's state
bool g_right_cleared = false;

} // namespace

void set_stereo(bool on) {
    ctr::Context& ctx = ctr::context();
    g_stereo = on && ctx.right_target != nullptr;
    g_right_cleared = false;
    if (g_stereo != g_shown) {
        gfxSet3D(g_stereo);
        g_shown = g_stereo;
    }
    ctx.right_eye = g_stereo ? ctx.right_target : nullptr;
}

void select_eye(std::uint8_t eye, Color top_clear) {
    ctr::Context& ctx = ctr::context();
    if (!g_stereo) return;
    C3D_RenderTarget* target = eye == 1 ? ctx.right_target : ctx.left_target;
    if (eye == 1 && !g_right_cleared) {
        g_right_cleared = true;
        C2D_TargetClear(target, ctr::to_c2d(top_clear));
    }
    ctx.targets[0] = target;
    ctr::draw_on(Screen::Top); // C2D_SceneBegin on the eye's target
}

} // namespace nnh::platform
