// Nintendo 3DS platform: application lifecycle — GPU setup and APT events (libctru + citro3d/citro2d).

#include <3ds.h>
#include <citro2d.h>

#include "context_3ds.hpp"
#include "nnh/platform/app.hpp"

namespace nnh::platform {
namespace {

constexpr std::size_t kDebugTextGlyphs = 4096;

ctr::Context g_context;

} // namespace

ctr::Context& ctr::context() { return g_context; }

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

} // namespace nnh::platform
