// Nintendo 3DS: tile batches with citro2d (contract nnh/platform/video.hpp, decision D26). Quads arrive in
// whole screen pixels; each is one C2D_DrawImage with angle 0 and no pivot, the texture's 1/size computed
// once per batch and one shared tint.
//
// Flips: x/y mirrors are a negative size, as for sprites (citro2d swaps the corner texcoords). The diagonal
// flip (swap x and y, square tiles) uses tex3ds' rotated sub-texture rule: when top < bottom, a corner reads
// (u, v) = (top or bottom, left or right). With left = v of the tile's top row, right = v of its bottom row,
// top = u of its left column and bottom = u of its right column, the screen's top-right corner shows the
// tile's bottom-left: a transpose. The mirrors then apply to that, as Tiled does.

#include "context_3ds.hpp"
#include "nnh/platform/video.hpp"

#include <3ds.h>
#include <citro2d.h>

namespace nnh::platform {
namespace {

constexpr float kDepth = 0.5f; // order comes from submission, as in draw2d_3ds.cpp

bool is_white(Color c) {
    return c.r == 255 && c.g == 255 && c.b == 255 && c.a == 255;
}

} // namespace

void draw_tiles(Screen screen, const TileBatch2D& batch) {
    if (batch.texture == 0 || batch.count <= 0) return;
    ctr::draw_on(screen);
    C3D_Tex* tex = ctr::texture_of(batch.texture);
    const float inv_w = 1.0f / static_cast<float>(tex->width);
    const float inv_h = 1.0f / static_cast<float>(tex->height);
    const float w = static_cast<float>(batch.tile_w);
    const float h = static_cast<float>(batch.tile_h);
    const bool tinted = !is_white(batch.color);
    C2D_ImageTint tint;
    if (tinted) {
        C2D_SetTintMode(C2D_TintMult);
        C2D_PlainImageTint(&tint, ctr::to_c2d(batch.color), 1.0f);
    }
    Tex3DS_SubTexture sub{static_cast<u16>(batch.tile_w), static_cast<u16>(batch.tile_h), 0, 0, 0, 0};
    const C2D_Image image{tex, &sub};
    C2D_DrawParams params{};
    params.depth = kDepth;
    for (int i = 0; i < batch.count; ++i) {
        const TileQuad2D& q = batch.quads[i];
        // citro3d textures are stored bottom-up: v = 1 at the top row (as draw2d_3ds.cpp).
        const float l = static_cast<float>(q.src_x) * inv_w;
        const float r = static_cast<float>(q.src_x + batch.tile_w) * inv_w;
        const float t = 1.0f - static_cast<float>(q.src_y) * inv_h;
        const float b = 1.0f - static_cast<float>(q.src_y + batch.tile_h) * inv_h;
        if ((q.orient & kTileOrientDiagonal) == 0) {
            sub.left = l;
            sub.right = r;
            sub.top = t;
            sub.bottom = b;
        } else {
            sub.left = t;
            sub.right = b;
            sub.top = l;
            sub.bottom = r;
        }
        params.pos.x = static_cast<float>(q.x);
        params.pos.y = static_cast<float>(q.y);
        params.pos.w = (q.orient & kTileOrientFlipX) != 0 ? -w : w;
        params.pos.h = (q.orient & kTileOrientFlipY) != 0 ? -h : h;
        C2D_DrawImage(image, &params, tinted ? &tint : nullptr);
    }
    if (tinted) C2D_SetTintMode(C2D_TintSolid); // citro2d's default, used by the debug text
}

} // namespace nnh::platform
