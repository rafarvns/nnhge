// The built-in font atlas (font_atlas.hpp).

#include "text/font_atlas.hpp"

#include "text/builtin_font.hpp"

namespace nnh::detail {

void build_font_atlas(unsigned char* rgba) {
    for (int i = 0; i < kAtlasSize * kAtlasSize; ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = 0;
    }
    for (int g = 0; g < kGlyphCount; ++g) {
        const Region cell = glyph_region(g);
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 8; ++col) {
                if ((kBuiltinFont[g][row] & (0x80 >> col)) == 0) continue;
                rgba[((cell.y + row) * kAtlasSize + cell.x + col) * 4 + 3] = 255;
            }
        }
    }
}

} // namespace nnh::detail
