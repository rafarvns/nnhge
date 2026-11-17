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
    // Hold the 3DS rate (60 Hz) even on faster monitors; 15 Hz in slow motion (F11).
    g_next_frame_ns += pc::context().slow_motion ? kFrameNs * 4 : kFrameNs;
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
    pc::dev_before_present();
    SDL_RenderPresent(ctx.renderer);
    pc::dev_wait_while_paused();
    if (ctx.paused) pc::reset_frame_pacing(); // after a pause or a single step, don't rush to catch up
    wait_for_next_frame();
}

void fill_rect(Screen screen, int x, int y, int width, int height, Color color) {
    pc::Context& ctx = pc::context();
    const SDL_FRect area = pc::screen_rect(screen);
    const SDL_Rect clip{static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.w),
                        static_cast<int>(area.h)};
    SDL_SetRenderClipRect(ctx.renderer, &clip);
    pc::set_draw_color(color);
    const SDL_FRect rect{area.x + static_cast<float>(x), area.y + static_cast<float>(y), static_cast<float>(width),
                         static_cast<float>(height)};
    SDL_RenderFillRect(ctx.renderer, &rect);
}

} // namespace nnh::platform
