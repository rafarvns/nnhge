#pragma once
// Glyph pages as gfx textures (spec 029 D49): coverage through platform::texture_create_alpha (or RGBA8 for colour
// pages), a slot of the texture table (AppConfig::gfx.max_textures), Linear memory accounted under the caller's tag; and
// their release after the GPU is done with them (D47). Load time only.

#include <cstdint>

namespace nnh {
struct MemTag;
}

namespace nnh::detail {

/// A texture from `width` × `height` bytes of coverage (multiples of 17) or, `rgba`, 4 bytes a pixel. @return its slot
/// (and `generation`), or −1: −2 when the table is full, −1 when the platform refused (out of memory).
int texture_from_page(const std::uint8_t* pixels, int width, int height, bool rgba, MemTag& tag, std::uint32_t& generation);

/// Releases a texture made by texture_from_page(): freed now outside a frame, else after the GPU is done with every
/// frame submitted so far (D47); its accounting moves from `tag` to the textures tag first (the deferred free counts
/// it there).
void texture_release_page(int slot, std::uint32_t generation, MemTag& tag);

} // namespace nnh::detail
