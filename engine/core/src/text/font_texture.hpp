#pragma once
// The built-in font's texture: created for every game at nnh::init(), released at shutdown. Internal.

#include <cstddef>
#include <cstdint>

#include "nnh/memory.hpp"

namespace nnh::detail {

/// Builds the atlas and creates the texture in the texture table (gfx_init). @return false if the
/// platform couldn't create it.
bool font_texture_create();

/// Releases it (gfx_shutdown, before the remaining textures).
void font_texture_release();

bool font_texture_ready();
int font_texture_slot();
std::uint32_t font_texture_generation();
std::size_t font_texture_bytes();

/// Memory tag of the font texture ("font").
MemTag& font_memory_tag();

} // namespace nnh::detail
