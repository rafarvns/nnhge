// PC simulator: a texture's pixels for the CPU rasteriser (spec 026, plan P16). Read back once, at a texture's first
// 3D use (drawn into a temporary target with blending and colour modulation off, then SDL_RenderReadPixels), and kept
// as a property of the SDL texture with a cleanup callback: freed when the texture is, so a reused handle never sees
// stale pixels. texture_pc.cpp is unchanged.

#include <SDL3/SDL.h>

#include <cstdlib>
#include <cstring>

#include "context_pc.hpp"
#include "video3d_pc.hpp"

namespace nnh::platform::pc {
namespace {

constexpr const char* kProperty = "nnh.texels3d";

struct Texels {
    int width;
    int height;
    std::uint8_t rgba[1]; // width × height × 4 follow
};

void free_texels(void*, void* value) { std::free(value); }

Texels* read_back(SDL_Texture* texture) {
    SDL_Renderer* r = context().renderer;
    float fw = 0, fh = 0;
    SDL_GetTextureSize(texture, &fw, &fh);
    const int w = static_cast<int>(fw), h = static_cast<int>(fh);
    SDL_Texture* target = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, w, h);
    if (target == nullptr) return nullptr;
    SDL_Texture* previous = SDL_GetRenderTarget(r);
    SDL_BlendMode blend = SDL_BLENDMODE_BLEND;
    Uint8 cr = 255, cg = 255, cb = 255, ca = 255;
    SDL_GetTextureBlendMode(texture, &blend);
    SDL_GetTextureColorMod(texture, &cr, &cg, &cb);
    SDL_GetTextureAlphaMod(texture, &ca);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureColorMod(texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(texture, 255);
    SDL_SetRenderTarget(r, target);
    SDL_RenderTexture(r, texture, nullptr, nullptr);
    SDL_Surface* shot = SDL_RenderReadPixels(r, nullptr);
    SDL_SetRenderTarget(r, previous);
    SDL_SetTextureBlendMode(texture, blend);
    SDL_SetTextureColorMod(texture, cr, cg, cb);
    SDL_SetTextureAlphaMod(texture, ca);
    SDL_DestroyTexture(target);
    if (shot == nullptr) return nullptr;
    SDL_Surface* rgba = SDL_ConvertSurface(shot, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(shot);
    if (rgba == nullptr) return nullptr;
    auto* t = static_cast<Texels*>(std::malloc(sizeof(Texels) + static_cast<std::size_t>(w) * h * 4));
    if (t != nullptr) {
        t->width = w, t->height = h;
        for (int y = 0; y < h; ++y)
            std::memcpy(t->rgba + static_cast<std::size_t>(y) * w * 4,
                        static_cast<const std::uint8_t*>(rgba->pixels) + static_cast<std::size_t>(y) * rgba->pitch,
                        static_cast<std::size_t>(w) * 4);
    }
    SDL_DestroySurface(rgba);
    return t;
}

} // namespace

raster3d::TextureView texels3d(TextureHandle handle) {
    auto* texture = reinterpret_cast<SDL_Texture*>(handle);
    if (texture == nullptr) return {};
    const SDL_PropertiesID props = SDL_GetTextureProperties(texture);
    auto* t = static_cast<Texels*>(SDL_GetPointerProperty(props, kProperty, nullptr));
    if (t == nullptr) {
        t = read_back(texture);
        if (t == nullptr) return {};
        SDL_SetPointerPropertyWithCleanup(props, kProperty, t, &free_texels, nullptr);
    }
    return {t->rgba, t->width, t->height};
}

} // namespace nnh::platform::pc
