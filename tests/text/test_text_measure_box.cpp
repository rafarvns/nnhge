// Measuring equals drawing (spec 029 FR-8, FR-10): no item draw_text records lies outside the box measure_text gives
// (the documented extents aside: the shadow one scaled pixel right and down, outline glyphs their own boxes, ink left of
// a negative x offset), over 10 000 seeded strings × fonts × scales × wraps × line limits × alignments.

#include <cstdint>
#include <vector>

#include "../font/font_load_fixture.hpp"
#include "nnh/font.hpp"
#include "nnh/gfx.hpp"
#include "test.hpp"

using namespace font_test;
namespace fake = nnh::platform_fake;

namespace {

/// A string of the fixture's characters, spaces, '\n', hyphens, CJK the font lacks, accents, now and then bad bytes.
void make(std::uint32_t& seed, char* out, int capacity) {
    static const char* const kPieces[] = {"A", "V", "j", "f", "a", " ", " ", "-", "\n", "\xE4\xB8\x80", "\xC3\xA9",
                                          "\xCC\x81", "\xFF", "AV", "Vo", "?"};
    auto next = [&] { return (seed = seed * 1664525u + 1013904223u) >> 8; };
    const int pieces = static_cast<int>(next() % 24);
    int n = 0;
    for (int i = 0; i < pieces; ++i) {
        const char* p = kPieces[next() % (sizeof kPieces / sizeof kPieces[0])];
        while (*p != 0 && n + 1 < capacity) out[n++] = *p++;
    }
    out[n] = 0;
}

FontBuilder font() {
    FontBuilder b;
    b.glyph(U' ', 0, 0, 0, 0, 0, 0, 4).glyph(U'-', 56, 0, 4, 2, 0, 8, 5).glyph(U'?', 16, 0, 5, 9, 1, 2, 6);
    b.glyph(U'A', 0, 0, 7, 9, 0, 2, 8).glyph(U'V', 8, 0, 7, 9, 0, 2, 8).glyph(U'a', 32, 0, 5, 7, 0, 4, 6);
    b.glyph(U'f', 40, 12, 10, 9, 1, 2, 6).glyph(U'j', 24, 0, 4, 11, 0, 4, 4).glyph(U'o', 48, 12, 5, 7, 0, 4, 6);
    b.glyph(0xE9, 32, 12, 5, 9, 0, 2, 6).glyph(0x2026, 0, 24, 9, 2, 0, 12, 10);
    b.kern(U'A', U'V', -1).kern(U'V', U'o', -2);
    return b;
}

} // namespace

NNH_TEST(text_measure_box_holds_every_drawn_item) {
    start_engine({.max_draws = 4096});
    add_font("fonts/m", font());
    auto loaded = nnh::load_font("fonts/m");
    CHECK(loaded.ok());
    if (!loaded) return;
    const nnh::Font f = static_cast<nnh::Font&&>(loaded.value());
    std::uint32_t seed = 81029;
    char text[200];
    int outside = 0, items = 0;
    for (int round = 0; round < 10000; ++round) {
        make(seed, text, sizeof text);
        nnh::TextOptions o;
        o.font = (seed & 1u) != 0 ? nnh::FontRef(f) : nnh::FontRef{};
        o.scale = 1 + static_cast<int>((seed >> 3) % 3);
        o.wrap_width = (seed >> 6) % 3 == 0 ? 0 : 8 + static_cast<int>((seed >> 9) % 120);
        o.max_lines = static_cast<int>((seed >> 16) % 4);
        o.align = static_cast<nnh::TextAlign>((seed >> 20) % 3);
        const nnh::Vec2 box = nnh::gfx::measure_text(text, o);
        const float left = o.align == nnh::TextAlign::Left ? 0 : (o.align == nnh::TextAlign::Center ? -box.x / 2 : -box.x);
        CHECK(nnh::frame());
        nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, text, o);
        CHECK(nnh::frame());
        const auto& s = fake::state();
        for (int i = 0; i < s.last_frame_item_count; ++i) {
            const nnh::platform::DrawItem2D& it = s.last_frame_items[i];
            ++items;
            // centred lines round their offset down: up to half a pixel × scale left of box.x / 2
            const bool in = it.x >= left - static_cast<float>(o.scale) && it.y >= 0 &&
                            it.x + it.w <= left + box.x + static_cast<float>(o.scale) && it.y + it.h <= box.y;
            outside += in ? 0 : 1;
        }
    }
    CHECK(outside == 0);
    CHECK(items > 10000);
}

NNH_TEST(text_measure_wraps_and_cuts_like_drawing) {
    start_engine();
    add_font("fonts/m", font());
    auto loaded = nnh::load_font("fonts/m");
    CHECK(loaded.ok());
    if (!loaded) return;
    const nnh::Font f = static_cast<nnh::Font&&>(loaded.value());
    // "AV AV" at 14: two lines of "AV" (15 − 1 = 14 wide), 20 high each
    const nnh::Vec2 two = nnh::gfx::measure_text("AV AV", {.font = f, .wrap_width = 15});
    CHECK(two.x == 15 && two.y == 40);
    // one line: "aaaa" (24) + "…" (10) within 36
    const nnh::Vec2 cut = nnh::gfx::measure_text("aaaaaaaa aa", {.scale = 2, .font = f, .wrap_width = 72, .max_lines = 1});
    CHECK(cut.x == 68 && cut.y == 40);
    CHECK(nnh::gfx::measure_text("", {.font = f}).x == 0 && nnh::gfx::measure_text(nullptr, {.font = f}).y == 0);
    CHECK(nnh::gfx::measure_text("f", {.font = f}).x == 11); // ink past the advance
}
