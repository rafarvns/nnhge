// The built-in font's path stays spec 005's (spec 029 FR-7, FR-13, AC-7): with every new option at its default the items
// are those spec 005's code records (built here the way draw_text did before spec 029: one sprite of the font texture
// per glyph, shadow first); a huge wrap width — which sends the text through the shared layout — gives the same items.

#include <vector>

#include "../font/font_load_fixture.hpp"
#include "nnh/gfx.hpp"
#include "test.hpp"
#include "text/glyphs.hpp"
#include "text/layout.hpp"

using namespace font_test;
namespace fake = nnh::platform_fake;
using Item = nnh::platform::DrawItem2D;

namespace {

template <typename Fn>
std::vector<Item> frame_items(Fn&& fn) {
    CHECK(nnh::frame());
    fn();
    CHECK(nnh::frame());
    const auto& s = fake::state();
    return std::vector<Item>(s.last_frame_items, s.last_frame_items + s.last_frame_item_count);
}

bool same(const Item& a, const Item& b) {
    return a.kind == b.kind && a.texture == b.texture && a.region.x == b.region.x && a.region.y == b.region.y &&
           a.region.w == b.region.w && a.region.h == b.region.h && a.x == b.x && a.y == b.y && a.w == b.w &&
           a.h == b.h && a.layer == b.layer && a.color.r == b.color.r && a.color.g == b.color.g &&
           a.color.b == b.color.b && a.color.a == b.color.a;
}

bool same_items(const std::vector<Item>& a, const std::vector<Item>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!same(a[i], b[i])) return false;
    return true;
}

const char* const kTexts[] = {"Hello, world", "two\nlines\r\n end", "caf\xC3\xA9 \xC2\xA0 x", "\xE4\xB8\x80 box",
                              "a\xFF" "b", "  spaced  ", "x\n\n"};

} // namespace

NNH_TEST(text_builtin_draws_one_cell_per_code_point_as_spec_005) {
    start_engine();
    const auto items = frame_items([] { nnh::gfx::draw_text(nnh::Screen::Top, {10, 20}, "Ab c", {.scale = 2, .shadow = true}); });
    CHECK(items.size() == 6); // shadow + glyph for A, b, c
    if (items.size() != 6) return;
    CHECK(items[0].x == 12 && items[0].y == 22 && items[1].x == 10 && items[1].y == 20);
    CHECK(items[2].x == 28 && items[4].x == 12 + 48 && items[5].x == 58); // c after the space: 3 cells × 16
    CHECK(items[1].w == 16 && items[1].h == 16);
}

NNH_TEST(text_builtin_items_are_the_same_through_the_shared_layout) {
    start_engine();
    for (const char* text : kTexts) {
        for (int scale = 1; scale <= 3; ++scale) {
            for (const nnh::TextAlign align : {nnh::TextAlign::Left, nnh::TextAlign::Center, nnh::TextAlign::Right}) {
                nnh::TextOptions plain;
                plain.scale = scale;
                plain.align = align;
                plain.shadow = scale == 2;
                plain.layer = 2;
                nnh::TextOptions wrapped = plain;
                wrapped.wrap_width = 100000; // the shared layout, no break
                const auto a = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Bottom, {100, 50}, text, plain); });
                const auto b = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Bottom, {100, 50}, text, wrapped); });
                CHECK(same_items(a, b));
                CHECK(nnh::gfx::measure_text(text, plain).x == nnh::gfx::measure_text(text, wrapped).x);
                CHECK(nnh::gfx::measure_text(text, plain).y == nnh::gfx::measure_text(text, wrapped).y);
                CHECK(nnh::gfx::measure_text(text, plain).x == nnh::gfx::measure_text(text, scale).x);
            }
        }
    }
}
