#pragma once
// Writing a converted font (spec 029 FR-2, D49): a packed BakedFont → `.nnhfont` bytes through the engine's own writer
// (engine/core/src/font/font_format.cpp), so the console's validator reads exactly what was written: glyphs sorted by code
// point, break bits from text/break_class.cpp, kerning runs per left glyph, the replacement (U+FFFD, else '?') and
// ellipsis (U+2026) glyphs, fallbacks, the licence path, pages as 4-bit coverage (or RGBA8). Tests: tests/test_cli_font.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "font_glyphs.hpp"
#include "glyph_pack.hpp"

namespace nnh::assetc {

/// @return "" (and the bytes in `out`) or the problem (a value outside the format's ranges).
std::string write_font(BakedFont& font, const std::vector<PageSize>& pages, std::vector<std::uint8_t>& out);

} // namespace nnh::assetc
