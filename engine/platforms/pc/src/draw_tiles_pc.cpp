// PC simulator: tile batches (contract nnh/platform/video.hpp, decision D26). Quads arrive in whole screen
// pixels; each is one SDL texture copy (SDL's renderer batches consecutive copies of one texture).
//
// Flips: SDL mirrors a copy first, then rotates it around its centre. Tiled's diagonal flip (swap x and y,
// square tiles) followed by its x/y mirrors is the same as a 90° clockwise turn of a copy mirrored by
// (x: flip_y, y: !flip_x) — derived in docs/tech/sdl3.md and checked pixel by pixel by
// tests/platform_conformance/test_draw_tiles_pc.cpp.
//
// Zoom (decision D33): a quad covers {x, y, dst_w + widen, dst_h + taller}. SDL rotates a copy about its
// destination's centre, so a diagonal quad with unequal sides passes the pre-swapped rectangle
// {x + (w − h) / 2, y + (h − w) / 2, h, w}, whose quarter turn covers exactly {x, y, w, h}.

#include "context_pc.hpp"
#include "nnh/platform/video.hpp"

#include <SDL3/SDL.h>

namespace nnh::platform {
namespace {

SDL_FlipMode flip_mode(bool horizontal, bool vertical) {
    int flip = SDL_FLIP_NONE;
    if (horizontal) flip |= SDL_FLIP_HORIZONTAL;
    if (vertical) flip |= SDL_FLIP_VERTICAL;
    return static_cast<SDL_FlipMode>(flip);
}

} // namespace

void draw_tiles(Screen screen, const TileBatch2D& batch) {
    if (batch.texture == 0 || batch.count <= 0) return;
    SDL_Renderer* renderer = pc::context().renderer;
    pc::draw_on(screen);
    auto* texture = reinterpret_cast<SDL_Texture*>(batch.texture);
    SDL_SetTextureColorMod(texture, batch.color.r, batch.color.g, batch.color.b);
    SDL_SetTextureAlphaMod(texture, batch.color.a);
    const float sw = static_cast<float>(batch.tile_w);
    const float sh = static_cast<float>(batch.tile_h);
    const float dw = static_cast<float>(batch.dst_w);
    const float dh = static_cast<float>(batch.dst_h);
    for (int i = 0; i < batch.count; ++i) {
        const TileQuad2D& q = batch.quads[i];
        const SDL_FRect source{static_cast<float>(q.src_x), static_cast<float>(q.src_y), sw, sh};
        const float w = (q.orient & kTileOrientWiden) != 0 ? dw + 1 : dw;
        const float h = (q.orient & kTileOrientTaller) != 0 ? dh + 1 : dh;
        const SDL_FRect dest{static_cast<float>(q.x), static_cast<float>(q.y), w, h};
        const bool fx = (q.orient & kTileOrientFlipX) != 0;
        const bool fy = (q.orient & kTileOrientFlipY) != 0;
        if ((q.orient & kTileOrientDiagonal) == 0) {
            if ((q.orient & 7) == 0) {
                SDL_RenderTexture(renderer, texture, &source, &dest);
            } else {
                SDL_RenderTextureRotated(renderer, texture, &source, &dest, 0.0, nullptr, flip_mode(fx, fy));
            }
        } else {
            const SDL_FRect turned{dest.x + (w - h) * 0.5f, dest.y + (h - w) * 0.5f, h, w}; // = dest when w == h
            SDL_RenderTextureRotated(renderer, texture, &source, &turned, 90.0, nullptr, flip_mode(fy, !fx));
        }
    }
}

} // namespace nnh::platform
