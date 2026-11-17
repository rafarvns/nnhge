// PC platform: frame begin/end, 60 Hz pacing and screenshots (SDL3).

#include <SDL3/SDL.h>

#include "context_pc.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {
namespace {

constexpr Uint64 kFrameNs = 1'000'000'000ull / 60;

Uint64 g_next_frame_ns = 0;

void save_screenshot(const char* path) {
    SDL_Surface* shot = SDL_RenderReadPixels(pc::context().renderer, nullptr);
    if (shot == nullptr || !SDL_SavePNG(shot, path)) SDL_Log("NNHGE: screenshot failed: %s", SDL_GetError());
    SDL_DestroySurface(shot);
}

void wait_for_next_frame() {
    // Hold the 3DS rate (60 Hz) even on faster monitors.
    g_next_frame_ns += kFrameNs;
    const Uint64 now = SDL_GetTicksNS();
    if (now < g_next_frame_ns) {
        SDL_DelayPrecise(g_next_frame_ns - now);
    } else if (now - g_next_frame_ns > kFrameNs * 4) {
        g_next_frame_ns = now; // fell far behind (debugger, window drag): don't try to catch up
    }
}

} // namespace

void pc::reset_frame_pacing() { g_next_frame_ns = SDL_GetTicksNS(); }

void begin_frame(Color top_clear, Color bottom_clear) {
    SDL_Renderer* renderer = pc::context().renderer;
    SDL_SetRenderClipRect(renderer, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    pc::set_draw_color(rgb(12, 12, 12)); // area outside the screens
    SDL_RenderClear(renderer);
    const SDL_FRect top = pc::screen_rect(Screen::Top);
    const SDL_FRect bottom = pc::screen_rect(Screen::Bottom);
    pc::set_draw_color(top_clear);
    SDL_RenderFillRect(renderer, &top);
    pc::set_draw_color(bottom_clear);
    SDL_RenderFillRect(renderer, &bottom);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

void end_frame() {
    pc::Context& ctx = pc::context();
    SDL_SetRenderClipRect(ctx.renderer, nullptr);
    ++ctx.frames;
    if (ctx.screenshot_path != nullptr && ctx.exit_after != 0 && ctx.frames == ctx.exit_after) {
        save_screenshot(ctx.screenshot_path);
    }
    SDL_RenderPresent(ctx.renderer);
    wait_for_next_frame();
}

} // namespace nnh::platform
