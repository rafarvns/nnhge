#pragma once
// The font module's internals shared by its files (font.cpp, font_load.cpp, font_debug.cpp) and the text code.

#include <cstddef>
#include <cstdint>

#include "nnh/font.hpp"
#include "text/text_face.hpp"

namespace nnh {

/// Reaches into nnh::Font for the engine (friend of Font).
struct FontAccess {
    static std::uint32_t id(const Font& f) { return f.id_; }
    static Font make(std::uint32_t id) {
        Font f;
        f.id_ = id;
        return f;
    }
};

MemTag& font_block_tag(); // the `font` tag (text/font_texture.cpp)

namespace detail {

/// Drops one reference of the font `id` (and, with the last one, its fallbacks', its block and pages).
void font_release(std::uint32_t id);
/// g_font_resolve's target: the face of `id` (its block and its fallbacks').
bool font_resolve_face(std::uint32_t id, TextFace& face);
/// Bytes of a loaded font: its block and its pages as the console counts them.
std::size_t font_bytes(int slot);

} // namespace detail
} // namespace nnh
