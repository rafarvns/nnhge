#pragma once
// Font descriptions `<path>.font` (spec 029 FR-1, FR-3, FR-4; plan "Font descriptions"): spec 014's TOML subset, one
// face at one size. Tests: tests/test_font_desc.cpp.

#include <string>
#include <string_view>
#include <vector>

#include "diagnostics.hpp"

namespace nnh::assetc {

struct FontDesc {
    std::string source;                 ///< a .ttf/.otf/.ttc beside the description (relative to it)
    int face = 0;                       ///< collections: the face index
    int size = 0;                       ///< em size in pixels, 6..128
    std::vector<std::string> chars{"ascii"}; ///< named sets and "U+" code points or ranges
    std::vector<std::string> chars_from;     ///< .chars files (relative to the description)
    std::vector<std::string> strings;        ///< language codes whose string files' characters are added
    bool antialias = true;
    int threshold = 128;                ///< antialias off: coverage ≥ threshold is on
    int padding = 1;                    ///< 1..4 pixels between glyphs on a page
    int outline = 0;                    ///< 0..3: a baked outline glyph per glyph
    std::vector<std::string> fallback;  ///< ≤ 3 font asset paths
    bool replacement = true;            ///< "auto": U+FFFD if the source has it, else '?'; "none"
    std::string licence;                ///< an asset path, or "own"
    int page = 0;                       ///< 0: the smallest side that fits; else a forced side 64..1024
    int line = 0;                       ///< the line of `source` (messages)
};

/// Reads a description. @return false (problems added with their lines) when it isn't usable.
bool read_font_desc(std::string_view text, FontDesc& out, Diagnostics& problems);

} // namespace nnh::assetc
