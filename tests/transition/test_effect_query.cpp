// The public effect geometry (spec 023 FR-9, FR-18): effect_rects, effect_color, max_rects and draw_effect work with
// no transition and no scene stack, and draw_effect records exactly effect_rects' rectangles.

#include "nnh/app.hpp"
#include "nnh/platform/video.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/transition.hpp"
#include "test.hpp"

namespace tr = nnh::transition;
namespace fake = nnh::platform_fake;
using nnh::Rect;
using nnh::Screen;

namespace {

bool same(const Rect& a, const Rect& b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

} // namespace

NNH_TEST(effect_rects_needs_no_init_and_no_scene) {
    nnh::shutdown();
    Rect out[4];
    CHECK(tr::effect_rects(Screen::Top, tr::wipe(tr::Edge::Left), 0.5f, out, 4) == 1);
    CHECK(same(out[0], {0, 0, 200, 240}));
    CHECK(tr::effect_rects(Screen::Bottom, tr::split(), 1.0f, out, 4) == 2);
    CHECK(same(out[0], {0, 0, 160, 240}) && same(out[1], {160, 0, 160, 240}));
    CHECK(tr::effect_rects(Screen::Top, tr::fade(), 0.0f, out, 4) == 0);
}

NNH_TEST(effect_rects_counts_past_its_capacity) {
    Rect out[3];
    out[2] = Rect{-1, -1, -1, -1};
    CHECK(tr::effect_rects(Screen::Top, tr::blinds(tr::Edge::Top, 8), 1.0f, out, 2) == 8);
    CHECK(same(out[0], {0, 0, 400, 30}) && same(out[1], {0, 30, 400, 30}));
    CHECK(same(out[2], {-1, -1, -1, -1})); // nothing written past the capacity
    CHECK(tr::effect_rects(Screen::Top, tr::blinds(tr::Edge::Top, 8), 1.0f, nullptr, 0) == 8);
    CHECK(tr::effect_rects(Screen::Top, tr::blinds(tr::Edge::Top, 8), 1.0f, nullptr, 5) == 8);
    CHECK(tr::effect_rects(Screen::Top, tr::blinds(tr::Edge::Top, 8), 1.0f, out, -3) == 8);
}

NNH_TEST(effect_rects_takes_the_placement) {
    Rect out[2];
    CHECK(tr::effect_rects(Screen::Top, tr::wipe(tr::Edge::Left), 0.5f, out, 2, {.continued = true}) == 1);
    CHECK(same(out[0], {200, 0, 200, 240}));
    CHECK(tr::effect_rects(Screen::Bottom, tr::wipe(tr::Edge::Top), 0.75f, out, 2, {.tall = true}) == 1);
    CHECK(same(out[0], {0, 0, 320, 120}));
    // The gap is clamped to 240: e = 540 of 720 rows, the bottom screen starts at row 480.
    CHECK(tr::effect_rects(Screen::Bottom, tr::wipe(tr::Edge::Top), 0.75f, out, 2, {.tall = true, .tall_gap = 999}) ==
          1);
    CHECK(same(out[0], {0, 0, 320, 60}));
}

NNH_TEST(same_none_and_custom_draw_nothing) {
    Rect out[1];
    CHECK(tr::effect_rects(Screen::Top, tr::same(), 1.0f, out, 1) == 0);
    CHECK(tr::effect_rects(Screen::Top, tr::none(), 1.0f, out, 1) == 0);
    CHECK(tr::effect_rects(Screen::Top, tr::custom(), 1.0f, out, 1) == 0);
    CHECK(tr::max_rects(tr::none()) == 0 && tr::max_rects(tr::custom()) == 0);
}

NNH_TEST(effect_color_is_opaque_but_for_the_fade) {
    const nnh::Color c = tr::effect_color(tr::fade(nnh::rgb(10, 20, 30)), 0.5f);
    CHECK(c.r == 10 && c.g == 20 && c.b == 30 && c.a == 128);
    CHECK(tr::effect_color(tr::fade(), 1.0f).a == 255);
    CHECK(tr::effect_color(tr::fade(), 0.0f / 0.0f).a == 0);
    CHECK(tr::effect_color(tr::iris({}, tr::IrisShape::Circle, nnh::rgba(1, 2, 3, 4)), 0.3f).a == 255);
}

NNH_TEST(max_rects_follows_each_effects_settings) {
    CHECK(tr::max_rects(tr::fade()) == 1);
    CHECK(tr::max_rects(tr::wipe(tr::Edge::Bottom)) == 1);
    CHECK(tr::max_rects(tr::split()) == 2);
    CHECK(tr::max_rects(tr::blinds(tr::Edge::Top, 8)) == 8);
    CHECK(tr::max_rects(tr::blinds(tr::Edge::Top, 40)) == 32);
    CHECK(tr::max_rects(tr::iris()) == 122);
    CHECK(tr::max_rects(tr::iris({}, tr::IrisShape::Diamond, nnh::colors::black, 16)) == 32);
    CHECK(tr::max_rects(tr::iris({}, tr::IrisShape::Diamond, nnh::colors::black, 1)) == 482);
}

NNH_TEST(draw_effect_records_what_effect_rects_returns) {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
    CHECK(nnh::frame());
    nnh::gfx::set_camera(Screen::Top, {{37, -12}, 2.0f}); // ignored: overlays are in screen pixels
    const tr::Effect e = tr::iris({{120, 90}}, tr::IrisShape::Circle, nnh::rgb(200, 10, 10), 16);
    tr::draw_effect(Screen::Top, e, 0.6f, {.layer = 7});
    CHECK(nnh::frame());
    Rect expected[64];
    const int n = tr::effect_rects(Screen::Top, e, 0.6f, expected, 64);
    const fake::State& s = fake::state();
    CHECK(n > 4 && s.last_frame_item_count == n);
    bool ok = true;
    for (int i = 0; i < n && i < s.last_frame_item_count; ++i) {
        const nnh::platform::DrawItem2D& it = s.last_frame_items[i];
        ok = ok && it.kind == nnh::platform::DrawItem2D::Kind::FillRect && it.layer == 7 && it.x == expected[i].x &&
             it.y == expected[i].y && it.w == expected[i].w && it.h == expected[i].h && it.color.r == 200 &&
             it.color.a == 255;
    }
    CHECK(ok);
    nnh::shutdown();
}

NNH_TEST(draw_effect_outside_a_frame_does_nothing) {
    nnh::shutdown();
    tr::draw_effect(Screen::Top, tr::fade(), 1.0f); // no init: nothing, no crash
    CHECK(true);
}
