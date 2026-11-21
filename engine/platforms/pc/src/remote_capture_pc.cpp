// PC simulator: screens read back for the console remote's screenshots (contract nnh/platform/remote.hpp,
// spec 013 T4.2). Each screen is a render target at native size (app_pc.cpp) that keeps what the last frame
// drew until the next begin_frame(), so core reads it right after end_frame(): RGBA8 rows, top-down, the
// same size as the console's screens. One surface is kept until the next capture or the release.

#include <SDL3/SDL.h>

#include "context_pc.hpp"
#include "nnh/platform/remote.hpp"

namespace nnh::platform {
namespace {

SDL_Surface* g_capture = nullptr;

} // namespace

bool remote_capture(Screen screen, ScreenPixels& out) {
    remote_capture_release();
    pc::Context& ctx = pc::context();
    if (ctx.frame_open) {
        pc::set_error("a screen is read back after end_frame(), not while a frame is drawn");
        return false;
    }
    SDL_Texture* previous = SDL_GetRenderTarget(ctx.renderer);
    pc::draw_on(screen);
    SDL_Surface* raw = SDL_RenderReadPixels(ctx.renderer, nullptr);
    SDL_SetRenderTarget(ctx.renderer, previous);
    g_capture = raw != nullptr ? SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGBA32) : nullptr;
    SDL_DestroySurface(raw);
    if (g_capture == nullptr || g_capture->pitch != g_capture->w * 4) {
        pc::set_error("screen read-back failed");
        remote_capture_release();
        return false;
    }
    out.data = static_cast<const std::uint8_t*>(g_capture->pixels);
    out.width = static_cast<std::uint16_t>(g_capture->w);
    out.height = static_cast<std::uint16_t>(g_capture->h);
    out.size = static_cast<std::uint32_t>(g_capture->w) * static_cast<std::uint32_t>(g_capture->h) * 4u;
    out.layout = PixelLayout::Rgba8Rows;
    return true;
}

void remote_capture_release() {
    SDL_DestroySurface(g_capture); // null-safe
    g_capture = nullptr;
}

} // namespace nnh::platform
