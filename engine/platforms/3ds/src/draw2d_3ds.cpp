// Nintendo 3DS: one 2D pass with citro2d (contract nnh/platform/video.hpp,
// docs/architecture/rendering.md). Items arrive sorted, camera-adjusted and in whole screen pixels;
// this file only maps them to citro2d. The render target clips each screen.

#include <3ds.h>
#include <citro2d.h>

#include "context_3ds.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {
namespace {

constexpr float kDepth = 0.5f; // order comes from submission, not depth

/// Texture coordinates of a region. citro3d textures are stored bottom-up, so v = 1 at the top row
/// (tex3ds' convention; top > bottom means "not rotated").
Tex3DS_SubTexture subtexture(const C3D_Tex& tex, const Region& r) {
    const float w = static_cast<float>(tex.width);
    const float h = static_cast<float>(tex.height);
    return Tex3DS_SubTexture{static_cast<u16>(r.w),
                             static_cast<u16>(r.h),
                             static_cast<float>(r.x) / w,
                             1.0f - static_cast<float>(r.y) / h,
                             static_cast<float>(r.x + r.w) / w,
                             1.0f - static_cast<float>(r.y + r.h) / h};
}

void draw_sprite(const DrawItem2D& item) {
    const Tex3DS_SubTexture sub = subtexture(*ctr::texture_of(item.texture), item.region);
    const C2D_Image image{ctr::texture_of(item.texture), &sub};
    // pos = where the pivot (center) lands; a negative size mirrors inside the rectangle, like SDL's flips.
    C2D_DrawParams params{};
    params.pos.x = item.x;
    params.pos.y = item.y;
    params.pos.w = item.flip_x ? -item.w : item.w;
    params.pos.h = item.flip_y ? -item.h : item.h;
    params.center.x = item.pivot_x;
    params.center.y = item.pivot_y;
    params.depth = kDepth;
    params.angle = item.rotation; // radians, clockwise on the y-down screen (same as the PC)
    const Color c = item.color;
    if (c.r == 255 && c.g == 255 && c.b == 255 && c.a == 255) {
        C2D_DrawImage(image, &params, nullptr);
        return;
    }
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, ctr::to_c2d(c), 1.0f); // with C2D_TintMult: texture color × tint, alpha × tint alpha
    C2D_DrawImage(image, &params, &tint);
}

void draw_outline(const DrawItem2D& item) {
    // 1-pixel frame inside the rectangle, like SDL_RenderRect.
    const u32 color = ctr::to_c2d(item.color);
    const float x = item.w < 0 ? item.x + item.w : item.x;
    const float y = item.h < 0 ? item.y + item.h : item.y;
    const float w = item.w < 0 ? -item.w : item.w;
    const float h = item.h < 0 ? -item.h : item.h;
    C2D_DrawRectSolid(x, y, kDepth, w, 1, color);
    if (h > 1) C2D_DrawRectSolid(x, y + h - 1, kDepth, w, 1, color);
    if (h > 2) {
        C2D_DrawRectSolid(x, y + 1, kDepth, 1, h - 2, color);
        if (w > 1) C2D_DrawRectSolid(x + w - 1, y + 1, kDepth, 1, h - 2, color);
    }
}

} // namespace

void draw_2d(Screen screen, const DrawItem2D* items, int count) {
    ctr::draw_on(screen);
    C2D_SetTintMode(C2D_TintMult);
    for (int i = 0; i < count; ++i) {
        const DrawItem2D& item = items[i];
        switch (item.kind) {
        case DrawItem2D::Kind::Sprite:
            if (item.texture != 0) draw_sprite(item);
            break;
        case DrawItem2D::Kind::FillRect:
            C2D_DrawRectSolid(item.x, item.y, kDepth, item.w, item.h, ctr::to_c2d(item.color));
            break;
        case DrawItem2D::Kind::RectOutline:
            draw_outline(item);
            break;
        case DrawItem2D::Kind::Line: {
            // Through the pixel centers, 1 pixel thick, like SDL_RenderLine.
            const u32 color = ctr::to_c2d(item.color);
            C2D_DrawLine(item.x + 0.5f, item.y + 0.5f, color, item.w + 0.5f, item.h + 0.5f, color, 1.0f, kDepth);
            break;
        }
        }
    }
    C2D_SetTintMode(C2D_TintSolid); // citro2d's default, used by the debug text
}

} // namespace nnh::platform
