// PC platform: application lifecycle — window creation, event polling, developer options (SDL3).
//
// Developer environment variables (simulator only, see docs/api/simulator.md):
//   NNH_EXIT_AFTER_FRAMES=<n>   quit after n frames (smoke tests, CI)
//   NNH_SCREENSHOT=<file.png>   save the last frame before quitting (needs NNH_EXIT_AFTER_FRAMES)
//   NNH_SIM_INPUT=<script>      scripted input, Debug/Development only (input_override_pc.cpp)
//   NNH_SIM_RENDERER=sdl        skip the GPU-device renderer (use SDL's default renderer)

#include <SDL3/SDL.h>
// The game owns main(); SDL must not replace it.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "context_pc.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/app.hpp"
#include "stdio_fs.hpp"

namespace nnh::platform {
namespace {

pc::Context g_context;
char g_last_error[256] = "";

} // namespace

void pc::set_error(const char* what) {
    std::snprintf(g_last_error, sizeof g_last_error, "%s: %s", what, SDL_GetError());
}

namespace {

using pc::set_error;

/// The renderer runs on an SDL_GPUDevice when one is available, so the future 3D renderer can share its
/// device and textures (docs/architecture/rendering.md, D20). Without a GPU (headless runs, old
/// drivers, VMs) SDL's default renderer is enough for 2D.
SDL_Renderer* create_renderer(SDL_Window* window) {
    const char* choice = std::getenv("NNH_SIM_RENDERER");
    const bool allow_gpu = choice == nullptr || std::strcmp(choice, "sdl") != 0;
    SDL_Renderer* renderer = allow_gpu ? SDL_CreateGPURenderer(nullptr, window) : nullptr;
    g_context.gpu_renderer = renderer != nullptr;
    if (renderer == nullptr) renderer = SDL_CreateRenderer(window, nullptr);
    return renderer;
}

/// One render target per 3DS screen at native size, scaled up when composed (nearest: sharp pixels).
bool create_screen_targets() {
    const int widths[2] = {400, 320};
    for (int i = 0; i < 2; ++i) {
        SDL_Texture* t = SDL_CreateTexture(g_context.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                                           widths[i], 240);
        if (t == nullptr) return false;
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
        g_context.screens[i] = t;
    }
    return true;
}

void destroy_window() {
    for (SDL_Texture*& t : g_context.screens) {
        SDL_DestroyTexture(t);
        t = nullptr;
    }
    SDL_DestroyRenderer(g_context.renderer);
    SDL_DestroyWindow(g_context.window);
    g_context = {};
}

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
    // Gamepads are optional: without the subsystem the simulator still runs on keyboard and mouse.
    // Pads already connected arrive as SDL_EVENT_GAMEPAD_ADDED in the first poll_events().
    SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    const int scale = config.pc_window_scale > 0 ? config.pc_window_scale : 1;
    g_context.scale = scale < pc::kMaxSimScale ? scale : pc::kMaxSimScale;
    const pc::SimSize size = pc::natural_size(g_context.layout, g_context.scale);
    g_context.window = SDL_CreateWindow(config.title, size.w, size.h, SDL_WINDOW_RESIZABLE);
    g_context.renderer = g_context.window != nullptr ? create_renderer(g_context.window) : nullptr;
    if (g_context.renderer == nullptr || !create_screen_targets()) {
        set_error("window creation failed");
        destroy_window();
        SDL_Quit();
        return false;
    }
    SDL_SetRenderVSync(g_context.renderer, 1);
    read_dev_options();
    pc::input_override_init();
    set_data_root(config);
    g_context.frames = 0;
    pc::reset_frame_pacing();
    return true;
}

void shutdown() {
    pc::gamepad_close();
    destroy_window();
    SDL_Quit();
}

const char* last_error() { return g_last_error; }

bool poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) return false;
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) pc::dev_key_pressed(event.key.scancode);
        pc::gamepad_event(event);
    }
    if (g_context.quit_requested) return false;
    return g_context.exit_after == 0 || g_context.frames < g_context.exit_after;
}

} // namespace nnh::platform
