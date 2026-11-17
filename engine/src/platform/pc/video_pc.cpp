// PC simulator: window, frame lifecycle and 60 Hz pacing (SDL3).
//
// Developer environment variables (simulator only, see docs/api/simulator.md):
//   NNH_EXIT_AFTER_FRAMES=<n>   quit after n frames (smoke tests, CI)
//   NNH_SCREENSHOT=<file.png>   save the last frame before quitting (needs NNH_EXIT_AFTER_FRAMES)

#include <SDL3/SDL.h>
// The game owns main(); SDL must not replace it.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <cstdlib>

#include "platform/pc/context_pc.hpp"
#include "platform/platform.hpp"

namespace nnh::platform {
namespace {

constexpr Uint64 kFrameNs = 1'000'000'000ull / 60;

pc::Context g_context;
Uint64 g_next_frame_ns = 0;
std::uint64_t g_frames = 0;
std::uint64_t g_exit_after = 0;
const char* g_screenshot_path = nullptr;

void save_screenshot() {
    SDL_Surface* shot = SDL_RenderReadPixels(g_context.renderer, nullptr);
    if (shot == nullptr || !SDL_SavePNG(shot, g_screenshot_path)) {
        SDL_Log("NNHGE: screenshot failed: %s", SDL_GetError());
    }
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

void read_dev_options() {
    if (const char* n = std::getenv("NNH_EXIT_AFTER_FRAMES")) g_exit_after = std::strtoull(n, nullptr, 10);
    g_screenshot_path = std::getenv("NNH_SCREENSHOT");
}

} // namespace

pc::Context& pc::context() { return g_context; }

bool init(const AppConfig& config) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("NNHGE: SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    const int scale = config.pc_window_scale > 0 ? config.pc_window_scale : 1;
    if (!SDL_CreateWindowAndRenderer(config.title, pc::kLogicalWidth * scale, pc::kLogicalHeight * scale,
                                     SDL_WINDOW_RESIZABLE, &g_context.window, &g_context.renderer)) {
        SDL_Log("NNHGE: window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }
    SDL_SetRenderLogicalPresentation(g_context.renderer, pc::kLogicalWidth, pc::kLogicalHeight,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderVSync(g_context.renderer, 1);
    read_dev_options();
    g_frames = 0;
    g_next_frame_ns = SDL_GetTicksNS();
    return true;
}

void shutdown() {
    SDL_DestroyRenderer(g_context.renderer);
    SDL_DestroyWindow(g_context.window);
    g_context = {};
    SDL_Quit();
}

bool poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return false;
    }
    return g_exit_after == 0 || g_frames < g_exit_after;
}

void begin_frame(Color top_clear, Color bottom_clear) {
    SDL_Renderer* renderer = g_context.renderer;
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
    SDL_SetRenderClipRect(g_context.renderer, nullptr);
    ++g_frames;
    if (g_screenshot_path != nullptr && g_exit_after != 0 && g_frames == g_exit_after) save_screenshot();
    SDL_RenderPresent(g_context.renderer);
    wait_for_next_frame();
}

} // namespace nnh::platform
