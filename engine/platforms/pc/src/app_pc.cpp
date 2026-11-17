// PC platform: application lifecycle — window creation, event polling, developer options (SDL3).
//
// Developer environment variables (simulator only, see docs/api/simulator.md):
//   NNH_EXIT_AFTER_FRAMES=<n>   quit after n frames (smoke tests, CI)
//   NNH_SCREENSHOT=<file.png>   save the last frame before quitting (needs NNH_EXIT_AFTER_FRAMES)

#include <SDL3/SDL.h>
// The game owns main(); SDL must not replace it.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <cstdlib>

#include "context_pc.hpp"
#include "nnh/platform/app.hpp"

namespace nnh::platform {
namespace {

pc::Context g_context;

void read_dev_options() {
    const char* n = std::getenv("NNH_EXIT_AFTER_FRAMES");
    g_context.exit_after = n != nullptr ? std::strtoull(n, nullptr, 10) : 0;
    g_context.screenshot_path = std::getenv("NNH_SCREENSHOT");
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
    g_context.frames = 0;
    pc::reset_frame_pacing();
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
    return g_context.exit_after == 0 || g_context.frames < g_context.exit_after;
}

} // namespace nnh::platform
