// PC simulator: the window view (spec 006) — where the two screen targets go in the window, the window
// keys (F2 layout, F3/F4 scale, F5 fit, F9 or Alt+Enter fullscreen) and the title notice. The layout
// math is pure (sim_layout.cpp); this file applies it with SDL.

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>

#include "context_pc.hpp"
#include "sim_layout.hpp"

namespace nnh::platform {
namespace {

constexpr int kScreenWidth[2] = {400, 320};
constexpr int kScreenHeight = 240;

void copy_screen(SDL_Renderer* renderer, SDL_Texture* screen, int width, const pc::Placement& p) {
    if (!p.visible || screen == nullptr) return;
    const SDL_FRect dest{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(width * p.scale),
                         static_cast<float>(kScreenHeight * p.scale)};
    SDL_RenderTexture(renderer, screen, nullptr, &dest);
}

constexpr std::uint64_t kNoticeMs = 2000;

/// The scale the window shows now (the fit scale while fitting or in fullscreen).
int effective_scale() {
    const pc::LayoutResult layout = pc::current_layout();
    return layout.top.visible ? layout.top.scale : layout.bottom.scale;
}

/// Shows the view in the window title for a moment: "My Game — Side by side 3x".
void show_notice() {
    pc::Context& ctx = pc::context();
    char text[192];
    const char* layout = pc::layout_title(ctx.layout);
    if (ctx.fullscreen) {
        std::snprintf(text, sizeof text, "%s — %s, fullscreen %dx", ctx.title, layout, effective_scale());
    } else if (ctx.fit) {
        std::snprintf(text, sizeof text, "%s — %s, fit %dx", ctx.title, layout, effective_scale());
    } else {
        std::snprintf(text, sizeof text, "%s — %s %dx", ctx.title, layout, ctx.scale);
    }
    SDL_SetWindowTitle(ctx.window, text);
    ctx.notice_until_ms = SDL_GetTicks() + kNoticeMs;
}

void set_fullscreen(bool on) {
    pc::Context& ctx = pc::context();
    if (ctx.fullscreen == on) return;
    if (!SDL_SetWindowFullscreen(ctx.window, on)) return; // keep the window as it is
    ctx.fullscreen = on;
    if (!on) pc::view_resize_window();
}

} // namespace

void pc::view_resize_window() {
    Context& ctx = context();
    if (ctx.fullscreen || ctx.fit || ctx.window == nullptr) return;
    // The layout is in render pixels; the window size is in window units (they differ on high-DPI).
    const SimSize size = natural_size(ctx.layout, ctx.scale);
    const float density = SDL_GetWindowPixelDensity(ctx.window) > 0 ? SDL_GetWindowPixelDensity(ctx.window) : 1;
    SDL_SetWindowSize(ctx.window, static_cast<int>(static_cast<float>(size.w) / density + 0.5f),
                      static_cast<int>(static_cast<float>(size.h) / density + 0.5f));
}

void pc::view_key_pressed(SDL_Scancode key, SDL_Keymod mods) {
    Context& ctx = context();
    switch (key) {
    case SDL_SCANCODE_F2:
        ctx.layout = next_layout(ctx.layout);
        view_resize_window();
        break;
    case SDL_SCANCODE_F3:
    case SDL_SCANCODE_F4: {
        // Stepping the scale leaves fit mode, starting from the scale it was showing.
        const int from = ctx.fit && !ctx.fullscreen ? effective_scale() : ctx.scale;
        const int to = from + (key == SDL_SCANCODE_F4 ? 1 : -1);
        ctx.scale = to < 1 ? 1 : (to > kMaxSimScale ? kMaxSimScale : to);
        ctx.fit = false;
        view_resize_window();
        break;
    }
    case SDL_SCANCODE_F5:
        ctx.fit = !ctx.fit;
        view_resize_window();
        break;
    case SDL_SCANCODE_F9: set_fullscreen(!ctx.fullscreen); break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        if ((mods & SDL_KMOD_ALT) == 0) return;
        set_fullscreen(!ctx.fullscreen);
        break;
    default: return;
    }
    show_notice();
}

void pc::view_update() {
    Context& ctx = context();
    if (ctx.notice_until_ms != 0 && SDL_GetTicks() >= ctx.notice_until_ms) {
        SDL_SetWindowTitle(ctx.window, ctx.title);
        ctx.notice_until_ms = 0;
    }
}

pc::LayoutResult pc::current_layout() {
    Context& ctx = context();
    int width = 0;
    int height = 0;
    SDL_GetCurrentRenderOutputSize(ctx.renderer, &width, &height);
    const int scale = ctx.fit || ctx.fullscreen ? fit_scale(ctx.layout, width, height) : ctx.scale;
    return compute_layout(ctx.layout, scale, width, height);
}

void pc::compose_screens() {
    Context& ctx = context();
    SDL_SetRenderTarget(ctx.renderer, nullptr);
    const LayoutResult layout = current_layout();
    SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_NONE);
    set_draw_color(rgb(12, 12, 12)); // around the screens
    SDL_RenderClear(ctx.renderer);
    copy_screen(ctx.renderer, ctx.screens[0], kScreenWidth[0], layout.top);
    copy_screen(ctx.renderer, ctx.screens[1], kScreenWidth[1], layout.bottom);
    SDL_SetRenderDrawBlendMode(ctx.renderer, SDL_BLENDMODE_BLEND);
}

} // namespace nnh::platform
