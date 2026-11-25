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
    pc::context().frame_open = true;
    // Each screen is drawn into its own target at native size; end_frame composes them (spec 006).
    pc::draw_on(Screen::Top);
    pc::set_draw_color(top_clear);
    SDL_RenderClear(renderer);
    pc::draw_on(Screen::Bottom);
    pc::set_draw_color(bottom_clear);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

void end_frame() {
    pc::Context& ctx = pc::context();
    pc::compose_screens();
    ctx.frame_open = false;
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

// SDL's renderer may still hold a frame's commands, but destroying a texture or a mesh through SDL is always
// safe: nothing to wait for.
void finish_frames() {}


} // namespace nnh::platform

