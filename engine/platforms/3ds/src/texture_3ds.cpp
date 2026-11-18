// Nintendo 3DS: textures from the .t3x files embedded by the build (tex3ds output; contract
// nnh/platform/video.hpp). A texture is a citro3d C3D_Tex in linear memory: citro2d draws it now, and
// a future citro3d 3D pass can sample the same texture (docs/architecture/rendering.md).

#include <3ds.h>
#include <citro2d.h>
#include <tex3ds.h>

#include <new>

#include "context_3ds.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {
namespace {

/// What a TextureHandle points to.
struct Texture3ds {
    C3D_Tex tex;
    Tex3DS_Texture t3x; // keeps the subtexture table tex3ds allocated (nullptr for RGBA textures)
};

/// Index of pixel (x, y) inside an 8x8 tile: the PICA200 stores tiles in Morton (Z) order.
u32 morton_in_tile(u32 x, u32 y) {
    return (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
}

} // namespace

bool texture_create(const void* data, std::size_t size, TextureInfo& out) {
    out = TextureInfo{};
    auto* texture = new (std::nothrow) Texture3ds{};
    if (texture == nullptr) {
        ctr::set_error("out of memory for a texture");
        return false;
    }
    texture->t3x = Tex3DS_TextureImport(data, size, &texture->tex, nullptr, false); // false: linear memory
    if (texture->t3x == nullptr) {
        delete texture;
        ctr::set_error("Tex3DS_TextureImport failed (not a .t3x, or out of linear memory)");
        return false;
    }
    C3D_TexSetFilter(&texture->tex, GPU_NEAREST, GPU_NEAREST); // sharp pixel art, like the PC
    C3D_TexSetWrap(&texture->tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    out.handle = reinterpret_cast<TextureHandle>(texture);
    out.width = texture->tex.width;
    out.height = texture->tex.height;
    out.bytes = texture->tex.size;
    return true;
}

bool texture_create_rgba(const void* pixels, int width, int height, TextureInfo& out) {
    out = TextureInfo{};
    auto* texture = new (std::nothrow) Texture3ds{};
    if (texture == nullptr || !C3D_TexInit(&texture->tex, static_cast<u16>(width), static_cast<u16>(height), GPU_RGBA8)) {
        delete texture;
        ctr::set_error("C3D_TexInit failed (out of linear memory?)");
        return false;
    }
    // GPU layout: 8x8 tiles left to right, rows of tiles from the TOP of the image, Morton order inside a
    // tile (rows top-down too); each pixel a u32 0xRRGGBBAA. No vertical flip: verified on an Old 3DS XL
    // (2026-11-18) — flipping gave scrambled or upside-down glyphs. Sampling uses v = 1 at the image top
    // (draw2d_3ds.cpp subtexture), the same as tex3ds textures.
    const auto* src = static_cast<const u8*>(pixels);
    auto* dst = static_cast<u32*>(texture->tex.data);
    const u32 w = static_cast<u32>(width);
    const u32 h = static_cast<u32>(height);
    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            const u8* p = src + (y * w + x) * 4;
            const u32 index = (((y >> 3) * (w >> 3) + (x >> 3)) << 6) + morton_in_tile(x & 7, y & 7);
            dst[index] = (u32{p[0]} << 24) | (u32{p[1]} << 16) | (u32{p[2]} << 8) | u32{p[3]};
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

void texture_destroy(TextureHandle handle) {
    if (handle == 0) return;
    auto* texture = reinterpret_cast<Texture3ds*>(handle);
    C3D_TexDelete(&texture->tex);
    if (texture->t3x != nullptr) Tex3DS_TextureFree(texture->t3x);
    delete texture;
}

C3D_Tex* ctr::texture_of(TextureHandle handle) { return &reinterpret_cast<Texture3ds*>(handle)->tex; }

} // namespace nnh::platform
