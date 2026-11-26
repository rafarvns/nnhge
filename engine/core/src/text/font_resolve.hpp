#pragma once
// How gfx and the UI reach game fonts (spec 029 D49): a FontRef id becomes a text face through g_font_resolve, a
// function pointer the first load_font() sets — no static initialiser, so a game that never loads a font links no font
// code, and its FontRefs are all the built-in font.

#include <cstdint>

#include "nnh/gfx.hpp"
#include "text/text_face.hpp"

namespace nnh::detail {

/// Fills `face` with the font of `id` and its fallbacks. @return false for a stale id.
using FontResolve = bool (*)(std::uint32_t id, TextFace& face);
extern FontResolve g_font_resolve;

/// The face of `font` at `scale` (clamped to 1..8): the built-in face for FontRef{} or before any font was loaded; a
/// stale FontRef asserts in Debug/Development and is the built-in face in Release.
TextFace face_for(FontRef font, int scale);

} // namespace nnh::detail
