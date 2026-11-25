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
constexpr std::uint8_t kMarker = 0xAB;   // the last pixel before the copy: changed once the copy has landed
constexpr int kMaxWaits = 100;           // × kWaitNs: 100 ms
constexpr s64 kWaitNs = 1'000'000;

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

/// Reads `target` (a screen's, or the top screen's right eye) back; see the file comment.
bool capture(C3D_RenderTarget* target, bool top, ScreenPixels& out) {
    if constexpr (!build::kDevToolsEnabled) return false;
    ctr::Context& ctx = ctr::context();
    if (ctx.frame_open) {
        ctr::set_error("a screen is read back after end_frame(), not while a frame is drawn");
        return false;
    }
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
    // C3D_SyncDisplayTransfer can return before its copy lands: its PPF wait may wake on the frame's own display
    // transfer (console check of spec 026: a probe read the previous capture's bytes, then a sentinel). So the buffer's
    // last pixel (written last) is set to a marker first, and after the call the CPU waits until it changes, dropping its
    // cached lines each time (100 ms at most: a screen whose last pixel really is the marker reads right after it).
    const u32 size = kColumnPixels * columns * kBytesPerPixel;
    const u32 address = reinterpret_cast<u32>(g_buffer);
    std::uint8_t* last = g_buffer + size - kBytesPerPixel;
    last[0] = last[1] = last[2] = kMarker;
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, address, size);
    const u32 dim = GX_BUFFER_DIM(kColumnPixels, columns);
    C3D_SyncDisplayTransfer(static_cast<u32*>(target->frameBuf.colorBuf), dim, reinterpret_cast<u32*>(g_buffer),
                            dim, target->transferFlags);
    for (int wait = 0;; ++wait) {
        svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, address, size); // the GPU wrote memory the CPU may cache
        if (last[0] != kMarker || last[1] != kMarker || last[2] != kMarker || wait == kMaxWaits) break;
        svcSleepThread(kWaitNs);
    }
    out.data = g_buffer;
    out.size = size;
    out.width = static_cast<std::uint16_t>(columns);
    out.height = static_cast<std::uint16_t>(kColumnPixels);
    out.layout = PixelLayout::Bgr8ColumnsBottomUp;
    return true;
}

} // namespace

bool remote_capture(Screen screen, ScreenPixels& out) {
    const bool top = screen == Screen::Top;
    return capture(ctr::context().targets[top ? 0 : 1], top, out);
}

// The right eye's target after a stereo frame (eyes_3ds.cpp sets ctx.right_eye), else the top screen's (spec 026). The
// same marker wait as remote_capture: C3D_SyncDisplayTransfer may return before its copy lands.
bool remote_capture_right(ScreenPixels& out) {
    const ctr::Context& ctx = ctr::context();
    return capture(ctx.right_eye != nullptr ? ctx.right_eye : ctx.targets[0], true, out);
}

void remote_capture_release() {
    if (g_buffer != nullptr) linearFree(g_buffer);
    g_buffer = nullptr;
}

} // namespace nnh::platform
