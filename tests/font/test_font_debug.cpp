// Font debugging (spec 029 FR-33): Font::dump's text, debug_draw_pages' items; nothing in Release.

#include "font_load_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/font.hpp"
#include "test.hpp"

using namespace font_test;
namespace fake = nnh::platform_fake;

NNH_TEST(font_dump_logs_glyphs_pages_bytes_and_metrics) {
    start_engine();
    FontBuilder b = FontBuilder::latin();
    b.kern(U'A', U'V', -1);
    add_font("fonts/a", b);
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    if (!f) return;
    LogLines info(nnh::LogLevel::Info);
    f.value().dump();
    const char* want = "font fonts/a: 66 glyphs, 1 page 128x64 A4, size 16 line 20 ascent 15 descent -4, 1 kerning pair";
    CHECK(info.contains(want) == nnh::build::kDevToolsEnabled);
    CHECK(std::string(f.value().path()) == (nnh::build::kDevToolsEnabled ? "fonts/a" : ""));
}

namespace {

int count(nnh::platform::DrawItem2D::Kind kind) {
    int n = 0;
    for (int i = 0; i < fake::state().last_frame_item_count; ++i) n += fake::state().last_frame_items[i].kind == kind ? 1 : 0;
    return n;
}

const nnh::platform::DrawItem2D* nth(nnh::platform::DrawItem2D::Kind kind, int index) {
    for (int i = 0; i < fake::state().last_frame_item_count; ++i)
        if (fake::state().last_frame_items[i].kind == kind && index-- == 0) return &fake::state().last_frame_items[i];
    return nullptr;
}

} // namespace

NNH_TEST(font_debug_draw_pages_draws_each_page_its_outline_and_glyph_rectangles) {
    start_engine();
    FontBuilder b;
    b.page_side = 64;
    b.page_height = 32;
    b.page_count = 2;
    b.glyph(U'A', 0, 0, 8, 8, 0, 0, 9).glyph(U'B', 16, 8, 4, 6, 0, 0, 5, 1).glyph(U' ', 0, 0, 0, 0, 0, 0, 4);
    add_font("fonts/a", b);
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    if (!f) return;
    CHECK(nnh::frame());
    nnh::font::debug_draw_pages(nnh::Screen::Top, f.value(), {10, 10});
    CHECK(nnh::frame());
    using K = nnh::platform::DrawItem2D::Kind;
    const bool on = nnh::build::kDevToolsEnabled;
    CHECK(count(K::Sprite) == (on ? 2 : 0));
    // 2 page outlines + a rectangle per glyph with pixels (A, B and the replacement glyph if the font has one)
    const int glyphs = f.value().has(0xFFFD) ? 3 : 2;
    CHECK(count(K::RectOutline) == (on ? 2 + glyphs : 0));
    if (!on || count(K::Sprite) != 2) return;
    CHECK(nth(K::Sprite, 0)->x == 10 && nth(K::Sprite, 0)->w == 64 && nth(K::Sprite, 0)->h == 32); // fits: scale 1
    CHECK(nth(K::Sprite, 1)->x == 10 + 64 + 4); // the next page, 4 pixels apart
    bool a = false, bb = false;
    for (int i = 0; nth(K::RectOutline, i) != nullptr; ++i) {
        const auto* r = nth(K::RectOutline, i);
        a = a || (r->x == 10 && r->y == 10 && r->w == 8 && r->h == 8);
        bb = bb || (r->x == 10 + 64 + 4 + 16 && r->y == 18 && r->w == 4 && r->h == 6);
    }
    CHECK(a && bb);
}

NNH_TEST(font_debug_draw_pages_scales_a_page_bigger_than_the_screen_to_fit) {
    // review S4: a 512x256 page on the 400x240 top screen is drawn at 400/512 = 0.78125, glyph rectangles with it
    start_engine();
    FontBuilder b;
    b.page_side = 512;
    b.page_height = 256;
    b.glyph(U'A', 256, 128, 64, 32, 0, 0, 9);
    add_font("fonts/big", b);
    auto f = nnh::load_font("fonts/big");
    CHECK(f.ok());
    if (!f) return;
    CHECK(nnh::frame());
    nnh::font::debug_draw_pages(nnh::Screen::Top, f.value(), {0, 0});
    CHECK(nnh::frame());
    using K = nnh::platform::DrawItem2D::Kind;
    if (!nnh::build::kDevToolsEnabled) {
        CHECK(fake::state().last_frame_item_count == 0);
        return;
    }
    const auto* page = nth(K::Sprite, 0);
    CHECK(page != nullptr && page->x == 0 && page->w == 400 && page->h == 200);
    bool glyph = false, outline = false;
    for (int i = 0; nth(K::RectOutline, i) != nullptr; ++i) {
        const auto* r = nth(K::RectOutline, i);
        outline = outline || (r->x == 0 && r->y == 0 && r->w == 400 && r->h == 200);
        glyph = glyph || (r->x == 200 && r->y == 100 && r->w == 50 && r->h == 25);
    }
    CHECK(outline && glyph);
}
