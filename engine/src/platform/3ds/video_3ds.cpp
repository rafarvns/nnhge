// Nintendo 3DS: GPU setup and frame lifecycle (libctru + citro3d/citro2d).

#include <3ds.h>
#include <citro2d.h>

#include "platform/3ds/context_3ds.hpp"
#include "platform/platform.hpp"

namespace nnh::platform {
namespace {

constexpr std::size_t kDebugTextGlyphs = 4096;

ctr::Context g_context;

} // namespace

ctr::Context& ctr::context() { return g_context; }

void ctr::draw_on(Screen screen) {
    C3D_RenderTarget* target = g_context.targets[screen == Screen::Top ? 0 : 1];
    if (g_context.current_target != target) {
        C2D_SceneBegin(target);
        g_context.current_target = target;
    }
}

bool init(const AppConfig& config) {
    (void)config; // title and icon come from the SMDH on 3DS
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return false;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C3D_Fini();
        gfxExit();
        return false;
    }
    C2D_Prepare();
    g_context.targets[0] = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_context.targets[1] = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    g_context.text_buffer = C2D_TextBufNew(kDebugTextGlyphs);
    return true;
}

void shutdown() {
    C2D_TextBufDelete(g_context.text_buffer);
    g_context = {};
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

bool poll_events() { return aptMainLoop(); }

void begin_frame(Color top_clear, Color bottom_clear) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(g_context.text_buffer);
    C2D_TargetClear(g_context.targets[0], ctr::to_c2d(top_clear));
    C2D_TargetClear(g_context.targets[1], ctr::to_c2d(bottom_clear));
    // Begin a scene on both targets so each one is presented even if nothing is drawn on it.
    g_context.current_target = nullptr;
    ctr::draw_on(Screen::Bottom);
    ctr::draw_on(Screen::Top);
}

void end_frame() { C3D_FrameEnd(0); }

} // namespace nnh::platform
