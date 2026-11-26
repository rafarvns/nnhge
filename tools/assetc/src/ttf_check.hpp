#pragma once
// The TrueType/OpenType pre-validator (spec 029 constitution X; plan "Rasterising"): stb_truetype does no range
// checking, so every offset it follows is checked here first — the table directory (and a collection's), head, hhea,
// maxp, hmtx, loca/glyf (every glyph's contours, flags, coordinates and composite components), cmap formats 0/4/6/12,
// kern format 0, GPOS pair adjustments, and CFF's INDEXes, top DICT, private DICTs and charstrings. CFF2 (variable CFF)
// is refused. Tests: tests/test_ttf_check.cpp.

#include <cstddef>
#include <string>

namespace nnh::assetc {

inline constexpr std::size_t kMaxFontFileBytes = 32u << 20;

/// @return "" when stb_truetype may read face `face` of `data`, else the reason.
std::string check_ttf(const unsigned char* data, std::size_t size, int face);

} // namespace nnh::assetc
