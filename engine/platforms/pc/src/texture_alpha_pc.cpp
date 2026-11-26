// PC simulator: coverage textures (contract nnh/platform/video.hpp texture_create_alpha, spec 029 D49). Expanded once
// to white RGBA8 texels with the coverage as alpha, so the renderer's tint multiplies them like any sprite; accounted
// as one byte per pixel, what the console uses (GPU_LA4), so the simulator enforces the console's numbers (D4).

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdlib>

#include "context_pc.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::platform {

bool texture_create_alpha(const void* coverage, int width, int height, TextureInfo& out) {
    out = TextureInfo{};
    if (coverage == nullptr || width < 8 || height < 8 || width > 1024 || height > 1024) {
        pc::set_error("texture_create_alpha: invalid arguments");
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    auto* rgba = static_cast<std::uint8_t*>(std::malloc(pixels * 4));
    if (rgba == nullptr) {
        pc::set_error("texture_create_alpha: out of memory");
        return false;
    }
    const auto* a = static_cast<const std::uint8_t*>(coverage);
    for (std::size_t i = 0; i < pixels; ++i) {
        rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = a[i];
    }
    SDL_Texture* texture =
        SDL_CreateTexture(pc::context().renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    const bool ok = texture != nullptr && SDL_UpdateTexture(texture, nullptr, rgba, width * 4);
    std::free(rgba);
    if (!ok) {
        pc::set_error("texture creation failed");
        SDL_DestroyTexture(texture);
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    out.handle = reinterpret_cast<TextureHandle>(texture);
    out.width = width;
    out.height = height;
    out.bytes = pixels;
    return true;
}

} // namespace nnh::platform
