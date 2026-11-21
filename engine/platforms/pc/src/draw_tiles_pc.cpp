// PC simulator: tile batches (contract nnh/platform/video.hpp, decision D26). Quads arrive in whole screen
// pixels; each is one SDL texture copy (SDL's renderer batches consecutive copies of one texture).
//
// Flips: SDL mirrors a copy first, then rotates it around its centre. Tiled's diagonal flip (swap x and y,
// square tiles) followed by its x/y mirrors is the same as a 90° clockwise turn of a copy mirrored by
// (x: flip_y, y: !flip_x) — derived in docs/tech/sdl3.md and checked pixel by pixel by
// tests/platform_conformance/test_draw_tiles_pc.cpp.

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
    const float w = static_cast<float>(batch.tile_w);
    const float h = static_cast<float>(batch.tile_h);
    for (int i = 0; i < batch.count; ++i) {
        const TileQuad2D& q = batch.quads[i];
        const SDL_FRect source{static_cast<float>(q.src_x), static_cast<float>(q.src_y), w, h};
        const SDL_FRect dest{static_cast<float>(q.x), static_cast<float>(q.y), w, h};
        const bool fx = (q.orient & kTileOrientFlipX) != 0;
        const bool fy = (q.orient & kTileOrientFlipY) != 0;
        if ((q.orient & kTileOrientDiagonal) == 0) {
            if (q.orient == 0) {
                SDL_RenderTexture(renderer, texture, &source, &dest);
            } else {
                SDL_RenderTextureRotated(renderer, texture, &source, &dest, 0.0, nullptr, flip_mode(fx, fy));
            }
        } else {
            SDL_RenderTextureRotated(renderer, texture, &source, &dest, 90.0, nullptr, flip_mode(fy, !fx));
        }
    }
}

} // namespace nnh::platform
