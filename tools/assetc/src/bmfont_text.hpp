#pragma once
// Reading AngelCode BMFont files (spec 029 FR-1; plan "BMFont"): the text and the XML forms through one attribute
// tokenizer (`key=value`, quoted strings), bounded lines; the binary form is refused. Tests: tests/test_bmfont_text.cpp.

#include <string>
#include <string_view>
#include <vector>

#include "diagnostics.hpp"

namespace nnh::assetc {

struct BmChar {
    int id = 0, x = 0, y = 0, w = 0, h = 0, xoffset = 0, yoffset = 0, xadvance = 0, page = 0;
    int line = 0;
};
struct BmKerning {
    int first = 0, second = 0, amount = 0;
};
struct BmFont {
    int size = 0, line_height = 0, base = 0, pages = 0;
    bool packed = false;
    std::vector<std::string> page_files; ///< by page id
    std::vector<BmChar> chars;
    std::vector<BmKerning> kernings;
};

inline constexpr std::size_t kMaxBmFontBytes = 4u << 20;
inline constexpr std::size_t kMaxBmFontLine = 4096;

/// Reads a BMFont's text or XML form. @return false (problems added with their lines) when it isn't usable.
bool read_bmfont(std::string_view text, BmFont& out, Diagnostics& problems);

} // namespace nnh::assetc
