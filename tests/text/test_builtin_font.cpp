// The built-in NNHGE font data (text/builtin_font.hpp, spec 005 FR-1, FR-2).

#include "test.hpp"
#include "text/builtin_font.hpp"
#include "text/glyphs.hpp"

using nnh::detail::glyph_index;
using nnh::detail::is_space;
using nnh::detail::kBuiltinFont;
using nnh::detail::kGlyphCount;
using nnh::detail::kReplacementGlyph;

namespace {

int pixels(int glyph) {
    int n = 0;
    for (int row = 0; row < 8; ++row) {
        for (std::uint8_t bits = kBuiltinFont[glyph][row]; bits != 0; bits &= bits - 1) ++n;
    }
    return n;
}

bool same(int a, int b) {
    for (int row = 0; row < 8; ++row) {
        if (kBuiltinFont[a][row] != kBuiltinFont[b][row]) return false;
    }
    return true;
}

char32_t code_point_of(int glyph) {
    if (glyph < 95) return static_cast<char32_t>(32 + glyph);
    if (glyph == kReplacementGlyph) return 0xFFFD;
    return static_cast<char32_t>(160 + glyph - 96);
}

} // namespace

NNH_TEST(every_glyph_but_the_spaces_has_pixels) {
    for (int g = 0; g < kGlyphCount; ++g) {
        const bool space = is_space(code_point_of(g));
        CHECK(space ? pixels(g) == 0 : pixels(g) > 0);
    }
}

NNH_TEST(glyphs_leave_the_first_and_last_columns_empty) {
    for (int g = 0; g < kGlyphCount; ++g) {
        for (int row = 0; row < 8; ++row) CHECK((kBuiltinFont[g][row] & 0x81) == 0);
    }
}

NNH_TEST(accented_letters_differ_from_their_base_letter) {
    const char32_t pairs[][2] = {{U'á', U'a'}, {U'ã', U'a'}, {U'ç', U'c'}, {U'é', U'e'}, {U'õ', U'o'},
                                 {U'Á', U'A'}, {U'Ç', U'C'}, {U'Ñ', U'N'}, {U'ü', U'u'}, {U'í', U'i'}};
    for (const auto& p : pairs) CHECK(!same(glyph_index(p[0]), glyph_index(p[1])));
}

NNH_TEST(similar_characters_stay_distinct) {
    const char32_t pairs[][2] = {{U'0', U'O'}, {U'1', U'l'}, {U'1', U'I'}, {U'5', U'S'}, {U'8', U'B'},
                                 {U'o', U'a'}, {U'v', U'y'}, {U'á', U'à'}, {U'ä', U'â'}};
    for (const auto& p : pairs) CHECK(!same(glyph_index(p[0]), glyph_index(p[1])));
    for (int g = 0; g < kGlyphCount; ++g) {
        if (g != kReplacementGlyph && pixels(g) > 0) CHECK(!same(g, kReplacementGlyph));
    }
}

NNH_TEST(the_letter_a_has_its_known_shape) {
    // Guards the bit order: column 1 is bit 6 (0x40), the glyph's top row is row 1.
    const std::uint8_t a[8] = {0x00, 0x38, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x00};
    for (int row = 0; row < 8; ++row) CHECK(kBuiltinFont[glyph_index(U'A')][row] == a[row]);
}
