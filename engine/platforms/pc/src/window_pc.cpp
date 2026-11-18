// PC simulator: the window view (spec 006) — where the two screen targets go in the window. The layout
// math is pure (sim_layout.cpp); this file applies it with SDL.

#include <SDL3/SDL.h>

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

} // namespace

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
