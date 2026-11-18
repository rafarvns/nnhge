// Text measuring (layout.hpp).

#include "text/layout.hpp"

namespace nnh::detail {

int clamp_text_scale(int scale) { return scale < 1 ? 1 : (scale > 8 ? 8 : scale); }

int line_cells(const char* p) {
    int cells = 0;
    char32_t c = 0;
    while ((c = next_codepoint(p)) != 0 && c != U'\n') {
        if (c != U'\r') ++cells;
    }
    return cells;
}

Vec2 measure(const char* utf8, int scale) {
    if (utf8 == nullptr || *utf8 == '\0') return {};
    scale = clamp_text_scale(scale);
    int widest = 0;
    int lines = 1;
    const char* p = utf8;
    for (;;) {
        const int cells = line_cells(p);
        if (cells > widest) widest = cells;
        // Skip to the next line.
        char32_t c = 0;
        while ((c = next_codepoint(p)) != 0 && c != U'\n') {
        }
        if (c == 0) break;
        ++lines;
    }
    return Vec2{static_cast<float>(widest * kFontCellSize * scale), static_cast<float>(lines * kFontLineHeight * scale)};
}

} // namespace nnh::detail
