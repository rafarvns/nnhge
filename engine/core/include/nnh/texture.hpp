#pragma once
/// @file nnh/texture.hpp
/// Textures: images on the GPU, loaded by name from the images the game declares in its build
/// (nnh_add_game(... IMAGES hero.png)). See docs/api/texture.md.
///
/// @code
/// auto hero = nnh::load_texture("hero");                 // art/hero.png
/// if (!hero) { NNH_LOG_ERROR(game, "%s", hero.error().message); return; }
/// nnh::Texture texture = std::move(hero.value());
/// nnh::gfx::draw(nnh::Screen::Top, texture, {100, 80});
/// @endcode

#include <cstdint>

#include "nnh/result.hpp"

namespace nnh {

/// A loaded texture. Move-only; the GPU memory is freed when the Texture is destroyed or reset()
/// (if it was drawn in the current frame, at the end of that frame). The same texture will serve 3D
/// rendering later. Handles are harmless after nnh::shutdown() (they just become invalid).
class Texture {
public:
    Texture() = default;
    ~Texture() { reset(); }
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    /// Frees the texture now (or at the end of the current frame). The Texture becomes empty.
    void reset();

    /// True while the texture is loaded (false for an empty Texture or after shutdown()).
    bool valid() const;
    int width() const { return width_; }
    int height() const { return height_; }

private:
    friend struct TextureAccess;
    int slot_ = -1;
    std::uint32_t generation_ = 0;
    int width_ = 0;
    int height_ = 0;
};

/// Loads an image: first the game's assets, `name` being the asset path without ".png"
/// ("characters/hero" → assets/characters/hero.png, <nnh/assets.hpp>), then the images embedded with
/// nnh_add_game(... IMAGES ...), by file name without folder or extension ("art/hero.png" → "hero").
/// Load time only (allocates GPU memory).
/// Errors: NotFound (in neither), InvalidArgument (sides not powers of two from 8 to 1024),
/// Corrupted (can't be decoded, or no GPU memory left), LimitReached (AppConfig::gfx.max_textures
/// loaded), Unsupported (before nnh::init()).
Result<Texture> load_texture(const char* name);

} // namespace nnh
