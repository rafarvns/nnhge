// PC simulator: one 2D pass (contract nnh/platform/video.hpp, docs/architecture/rendering.md).
// Items arrive sorted, camera-adjusted and in whole screen pixels; this file only maps them to SDL.
// SDL's renderer batches consecutive draws itself.

#include <SDL3/SDL.h>

#include "context_pc.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {
namespace {

constexpr float kRadiansToDegrees = 57.29577951308232f;

void draw_sprite(SDL_Renderer* renderer, const SDL_FRect& area, const DrawItem2D& item) {
    auto* texture = reinterpret_cast<SDL_Texture*>(item.texture);
    SDL_SetTextureColorMod(texture, item.color.r, item.color.g, item.color.b);
    SDL_SetTextureAlphaMod(texture, item.color.a);
    const SDL_FRect source{static_cast<float>(item.region.x), static_cast<float>(item.region.y),
                           static_cast<float>(item.region.w), static_cast<float>(item.region.h)};
    // The pivot lands on (x, y); rotation turns around it. Flips mirror the image inside its rectangle.
    const SDL_FRect dest{area.x + item.x - item.pivot_x, area.y + item.y - item.pivot_y, item.w, item.h};
    const SDL_FPoint center{item.pivot_x, item.pivot_y};
    int flip = SDL_FLIP_NONE;
    if (item.flip_x) flip |= SDL_FLIP_HORIZONTAL;
    if (item.flip_y) flip |= SDL_FLIP_VERTICAL;
    SDL_RenderTextureRotated(renderer, texture, &source, &dest, static_cast<double>(item.rotation * kRadiansToDegrees),
                             &center, static_cast<SDL_FlipMode>(flip));
}

SDL_FRect shape_rect(const SDL_FRect& area, const DrawItem2D& item) {
    SDL_FRect r{area.x + item.x, area.y + item.y, item.w, item.h};
    if (r.w < 0) {
        r.x += r.w;
        r.w = -r.w;
    }
    if (r.h < 0) {
        r.y += r.h;
        r.h = -r.h;
    }
    return r;
}

} // namespace

void draw_2d(Screen screen, const DrawItem2D* items, int count) {
    SDL_Renderer* renderer = pc::context().renderer;
    const SDL_FRect area = pc::screen_rect(screen);
    const SDL_Rect clip{static_cast<int>(area.x), static_cast<int>(area.y), static_cast<int>(area.w),
                        static_cast<int>(area.h)};
    SDL_SetRenderClipRect(renderer, &clip);
    for (int i = 0; i < count; ++i) {
        const DrawItem2D& item = items[i];
        switch (item.kind) {
        case DrawItem2D::Kind::Sprite:
            if (item.texture != 0) draw_sprite(renderer, area, item);
            break;
        case DrawItem2D::Kind::FillRect: {
            const SDL_FRect r = shape_rect(area, item);
            pc::set_draw_color(item.color);
            SDL_RenderFillRect(renderer, &r);
            break;
        }
        case DrawItem2D::Kind::RectOutline: {
            const SDL_FRect r = shape_rect(area, item);
            pc::set_draw_color(item.color);
            SDL_RenderRect(renderer, &r);
            break;
        }
        case DrawItem2D::Kind::Line:
            pc::set_draw_color(item.color);
            SDL_RenderLine(renderer, area.x + item.x, area.y + item.y, area.x + item.w, area.y + item.h);
            break;
        }
    }
    SDL_SetRenderClipRect(renderer, nullptr);
}

} // namespace nnh::platform
