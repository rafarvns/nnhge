// PC simulator developer controls (Debug/Development builds):
//   F1 overlay · Pause pause/resume · F10 one frame while paused · F11 slow motion (1/4) · F12 screenshot
// Screenshots go to <data folder>/screenshots/shot-NNN.png.

#include <SDL3/SDL.h>

#include <cstdio>

#include "context_pc.hpp"
#include "nnh/build.hpp"
#include "nnh/platform/fs.hpp"
#include "nnh/platform/system.hpp"

namespace nnh::platform {
namespace {

int g_screenshot_number = 0;

void save_screenshot_now() {
    pc::Context& ctx = pc::context();
    char folder[384];
    char path[400];
    std::snprintf(folder, sizeof folder, "%sscreenshots", fs_root());
    SDL_CreateDirectory(folder);
    std::snprintf(path, sizeof path, "%s/shot-%03d.png", folder, ++g_screenshot_number);
    SDL_Surface* shot = SDL_RenderReadPixels(ctx.renderer, nullptr);
    if (shot != nullptr) {
        SDL_SavePNG(shot, path);
        SDL_DestroySurface(shot);
    }
}

} // namespace

void pc::dev_key_pressed(SDL_Scancode key) {
    if constexpr (!build::kDevToolsEnabled) return;
    Context& ctx = context();
    switch (key) {
    case SDL_SCANCODE_F1: ctx.dev_requests |= kDevToggleOverlay; break;
    case SDL_SCANCODE_PAUSE: ctx.paused = !ctx.paused; break;
    case SDL_SCANCODE_F10: ctx.step_requested = true; break;
    case SDL_SCANCODE_F11: ctx.slow_motion = !ctx.slow_motion; break;
    case SDL_SCANCODE_F12: ctx.screenshot_requested = true; break;
    default: break;
    }
}

void pc::dev_before_present() {
    Context& ctx = context();
    if (ctx.screenshot_requested) {
        ctx.screenshot_requested = false;
        save_screenshot_now();
    }
}

void pc::dev_wait_while_paused() {
    Context& ctx = context();
    ctx.step_requested = false;
    while (ctx.paused && !ctx.step_requested && !ctx.quit_requested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) ctx.quit_requested = true;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) dev_key_pressed(event.key.scancode);
        }
        SDL_Delay(16);
    }
}

} // namespace nnh::platform
