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

void begin_frame(Color top_clear, Color bottom_clear) {
    ctr::Context& ctx = ctr::context();
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(ctx.text_buffer);
    C2D_TargetClear(ctx.targets[0], ctr::to_c2d(top_clear));
    C2D_TargetClear(ctx.targets[1], ctr::to_c2d(bottom_clear));
    // Begin a scene on both targets so each one is presented even if nothing is drawn on it.
    ctx.current_target = nullptr;
    ctr::draw_on(Screen::Bottom);
    ctr::draw_on(Screen::Top);
}

void end_frame() { C3D_FrameEnd(0); }

void fill_rect(Screen screen, int x, int y, int width, int height, Color color) {
    ctr::draw_on(screen);
    C2D_DrawRectSolid(static_cast<float>(x), static_cast<float>(y), 0.4f, static_cast<float>(width),
                      static_cast<float>(height), ctr::to_c2d(color));
}

} // namespace nnh::platform
