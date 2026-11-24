// Screens and the tall area; resolving each screen's effect per phase (spec 023 FR-5, FR-6; plan 023 "Resolving
// screens", "Tall area → screens").

#include "geometry_fixture.hpp"

namespace tr = nnh::transition;
using geo_test::draw;
using geo_test::is;
using geo_test::Raster;
using geo_test::Rects;
using nnh::Screen;

namespace {
constexpr int kHalf = 32768;
constexpr int kFull = 65536;

bool same_effect(const tr::Effect& a, const tr::Effect& b) {
    return a.kind == b.kind && a.edge == b.edge && a.color.r == b.color.r && a.color.g == b.color.g &&
           a.center.given == b.center.given && a.center.point == b.center.point;
}
} // namespace

NNH_TEST(a_tall_wipe_runs_down_the_top_screen_then_the_bottom_one) {
    const tr::Effect e = tr::wipe(tr::Edge::Top);
    Rects t = draw(e, Screen::Top, kHalf, {.tall = true}); // e = 240 of 480
    Rects b = draw(e, Screen::Bottom, kHalf, {.tall = true});
    CHECK(t.n == 1 && is(t.r[0], 0, 0, 400, 240));
    CHECK(b.n == 0);
    t = draw(e, Screen::Top, 49152, {.tall = true}); // e = 360
    b = draw(e, Screen::Bottom, 49152, {.tall = true});
    CHECK(t.n == 1 && is(t.r[0], 0, 0, 400, 240));
    CHECK(b.n == 1 && is(b.r[0], 0, 0, 320, 120));
    b = draw(e, Screen::Bottom, kFull, {.tall = true});
    CHECK(b.n == 1 && is(b.r[0], 0, 0, 320, 240));
}

NNH_TEST(the_gap_between_the_screens_is_drawn_nowhere) {
    const tr::Effect e = tr::wipe(tr::Edge::Top);
    // Gap 24: the area is 504 rows; at 49152, e = 378: the bottom screen's rows 264..378 → 0..114.
    Rects b = draw(e, Screen::Bottom, 49152, {.tall = true, .gap = 24});
    CHECK(b.n == 1 && is(b.r[0], 0, 0, 320, 114));
    // e = 252: only the gap's rows past the top screen: nothing on the bottom one.
    b = draw(e, Screen::Bottom, 32768, {.tall = true, .gap = 24});
    CHECK(b.n == 0);
    const Rects t = draw(e, Screen::Top, kFull, {.tall = true, .gap = 24});
    b = draw(e, Screen::Bottom, kFull, {.tall = true, .gap = 24});
    CHECK(t.n == 1 && is(t.r[0], 0, 0, 400, 240) && b.n == 1 && is(b.r[0], 0, 0, 320, 240));
}

NNH_TEST(a_tall_left_wipe_skips_the_bottom_screens_inset) {
    const tr::Effect e = tr::wipe(tr::Edge::Left);
    const Rects t = draw(e, Screen::Top, 16384, {.tall = true}); // e = 100 of 400
    const Rects b = draw(e, Screen::Bottom, 16384, {.tall = true}); // area x 40..100 → 0..60
    CHECK(t.n == 1 && is(t.r[0], 0, 0, 100, 240));
    CHECK(b.n == 1 && is(b.r[0], 0, 0, 60, 240));
}

NNH_TEST(a_tall_iris_on_a_bottom_point_closes_over_both_screens) {
    const tr::Effect e = tr::iris({{160, 120}, Screen::Bottom});
    for (int gap : {0, 24}) {
        int last_top = -1, last_bottom = -1;
        bool ok = true;
        for (int q = 0; q <= kFull; q += 4096) {
            Raster top(Screen::Top), bottom(Screen::Bottom);
            const Rects t = draw(e, Screen::Top, q, {.tall = true, .gap = gap});
            const Rects b = draw(e, Screen::Bottom, q, {.tall = true, .gap = gap});
            top.add(t);
            bottom.add(b);
            ok = ok && top.covered() >= last_top && bottom.covered() >= last_bottom && !top.outside &&
                 !bottom.outside && !top.overlaps() && !bottom.overlaps();
            ok = ok && t.n <= tr::max_rects(e, true) && b.n <= tr::max_rects(e, true);
            last_top = top.covered();
            last_bottom = bottom.covered();
        }
        CHECK(ok && last_top == 400 * 240 && last_bottom == 320 * 240);
        // Half closed: the circle's centre (the bottom screen's middle) is open, the top screen's corner covered.
        Raster bottom(Screen::Bottom), top(Screen::Top);
        bottom.add(draw(e, Screen::Bottom, kHalf, {.tall = true, .gap = gap}));
        top.add(draw(e, Screen::Top, kHalf, {.tall = true, .gap = gap}));
        CHECK(!bottom.at(160, 120) && top.at(0, 0));
    }
}

NNH_TEST(a_tall_fade_stays_per_screen) {
    const Rects t = draw(tr::fade(), Screen::Top, kHalf, {.tall = true, .gap = 24});
    const Rects b = draw(tr::fade(), Screen::Bottom, kHalf, {.tall = true, .gap = 24});
    CHECK(t.n == 1 && is(t.r[0], 0, 0, 400, 240) && b.n == 1 && is(b.r[0], 0, 0, 320, 240));
}

NNH_TEST(tall_bottom_bands_may_straddle_one_more_band) {
    CHECK(tr::max_rects(tr::iris(), true) == 124);
    CHECK(tr::max_rects(tr::iris({}, tr::IrisShape::Circle, nnh::colors::black, 1), true) == tr::kMaxEffectRects);
    bool ok = true;
    for (int gap = 0; gap <= 240; gap += 7) {
        for (int q = 1; q < kFull; q += 1500) {
            const tr::Effect e = tr::iris({{200, 30}}, tr::IrisShape::Diamond, nnh::colors::black, 4);
            ok = ok && draw(e, Screen::Bottom, q, {.tall = true, .gap = gap}).n <= tr::max_rects(e, true);
        }
    }
    CHECK(ok);
}

NNH_TEST(screens_resolve_same_and_none_per_phase) {
    const tr::Effect w = tr::wipe(tr::Edge::Left);
    const tr::Effect f = tr::fade(nnh::colors::white);
    const tr::Effect i = tr::iris({{10, 10}});
    nnh::detail::ScreenEffects s = nnh::detail::resolve_screens({.effect = w});
    CHECK(same_effect(s.top_out, w) && same_effect(s.top_in, w) && same_effect(s.bottom_out, w) &&
          same_effect(s.bottom_in, w));
    s = nnh::detail::resolve_screens({.effect = w, .reveal = f});
    CHECK(same_effect(s.top_in, f) && same_effect(s.bottom_out, w) && same_effect(s.bottom_in, f));
    s = nnh::detail::resolve_screens({.effect = w, .reveal = f, .bottom = tr::none()});
    CHECK(s.bottom_out.kind == tr::Kind::None && s.bottom_in.kind == tr::Kind::None);
    s = nnh::detail::resolve_screens({.effect = w, .reveal = f, .bottom = i});
    CHECK(same_effect(s.bottom_out, i) && same_effect(s.bottom_in, i));
    s = nnh::detail::resolve_screens({.effect = w, .bottom = i, .bottom_reveal = f});
    CHECK(same_effect(s.top_in, w) && same_effect(s.bottom_out, i) && same_effect(s.bottom_in, f));
    s = nnh::detail::resolve_screens({.effect = w, .bottom_reveal = tr::none()});
    CHECK(same_effect(s.bottom_out, w) && s.bottom_in.kind == tr::Kind::None);
}

NNH_TEST(tall_transitions_use_the_top_effects_on_both_screens) {
    const nnh::detail::ScreenEffects s = nnh::detail::resolve_screens(
        {.effect = tr::wipe(tr::Edge::Top), .reveal = tr::fade(), .bottom = tr::none(), .tall = true});
    CHECK(s.bottom_out.kind == tr::Kind::Wipe && s.bottom_in.kind == tr::Kind::Fade);
}
