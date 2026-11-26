#pragma once
// A BMFont's glyphs (spec 029 FR-1, FR-2; plan "BMFont"): its pages `<font>_<anything>.png` beside the `.fnt` decoded and
// cut into glyph bitmaps, re-packed later under D49's rules. Mono when every texel is white (coverage in alpha) or opaque
// grey (coverage in grey): 16 levels; any other colour makes a colour font (RGBA8). Metrics: line height = lineHeight,
// ascent = base, descent = base − lineHeight. Tests: tests/test_bmfont_convert.cpp.

#include <string>
#include <vector>

#include "files.hpp"
#include "font_glyphs.hpp"

namespace nnh::assetc {

/// @return "" or the problems ("file:line: reason" / "file: reason").
std::string read_bmfont_glyphs(FileSource& files, const std::string& path, BakedFont& out,
                               std::vector<std::string>& inputs);

} // namespace nnh::assetc
