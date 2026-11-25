// Nintendo 3DS platform: frame begin/end and render target selection (citro3d/citro2d).

#include <3ds.h>
#include <citro2d.h>

#include "context_3ds.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {

void ctr::draw_on(Screen screen) {
    Context& ctx = context();
    C3D_RenderTarget* target = ctx.targets[screen == Screen::Top ? 0 : 1];
    if (ctx.current_target != target) {
        C2D_SceneBegin(target);
        ctx.current_target = target;
    }
}

// C3D_FrameEnd starts the GPU and returns at once; C3D_FrameBegin waits for the frame slot (SYNCDRAW) and for the GPU's
// queue, then reuses the one command buffer: core frees released textures and meshes only after it (D47).
void begin_frame(Color top_clear, Color bottom_clear) {
    ctr::Context& ctx = ctr::context();
    if (ctx.c3d_waited) {
        C3D_FrameSync(); // the GPU is idle already: only the frame slot SYNCDRAW would have waited for
        ctx.c3d_waited = false;
    } else {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
    C2D_TargetClear(ctx.targets[0], ctr::to_c2d(top_clear));
    C2D_TargetClear(ctx.targets[1], ctr::to_c2d(bottom_clear));
    // Begin a scene on both targets so each one is presented even if nothing is drawn on it.
    ctx.current_target = nullptr;
    ctr::draw_on(Screen::Bottom);
    ctr::draw_on(Screen::Top);
    ctx.frame_open = true;
}

void end_frame() {
    C3D_FrameEnd(0);
    ctr::context().frame_open = false;
}

// citro3d has no public "wait for the GPU" outside its frames: C3D_FrameBegin waits for the queue and enters the next
// frame, which the next begin_frame() (or shutdown, through C3D_FrameEnd) continues. Nothing is drawn or shown.
void finish_frames() {
    ctr::Context& ctx = ctr::context();
    if (ctx.frame_open || ctx.c3d_waited) return;
    C3D_FrameBegin(0);
    ctx.c3d_waited = true;
}

} // namespace nnh::platform

