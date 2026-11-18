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
    Tex3DS_Texture t3x; // keeps the subtexture table tex3ds allocated
};

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

void texture_destroy(TextureHandle handle) {
    if (handle == 0) return;
    auto* texture = reinterpret_cast<Texture3ds*>(handle);
    C3D_TexDelete(&texture->tex);
    Tex3DS_TextureFree(texture->t3x);
    delete texture;
}

C3D_Tex* ctr::texture_of(TextureHandle handle) { return &reinterpret_cast<Texture3ds*>(handle)->tex; }

} // namespace nnh::platform
