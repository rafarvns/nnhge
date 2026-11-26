// Drawing with game fonts (spec 029 FR-7, FR-9, FR-11, FR-12; plan "Text layout"): the items one frame records on the
// fake platform, against positions computed by hand from the fixture's metrics — pen + offset, kerning, scale, layer,
// camera and zoom, alignment, '\n', shadow and outline order, fallbacks, the replacement glyph, the hollow box.

#include <cstdio>
#include <vector>

#include "../font/font_load_fixture.hpp"
#include "nnh/font.hpp"
#include "nnh/gfx.hpp"
#include "test.hpp"

using namespace font_test;
namespace fake = nnh::platform_fake;
using Item = nnh::platform::DrawItem2D;
using Kind = Item::Kind;

namespace {

FontBuilder kerned() {
    FontBuilder b;
    b.glyph(U' ', 0, 0, 0, 0, 0, 0, 4);
    b.glyph(U'A', 0, 0, 7, 9, 0, 2, 8).glyph(U'V', 8, 0, 7, 9, 0, 2, 8).glyph(U'?', 16, 0, 5, 9, 1, 2, 6);
    b.glyph(U'j', 24, 0, 4, 11, -1, 4, 4);
    b.kern(U'A', U'V', -1);
    return b;
}

nnh::Font load(const char* path, const FontBuilder& b) {
    add_font(path, b);
    auto f = nnh::load_font(path);
    CHECK(f.ok());
    if (!f) std::printf("    load_font(%s): %s\n", path, f.error().message);
    return f ? static_cast<nnh::Font&&>(f.value()) : nnh::Font{};
}

/// The items of one frame that draws `fn`.
template <typename Fn>
std::vector<Item> frame_items(Fn&& fn) {
    CHECK(nnh::frame());
    fn();
    CHECK(nnh::frame());
    const auto& s = fake::state();
    return std::vector<Item>(s.last_frame_items, s.last_frame_items + s.last_frame_item_count);
}

bool sprite_at(const Item& it, nnh::platform::TextureHandle tex, int rx, int ry, int rw, int rh, float x, float y,
               float w, float h) {
    return it.kind == Kind::Sprite && it.texture == tex && it.region.x == rx && it.region.y == ry && it.region.w == rw &&
           it.region.h == rh && it.x == x && it.y == y && it.w == w && it.h == h;
}

bool same_color(nnh::Color a, nnh::Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

} // namespace

NNH_TEST(text_draw_font_glyphs_at_pen_plus_offset_with_kerning) {
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const auto page = last_alpha_handle();
    const auto items = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {10, 20}, "AV A", {.font = f}); });
    CHECK(items.size() == 3); // the space draws nothing
    if (items.size() != 3) return;
    CHECK(sprite_at(items[0], page, 0, 0, 7, 9, 10, 22, 7, 9));
    CHECK(sprite_at(items[1], page, 8, 0, 7, 9, 17, 22, 7, 9));  // 8 − 1
    CHECK(sprite_at(items[2], page, 0, 0, 7, 9, 29, 22, 7, 9));  // 7 + 8 + 4 (the space)
    CHECK(same_color(items[0].color, nnh::colors::white));
}

NNH_TEST(text_draw_font_scale_multiplies_pens_offsets_and_sizes) {
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const auto page = last_alpha_handle();
    const auto items = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {10, 20}, "AV", {.scale = 3, .font = f}); });
    CHECK(items.size() == 2);
    if (items.size() != 2) return;
    CHECK(sprite_at(items[0], page, 0, 0, 7, 9, 10, 26, 21, 27));
    CHECK(sprite_at(items[1], page, 8, 0, 7, 9, 31, 26, 21, 27));
}

NNH_TEST(text_draw_font_alignment_centre_rounds_down_and_right) {
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const auto page = last_alpha_handle();
    // "AV" is 15 wide: centred at 100 → 100 − 7; right → 100 − 15
    const auto c = frame_items([&] {
        nnh::gfx::draw_text(nnh::Screen::Top, {100, 0}, "AV", {.align = nnh::TextAlign::Center, .font = f});
    });
    CHECK(c.size() == 2 && sprite_at(c[0], page, 0, 0, 7, 9, 93, 2, 7, 9));
    const auto r = frame_items([&] {
        nnh::gfx::draw_text(nnh::Screen::Top, {100, 0}, "AV", {.align = nnh::TextAlign::Right, .font = f});
    });
    CHECK(r.size() == 2 && sprite_at(r[0], page, 0, 0, 7, 9, 85, 2, 7, 9));
}

NNH_TEST(text_draw_font_newlines_and_carriage_returns) {
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const auto page = last_alpha_handle();
    const auto items = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, "A\r\nV", {.font = f}); });
    CHECK(items.size() == 2);
    if (items.size() != 2) return;
    CHECK(sprite_at(items[0], page, 0, 0, 7, 9, 0, 2, 7, 9));
    CHECK(sprite_at(items[1], page, 8, 0, 7, 9, 0, 22, 7, 9)); // line height 20
}

NNH_TEST(text_draw_font_shadow_comes_first_one_scaled_pixel_down_right) {
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const auto page = last_alpha_handle();
    const nnh::Color shade = nnh::rgba(1, 2, 3, 200);
    const auto items = frame_items([&] {
        nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, "A", {.scale = 2, .shadow = true, .shadow_color = shade, .font = f});
    });
    CHECK(items.size() == 2);
    if (items.size() != 2) return;
    CHECK(sprite_at(items[0], page, 0, 0, 7, 9, 2, 6, 14, 18) && same_color(items[0].color, shade));
    CHECK(sprite_at(items[1], page, 0, 0, 7, 9, 0, 4, 14, 18));
}

NNH_TEST(text_draw_font_outline_glyph_is_drawn_first_in_its_colour) {
    start_engine();
    FontBuilder b = kerned();
    b.glyphs[1].has_outline = true; // 'A'
    b.glyphs[1].outline = {40, 0, 9, 11, -1, 1};
    nnh::Font f = load("fonts/o", b);
    const auto page = last_alpha_handle();
    const nnh::Color edge = nnh::rgba(0, 0, 0, 255);
    const auto items = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {10, 10}, "AV", {.font = f, .outline_color = edge}); });
    CHECK(items.size() == 3); // V has no outline bitmap
    if (items.size() != 3) return;
    CHECK(sprite_at(items[0], page, 40, 0, 9, 11, 9, 11, 9, 11) && same_color(items[0].color, edge));
    CHECK(sprite_at(items[1], page, 0, 0, 7, 9, 10, 12, 7, 9));
    const auto plain = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {10, 10}, "AV", {.font = f}); });
    CHECK(plain.size() == 2); // no outline colour: no outline
}

NNH_TEST(text_draw_font_layer_camera_and_zoom) {
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const auto page = last_alpha_handle();
    const auto items = frame_items([&] {
        nnh::gfx::set_camera(nnh::Screen::Top, {.position = {5, 0}, .zoom = 2});
        nnh::gfx::draw_text(nnh::Screen::Top, {10, 0}, "A", {.layer = 3, .font = f});
        nnh::gfx::draw_text(nnh::Screen::Top, {10, 0}, "A", {.layer = 3, .ignore_camera = true, .font = f});
    });
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    CHECK(items.size() == 2);
    if (items.size() != 2) return;
    CHECK(items[0].layer == 3 && sprite_at(items[0], page, 0, 0, 7, 9, 10, 4, 14, 18)); // (10 − 5) × 2, 2 × 2
    CHECK(sprite_at(items[1], page, 0, 0, 7, 9, 10, 2, 7, 9));
}

NNH_TEST(text_draw_font_fallback_glyph_uses_its_own_font_and_metrics) {
    start_engine();
    FontBuilder cjk;
    cjk.glyph(0x4E00, 0, 0, 12, 12, 1, 0, 14);
    FontBuilder a = kerned();
    a.fallbacks = {"fonts/cjk"};
    add_font("fonts/cjk", cjk);
    nnh::Font f = load("fonts/a", a); // its page, then its fallback's
    const auto cjk_page = last_alpha_handle();
    const auto page = cjk_page - 1;
    const auto items = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, "A\xE4\xB8\x80V\nA", {.font = f}); });
    CHECK(items.size() == 4);
    if (items.size() != 4) return;
    CHECK(sprite_at(items[0], page, 0, 0, 7, 9, 0, 2, 7, 9));
    CHECK(sprite_at(items[1], cjk_page, 0, 0, 12, 12, 9, 0, 12, 12)); // pen 8 + 1, no kerning across fonts
    CHECK(sprite_at(items[2], page, 8, 0, 7, 9, 22, 2, 7, 9));        // 8 + 14
    CHECK(sprite_at(items[3], page, 0, 0, 7, 9, 0, 22, 7, 9));        // the first font's line height
}

NNH_TEST(text_draw_font_missing_characters_use_the_replacement_glyph) {
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const auto page = last_alpha_handle();
    LogLines warnings(nnh::LogLevel::Warn);
    const auto items = frame_items([&] {
        nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, "\xE4\xB8\x80" "A\xFF", {.font = f});
        nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, "\xE4\xB8\x80", {.font = f});
    });
    CHECK(items.size() == 4);
    if (items.size() != 4) return;
    CHECK(sprite_at(items[0], page, 16, 0, 5, 9, 1, 2, 5, 9));  // '?' for U+4E00
    CHECK(sprite_at(items[1], page, 0, 0, 7, 9, 6, 2, 7, 9));
    CHECK(sprite_at(items[2], page, 16, 0, 5, 9, 15, 2, 5, 9)); // invalid UTF-8 → the replacement too
    CHECK(warnings.count("U+4E00 not in fonts/k") == (nnh::build::kDevToolsEnabled ? 1 : 0)); // once per font
}

NNH_TEST(text_draw_font_without_a_replacement_draws_the_hollow_box) {
    start_engine();
    FontBuilder b = kerned();
    b.glyphs.erase(b.glyphs.begin() + 3); // no '?'
    nnh::Font f = load("fonts/n", b);
    const auto items = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {10, 10}, "\xE4\xB8\x80" "A", {.scale = 2, .font = f}); });
    CHECK(items.size() == 2);
    if (items.size() != 2) return;
    // size 16: advance max(4, 8) = 8; a 6 × 14 outline at pen + (1, 1), × 2
    CHECK(items[0].kind == Kind::RectOutline && items[0].x == 12 && items[0].y == 12 && items[0].w == 12 && items[0].h == 28);
    CHECK(items[1].x == 10 + 16 && items[1].kind == Kind::Sprite);
}

NNH_TEST(text_draw_font_zero_width_characters_draw_nothing) {
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const auto items = frame_items([&] {
        nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, "A\xCC\x81\xE2\x80\x8D\xEF\xB8\x8FV", {.font = f});
    });
    CHECK(items.size() == 2 && items.size() == 2 && items[1].x == 7); // A, then V kerned as if nothing was between
}

NNH_TEST(text_draw_unwrapped_text_equals_the_wrapper_path_with_a_huge_width) {
    // the no-wrap fast path (lines end only at '\n') draws exactly what the wrapper draws when nothing needs wrapping
    start_engine();
    nnh::Font f = load("fonts/k", kerned());
    const char* texts[] = {"AV A", "AV\nA j\n", "\nAVAV ? j", "A\xE2\x82\xAC" "V", ""};
    const nnh::TextAlign aligns[] = {nnh::TextAlign::Left, nnh::TextAlign::Center, nnh::TextAlign::Right};
    int mismatches = 0;
    for (const char* t : texts) {
        for (const nnh::TextAlign a : aligns) {
            for (int scale = 1; scale <= 2; ++scale) {
                nnh::TextOptions plain{.scale = scale, .align = a, .shadow = scale == 2, .font = f};
                nnh::TextOptions wide = plain;
                wide.wrap_width = 30000;
                const auto x = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {100, 20}, t, plain); });
                const auto y = frame_items([&] { nnh::gfx::draw_text(nnh::Screen::Top, {100, 20}, t, wide); });
                bool same = x.size() == y.size();
                for (std::size_t i = 0; same && i < x.size(); ++i)
                    same = x[i].kind == y[i].kind && x[i].x == y[i].x && x[i].y == y[i].y && x[i].w == y[i].w &&
                           x[i].h == y[i].h && x[i].region.x == y[i].region.x && same_color(x[i].color, y[i].color);
                mismatches += same ? 0 : 1;
            }
        }
    }
    CHECK(mismatches == 0);
}
