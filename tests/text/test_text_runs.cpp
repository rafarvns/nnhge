// Faces and runs (spec 029 FR-6, FR-7, FR-12; plan "Text layout"): the pen walk of one line through the built-in font
// or a game font — advances, kerning within one font, ink past the advance, zero-advance characters, alignment.
// Expected pens are computed by hand from the fixture's metrics.

#include <vector>

#include "../font/font_fixture.hpp"
#include "font/font_block.hpp"
#include "test.hpp"
#include "text/text_runs.hpp"

using namespace nnh::detail;
using font_test::FontBuilder;

namespace {

struct Block {
    std::vector<std::uint64_t> mem;
    const FontBlock* font = nullptr;
};

Block make(const FontBuilder& b) {
    Block out;
    const auto f = b.build();
    const font::Header h = font::read_header(f.data());
    out.mem.assign(font_block_bytes(h) / 8 + 1, 0);
    out.font = font_block_init(out.mem.data(), font_block_bytes(h), f.data(), h.off_page_data);
    return out;
}

/// A face over one font.
TextFace face_of(const Block& b) {
    TextFace f;
    f.fonts[0] = b.font;
    f.count = 1;
    return f;
}

struct Walk {
    std::vector<int> pens, advances, bytes;
    std::vector<bool> drawn;
    int width = 0;
};

Walk walk(const TextFace& face, const char* text) {
    Walk w;
    const char* end = text;
    while (*end != 0) ++end;
    w.width = walk_run(face, text, end, [&](const RunItem& it) {
        w.pens.push_back(it.pen);
        w.advances.push_back(it.advance);
        w.bytes.push_back(it.byte);
        w.drawn.push_back(it.draw);
    });
    return w;
}

FontBuilder kerned() {
    FontBuilder b;
    b.glyph(U' ', 0, 0, 0, 0, 0, 0, 4);
    b.glyph(U'A', 0, 0, 7, 9, 0, 2, 8).glyph(U'V', 8, 0, 7, 9, 0, 2, 8).glyph(U'a', 16, 0, 5, 7, 0, 4, 6);
    b.glyph(U'b', 24, 0, 5, 9, 0, 2, 6).glyph(U'f', 32, 0, 10, 9, 1, 2, 6); // 'f' inks past its advance
    b.glyph(U'j', 48, 0, 4, 11, -1, 4, 4);                                  // a negative x offset
    b.kern(U'A', U'V', -1).kern(U'V', U'A', -2).kern(U'A', U' ', 3);
    return b;
}

} // namespace

NNH_TEST(text_runs_builtin_face_is_one_cell_per_code_point) {
    const TextFace builtin;
    const Walk w = walk(builtin, "ab c\xC3\xA9");
    CHECK((w.pens == std::vector<int>{0, 8, 16, 24, 32}));
    CHECK((w.drawn == std::vector<bool>{true, true, false, true, true})); // the space draws nothing
    CHECK((w.bytes == std::vector<int>{0, 1, 2, 3, 4}));
    CHECK(w.width == 40);
}

NNH_TEST(text_runs_builtin_face_skips_carriage_returns) {
    const TextFace builtin;
    const Walk w = walk(builtin, "a\rb");
    CHECK((w.pens == std::vector<int>{0, 8}) && (w.bytes == std::vector<int>{0, 2}) && w.width == 16);
}

NNH_TEST(text_runs_font_advances_and_kerning_pairs) {
    const Block b = make(kerned());
    const Walk w = walk(face_of(b), "AVA a");
    // A at 0 (adv 8); V: 8 − 1 = 7 (adv 8); A: 15 − 2 = 13; ' ': 21 + 3 = 24 (adv 4); a at 28
    CHECK((w.pens == std::vector<int>{0, 7, 13, 24, 28}));
    CHECK((w.drawn == std::vector<bool>{true, true, true, false, true}));
    CHECK(w.width == 34);
}

NNH_TEST(text_runs_width_includes_ink_past_the_last_advance) {
    const Block b = make(kerned());
    CHECK(walk(face_of(b), "af").width == 6 + 1 + 10); // 'f' at 6: ink to 6 + 1 + 10 = 17 > pen 12
    CHECK(walk(face_of(b), "fa").width == 12);         // 'f''s ink (11) is inside the line
    CHECK(walk(face_of(b), "j").width == 4);           // ink −1 + 4 = 3 < advance 4
}

NNH_TEST(text_runs_combining_marks_and_joiners_advance_nothing) {
    const Block b = make(kerned());
    const Walk w = walk(face_of(b), "a\xCC\x81" "b\xE2\x80\x8D" "a"); // a, U+0301, b, ZWJ, a
    CHECK((w.pens == std::vector<int>{0, 6, 6, 12, 12}));
    CHECK((w.advances == std::vector<int>{6, 0, 6, 0, 6}));
    CHECK((w.drawn == std::vector<bool>{true, false, true, false, true}));
    CHECK(w.width == 18);
}

NNH_TEST(text_runs_kerning_is_kept_across_zero_width_characters_only) {
    const Block b = make(kerned());
    CHECK((walk(face_of(b), "A\xCC\x81V").pens == std::vector<int>{0, 8, 7})); // the mark is transparent
    CHECK((walk(face_of(b), "AbV").pens == std::vector<int>{0, 8, 14}));      // no pair (A, b) or (b, V)
}

NNH_TEST(text_runs_alignment_offsets_round_the_centre_down) {
    CHECK(align_offset(nnh::TextAlign::Left, 15) == 0);
    CHECK(align_offset(nnh::TextAlign::Center, 15) == -7);
    CHECK(align_offset(nnh::TextAlign::Center, 16) == -8);
    CHECK(align_offset(nnh::TextAlign::Right, 15) == -15);
}

NNH_TEST(text_runs_face_metrics_of_the_builtin_and_a_font) {
    const TextFace builtin;
    CHECK(face_line_height(builtin) == 10 && face_ascent(builtin) == 8 && face_size(builtin) == 8);
    const Block b = make(kerned());
    CHECK(face_line_height(face_of(b)) == 20 && face_ascent(face_of(b)) == 15 && face_size(face_of(b)) == 16);
}
