// PC simulator: textures from the PNGs embedded by the build (contract nnh/platform/video.hpp).
// A texture is an SDL_Texture of the window's renderer (on an SDL_GPUDevice when available: the same
// device the future 3D renderer will use, docs/architecture/rendering.md).

#include <SDL3/SDL.h>

#include "context_pc.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {

bool texture_create(const void* data, std::size_t size, TextureInfo& out) {
    out = TextureInfo{};
    SDL_IOStream* io = SDL_IOFromConstMem(data, size);
    SDL_Surface* surface = io != nullptr ? SDL_LoadPNG_IO(io, true) : nullptr;
    if (surface == nullptr) {
        pc::set_error("PNG decode failed");
        return false;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(pc::context().renderer, surface);
    const int width = surface->w;
    const int height = surface->h;
    SDL_DestroySurface(surface);
    if (texture == nullptr) {
        pc::set_error("texture creation failed");
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST); // sharp pixel art, like the 3DS backend
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    out.handle = reinterpret_cast<TextureHandle>(texture);
    out.width = width;
    out.height = height;
    out.bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4; // RGBA8, as on 3DS
    return true;
}

bool texture_create_rgba(const void* pixels, int width, int height, TextureInfo& out) {
    out = TextureInfo{};
    SDL_Texture* texture = SDL_CreateTexture(pc::context().renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture == nullptr || !SDL_UpdateTexture(texture, nullptr, pixels, width * 4)) {
        pc::set_error("texture creation failed");
        SDL_DestroyTexture(texture);
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    out.handle = reinterpret_cast<TextureHandle>(texture);
    out.width = width;
    out.height = height;
    out.bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    return true;
}

void texture_destroy(TextureHandle handle) {
    if (handle != 0) SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(handle));
}

} // namespace nnh::platform
