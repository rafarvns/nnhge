// Nintendo 3DS platform: screens read back for the console remote's screenshots (contract nnh/platform/remote.hpp,
// spec 013 T5.2). The source is each screen's citro2d render target (tiled RGBA8 in VRAM, 240 × 400 or 240 × 320:
// the screen turned sideways), which keeps what the last frame drew until the next begin_frame(). The GPU copies
// it with C3D_SyncDisplayTransfer and the target's own transferFlags — the conversion C3D_FrameEnd() makes into
// the framebuffer — into one linear buffer, so the result has the framebuffer's layout: BGR8 (bytes B, G, R),
// one column of the screen after another from left to right, each from the bottom row up
// (PixelLayout::Bgr8ColumnsBottomUp: pixel (x, y) at ((x × 240) + (239 − y)) × 3). The buffer (288 000 bytes of
// linear memory, top screen size) is allocated at the first capture and reused for both screens: core sends one
// screen before asking for the next. Debug/Development only: core never calls this in Release.

#include <3ds.h>
#include <citro2d.h>

#include <cstdint>

#include "context_3ds.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/remote.hpp"

namespace nnh::platform {
namespace {

constexpr u32 kColumnPixels = 240; // a framebuffer column is one screen column: 240 pixels, bottom to top
constexpr u32 kBytesPerPixel = 3;  // BGR8
constexpr u32 kBufferBytes = kColumnPixels * 400 * kBytesPerPixel;

std::uint8_t* g_buffer = nullptr; // linear memory, kBufferBytes

/// Why `target` can't be read back as linear BGR8 of the screen's size, or nullptr if it can.
const char* unreadable(const C3D_RenderTarget* target, gfxScreen_t screen, u32 columns) {
    if (target == nullptr || target->frameBuf.colorBuf == nullptr) return "the screen has no render target";
    if (target->frameBuf.width != kColumnPixels || target->frameBuf.height != columns) {
        return "the screen's render target is not the screen's size";
    }
    const u32 flags = target->transferFlags;
    if ((flags & GX_TRANSFER_OUT_FORMAT(7)) != GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) ||
        (flags & GX_TRANSFER_OUT_TILED(1)) != 0) {
        return "the screen's display transfer doesn't give linear BGR8";
    }
    if (gfxGetScreenFormat(screen) != GSP_BGR8_OES) return "the screen's framebuffer is not BGR8";
    return nullptr;
}

} // namespace

bool remote_capture(Screen screen, ScreenPixels& out) {
    if constexpr (!build::kDevToolsEnabled) return false;
    ctr::Context& ctx = ctr::context();
    if (ctx.frame_open) {
        ctr::set_error("a screen is read back after end_frame(), not while a frame is drawn");
        return false;
    }
    const bool top = screen == Screen::Top;
    C3D_RenderTarget* target = ctx.targets[top ? 0 : 1];
    const u32 columns = top ? 400 : 320;
    if (const char* why = unreadable(target, top ? GFX_TOP : GFX_BOTTOM, columns)) {
        ctr::set_error(why);
        return false;
    }
    if (g_buffer == nullptr) g_buffer = static_cast<std::uint8_t*>(linearAlloc(kBufferBytes));
    if (g_buffer == nullptr) {
        ctr::set_error("no linear memory for the screen read-back (288000 bytes)");
        return false;
    }
    // Waits for the GPU's queue (the frame just ended: its drawing and its own transfers), then for this copy.
    const u32 dim = GX_BUFFER_DIM(kColumnPixels, columns);
    C3D_SyncDisplayTransfer(static_cast<u32*>(target->frameBuf.colorBuf), dim, reinterpret_cast<u32*>(g_buffer),
                            dim, target->transferFlags);
    const u32 size = kColumnPixels * columns * kBytesPerPixel;
    GSPGPU_InvalidateDataCache(g_buffer, size); // the GPU wrote memory the CPU may hold in its cache
    out.data = g_buffer;
    out.size = size;
    out.width = static_cast<std::uint16_t>(columns);
    out.height = static_cast<std::uint16_t>(kColumnPixels);
    out.layout = PixelLayout::Bgr8ColumnsBottomUp;
    return true;
}

void remote_capture_release() {
    if (g_buffer != nullptr) linearFree(g_buffer);
    g_buffer = nullptr;
}

} // namespace nnh::platform
