// Nintendo 3DS: coverage textures (contract nnh/platform/video.hpp texture_create_alpha, spec 029 D49) as GPU_LA4, one
// byte per texel: luminance 15 (white) in the high nibble, the coverage's 4 bits in the low nibble. The PICA200 expands
// a 4-bit channel v to 8 bits as v × 17 (v << 4 | v), so the 16 levels core sends (multiples of 17) come back exactly
// and the texel is white with that alpha; draw2d_3ds.cpp's tint multiplies it like any sprite. Nibble order and
// expansion from 3dbrew's "GPU/Textures" format table (LA4: L in bits 4–7, A in bits 0–3), checked on the console
// (spec 029 AC-5).

#include <3ds.h>
#include <citro2d.h>

#include <new>

#include "context_3ds.hpp"
#include "nnh/platform/video.hpp"
#include "texture_3ds.hpp"

namespace nnh::platform {
namespace {

/// Offset of pixel (x, y) inside an 8×8 tile (Morton / Z order), indexed by y * 8 + x.
constexpr u8 kMorton[64] = {
    0,  1,  4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23, 8,  9,  12, 13, 24, 25,
    28, 29, 10, 11, 14, 15, 26, 27, 30, 31, 32, 33, 36, 37, 48, 49, 52, 53, 34, 35, 38, 39,
    50, 51, 54, 55, 40, 41, 44, 45, 56, 57, 60, 61, 42, 43, 46, 47, 58, 59, 62, 63,
};

} // namespace

bool texture_create_alpha(const void* coverage, int width, int height, TextureInfo& out) {
    out = TextureInfo{};
    if (coverage == nullptr || width < 8 || height < 8 || width > 1024 || height > 1024) {
        ctr::set_error("texture_create_alpha: invalid arguments");
        return false;
    }
    auto* texture = new (std::nothrow) ctr::Texture3ds{};
    if (texture == nullptr || !C3D_TexInit(&texture->tex, static_cast<u16>(width), static_cast<u16>(height), GPU_LA4)) {
        delete texture;
        ctr::set_error("C3D_TexInit failed (out of linear memory?)");
        return false;
    }
    // Tiles left to right, rows of tiles from the image's top (as texture_create_rgba), Morton order inside a tile.
    const auto* src = static_cast<const u8*>(coverage);
    auto* dst = static_cast<u8*>(texture->tex.data);
    const u32 w = static_cast<u32>(width);
    const u32 h = static_cast<u32>(height);
    for (u32 y = 0; y < h; ++y) {
        const u8* row = src + y * w;
        u8* tiles = dst + (((y >> 3) * (w >> 3)) << 6);
        const u8* morton = kMorton + ((y & 7) << 3);
        for (u32 x = 0; x < w; ++x) {
            tiles[((x >> 3) << 6) + morton[x & 7]] = static_cast<u8>(0xF0u | (row[x] / 17u));
        }
    }
    C3D_TexFlush(&texture->tex);
    C3D_TexSetFilter(&texture->tex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&texture->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    out.handle = reinterpret_cast<TextureHandle>(texture);
    out.width = width;
    out.height = height;
    out.bytes = texture->tex.size;
    return true;
}

} // namespace nnh::platform
