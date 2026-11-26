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

NNH_TEST(font_debug_draw_pages_draws_each_page_and_its_outline) {
    start_engine();
    FontBuilder b = FontBuilder::latin();
    b.page_count = 2;
    add_font("fonts/a", b);
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    if (!f) return;
    CHECK(nnh::frame());
    nnh::font::debug_draw_pages(nnh::Screen::Top, f.value(), {10, 10});
    CHECK(nnh::frame());
    const auto& s = fake::state();
    CHECK(s.last_frame_item_count == (nnh::build::kDevToolsEnabled ? 4 : 0)); // 2 pages + 2 outlines
    if (nnh::build::kDevToolsEnabled && s.last_frame_item_count == 4) {
        CHECK(s.last_frame_items[0].kind == nnh::platform::DrawItem2D::Kind::Sprite);
        CHECK(s.last_frame_items[0].x == 10 && s.last_frame_items[0].w == 128 && s.last_frame_items[0].h == 64);
        CHECK(s.last_frame_items[2].x == 10 + 128 + 4); // the next page, 4 pixels apart
    }
}
