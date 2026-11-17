// PC platform: application lifecycle — window creation, event polling, developer options (SDL3).
//
// Developer environment variables (simulator only, see docs/api/simulator.md):
//   NNH_EXIT_AFTER_FRAMES=<n>   quit after n frames (smoke tests, CI)
//   NNH_SCREENSHOT=<file.png>   save the last frame before quitting (needs NNH_EXIT_AFTER_FRAMES)

#include <SDL3/SDL.h>
// The game owns main(); SDL must not replace it.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>

#include "context_pc.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/app.hpp"
#include "stdio_fs.hpp"

namespace nnh::platform {
namespace {

pc::Context g_context;
char g_last_error[256] = "";

void set_error(const char* what) { std::snprintf(g_last_error, sizeof g_last_error, "%s: %s", what, SDL_GetError()); }

/// Debug/Development: <exe dir>/data/<folder>/ (easy to inspect next to the build);
/// Release: the user's app-data folder (SDL_GetPrefPath).
void set_data_root(const AppConfig& config) {
    char root[256];
    if constexpr (build::kDevToolsEnabled) {
        const char* base = SDL_GetBasePath();
        std::snprintf(root, sizeof root, "%sdata/%s/", base != nullptr ? base : "./", config.data_folder);
    } else {
        char* pref = SDL_GetPrefPath("NNHGE", config.data_folder);
        std::snprintf(root, sizeof root, "%s", pref != nullptr ? pref : "./");
        SDL_free(pref);
    }
    for (char* p = root; *p != '\0'; ++p) {
        if (*p == '\\') *p = '/';
    }
    shared::stdio_fs_set_root(root);
}

void read_dev_options() {
    const char* n = std::getenv("NNH_EXIT_AFTER_FRAMES");
    g_context.exit_after = n != nullptr ? std::strtoull(n, nullptr, 10) : 0;
    g_context.screenshot_path = std::getenv("NNH_SCREENSHOT");
}

} // namespace

pc::Context& pc::context() { return g_context; }

bool init(const AppConfig& config) {
    SDL_SetMainReady();
    g_last_error[0] = '\0';
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        set_error("SDL_Init failed");
        return false;
    }
    const int scale = config.pc_window_scale > 0 ? config.pc_window_scale : 1;
    if (!SDL_CreateWindowAndRenderer(config.title, pc::kLogicalWidth * scale, pc::kLogicalHeight * scale,
                                     SDL_WINDOW_RESIZABLE, &g_context.window, &g_context.renderer)) {
        set_error("window creation failed");
        SDL_Quit();
        return false;
    }
    SDL_SetRenderLogicalPresentation(g_context.renderer, pc::kLogicalWidth, pc::kLogicalHeight,
                                     SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    SDL_SetRenderVSync(g_context.renderer, 1);
    read_dev_options();
    set_data_root(config);
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

const char* last_error() { return g_last_error; }

bool poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return false;
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) pc::dev_key_pressed(event.key.scancode);
    }
    if (g_context.quit_requested) return false;
    return g_context.exit_after == 0 || g_context.frames < g_context.exit_after;
}

} // namespace nnh::platform
