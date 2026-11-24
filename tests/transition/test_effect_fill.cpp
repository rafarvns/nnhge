// Fade, wipe, split and blinds: exact rectangles at fixed coverages (spec 023 FR-4, FR-5; plan 023 "Geometry").

#include "geometry_fixture.hpp"

namespace tr = nnh::transition;
using geo_test::draw;
using geo_test::is;
using geo_test::Rects;
using nnh::Screen;

namespace {
constexpr int kHalf = 32768;
constexpr int kFull = 65536;
} // namespace

NNH_TEST(coverage_q_rounds_half_up_and_clamps) {
    CHECK(nnh::detail::coverage_q(0.0f) == 0);
    CHECK(nnh::detail::coverage_q(0.25f) == 16384);
    CHECK(nnh::detail::coverage_q(0.5f) == kHalf);
    CHECK(nnh::detail::coverage_q(1.0f) == kFull);
    CHECK(nnh::detail::coverage_q(2.0f) == kFull);
    CHECK(nnh::detail::coverage_q(-1.0f) == 0);
    CHECK(nnh::detail::coverage_q(0.0f / 0.0f) == 0);
    CHECK(nnh::detail::coverage_q(1.0f / 65536.0f) == 1);
    CHECK(nnh::detail::coverage_q(0.5f / 65536.0f) == 1);   // ½ rounds up
    CHECK(nnh::detail::coverage_q(0.49f / 65536.0f) == 0);
}

NNH_TEST(ext_is_the_rounded_share_of_a_length) {
    CHECK(nnh::detail::ext(0, 400) == 0);
    CHECK(nnh::detail::ext(kFull, 400) == 400);
    CHECK(nnh::detail::ext(kHalf, 400) == 200);
    CHECK(nnh::detail::ext(16548, 400) == 101);
    CHECK(nnh::detail::ext(65535, 400) == 400);
    CHECK(nnh::detail::ext(1, 400) == 0);
}

NNH_TEST(nothing_is_drawn_at_coverage_zero) {
    const tr::Effect effects[] = {tr::fade(), tr::wipe(tr::Edge::Left), tr::split(), tr::blinds()};
    for (const tr::Effect& e : effects) {
        CHECK(draw(e, Screen::Top, 0).n == 0);
        CHECK(draw(e, Screen::Bottom, 0).n == 0);
    }
}

NNH_TEST(a_fade_covers_the_screen_with_its_alpha_rounded) {
    const Rects r = draw(tr::fade(nnh::colors::white), Screen::Bottom, kHalf);
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 320, 240));
    const nnh::detail::ResolvedEffect fx =
        nnh::detail::resolve_effect(tr::fade(nnh::colors::white), Screen::Top, {}, false);
    CHECK(nnh::detail::effect_color_q(fx, 0).a == 0);
    CHECK(nnh::detail::effect_color_q(fx, 128).a == 0);
    CHECK(nnh::detail::effect_color_q(fx, 129).a == 1);
    CHECK(nnh::detail::effect_color_q(fx, kHalf).a == 128);
    CHECK(nnh::detail::effect_color_q(fx, kFull).a == 255);
    CHECK(nnh::detail::effect_color_q(fx, kHalf).r == 255);
    CHECK(draw(tr::fade(), Screen::Top, 128).n == 0); // alpha 0: nothing drawn
}

NNH_TEST(covers_are_opaque_whatever_the_colour_alpha) {
    const nnh::detail::ResolvedEffect fx =
        nnh::detail::resolve_effect(tr::wipe(tr::Edge::Left, nnh::rgba(10, 20, 30, 40)), Screen::Top, {}, false);
    const nnh::Color c = nnh::detail::effect_color_q(fx, kHalf);
    CHECK(c.r == 10 && c.g == 20 && c.b == 30 && c.a == 255);
}

NNH_TEST(wipes_grow_from_each_edge) {
    Rects r = draw(tr::wipe(tr::Edge::Left), Screen::Top, kHalf);
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 200, 240));
    r = draw(tr::wipe(tr::Edge::Left), Screen::Top, 16384);
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 100, 240));
    r = draw(tr::wipe(tr::Edge::Right), Screen::Bottom, kHalf);
    CHECK(r.n == 1 && is(r.r[0], 160, 0, 160, 240));
    r = draw(tr::wipe(tr::Edge::Top), Screen::Top, 49152);
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 400, 180));
    r = draw(tr::wipe(tr::Edge::Bottom), Screen::Top, 49152);
    CHECK(r.n == 1 && is(r.r[0], 0, 60, 400, 180));
    r = draw(tr::wipe(tr::Edge::Left), Screen::Top, 1); // ext rounds to 0
    CHECK(r.n == 0);
    r = draw(tr::wipe(tr::Edge::Left), Screen::Top, 65535);
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 400, 240));
    r = draw(tr::wipe(tr::Edge::Bottom), Screen::Bottom, kFull);
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 320, 240));
}

NNH_TEST(a_continued_wipe_leaves_by_the_opposite_edge) {
    Rects r = draw(tr::wipe(tr::Edge::Left), Screen::Top, kHalf, {.continued = true});
    CHECK(r.n == 1 && is(r.r[0], 200, 0, 200, 240));
    r = draw(tr::wipe(tr::Edge::Top), Screen::Top, 16384, {.continued = true});
    CHECK(r.n == 1 && is(r.r[0], 0, 180, 400, 60));
    r = draw(tr::wipe(tr::Edge::Bottom), Screen::Bottom, 16384, {.continued = true});
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 320, 60));
    r = draw(tr::wipe(tr::Edge::Right), Screen::Bottom, 16384, {.continued = true});
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 80, 240));
}

NNH_TEST(a_split_closes_with_the_odd_pixel_on_the_left) {
    Rects r = draw(tr::split(), Screen::Top, kHalf);
    CHECK(r.n == 2 && is(r.r[0], 0, 0, 100, 240) && is(r.r[1], 300, 0, 100, 240));
    r = draw(tr::split(), Screen::Top, 16548); // e = 101
    CHECK(r.n == 2 && is(r.r[0], 0, 0, 51, 240) && is(r.r[1], 350, 0, 50, 240));
    r = draw(tr::split(tr::SplitAxis::TopBottom), Screen::Top, kHalf);
    CHECK(r.n == 2 && is(r.r[0], 0, 0, 400, 60) && is(r.r[1], 0, 180, 400, 60));
    r = draw(tr::split(), Screen::Bottom, kFull);
    CHECK(r.n == 2 && is(r.r[0], 0, 0, 160, 240) && is(r.r[1], 160, 0, 160, 240));
    r = draw(tr::split(), Screen::Top, 82); // e = 1: only the left piece
    CHECK(r.n == 1 && is(r.r[0], 0, 0, 1, 240));
}

NNH_TEST(a_continued_split_is_one_centred_band) {
    Rects r = draw(tr::split(), Screen::Top, kHalf, {.continued = true});
    CHECK(r.n == 1 && is(r.r[0], 100, 0, 200, 240));
    r = draw(tr::split(tr::SplitAxis::TopBottom), Screen::Bottom, 16384, {.continued = true});
    CHECK(r.n == 1 && is(r.r[0], 0, 90, 320, 60));
    r = draw(tr::split(), Screen::Top, 16548, {.continued = true}); // e = 101: (400 − 101) >> 1 = 149
    CHECK(r.n == 1 && is(r.r[0], 149, 0, 101, 240));
}

NNH_TEST(blinds_bars_split_the_screen_at_floor_i_l_over_n) {
    const int counts[] = {2, 8, 32, 7};
    for (int n : counts) {
        const Rects r = draw(tr::blinds(tr::Edge::Top, n), Screen::Top, kFull);
        CHECK(r.n == n);
        for (int i = 0; i < n && i < r.n; ++i) {
            const int b0 = i * 240 / n;
            const int b1 = (i + 1) * 240 / n;
            CHECK(is(r.r[i], 0, b0, 400, b1 - b0));
        }
        const Rects v = draw(tr::blinds(tr::Edge::Left, n), Screen::Bottom, kFull);
        CHECK(v.n == n);
        for (int i = 0; i < n && i < v.n; ++i) {
            const int b0 = i * 320 / n;
            const int b1 = (i + 1) * 320 / n;
            CHECK(is(v.r[i], b0, 0, b1 - b0, 240));
        }
    }
}

NNH_TEST(each_blind_grows_from_its_own_edge) {
    Rects r = draw(tr::blinds(tr::Edge::Top, 8), Screen::Top, kHalf); // bars of 30: 15 each
    CHECK(r.n == 8);
    for (int i = 0; i < 8 && i < r.n; ++i) CHECK(is(r.r[i], 0, 30 * i, 400, 15));
    r = draw(tr::blinds(tr::Edge::Bottom, 8), Screen::Top, kHalf);
    for (int i = 0; i < 8 && i < r.n; ++i) CHECK(is(r.r[i], 0, 30 * i + 15, 400, 15));
    r = draw(tr::blinds(tr::Edge::Left, 2), Screen::Top, kHalf);
    CHECK(r.n == 2 && is(r.r[0], 0, 0, 100, 240) && is(r.r[1], 200, 0, 100, 240));
    r = draw(tr::blinds(tr::Edge::Right, 2), Screen::Top, kHalf);
    CHECK(r.n == 2 && is(r.r[0], 100, 0, 100, 240) && is(r.r[1], 300, 0, 100, 240));
    r = draw(tr::blinds(tr::Edge::Top, 32), Screen::Top, kHalf); // bars of 7 and 8 rows: 4 rows each
    CHECK(r.n == 32 && is(r.r[0], 0, 0, 400, 4) && is(r.r[1], 0, 7, 400, 4));
}

NNH_TEST(continued_blinds_grow_from_the_opposite_edge) {
    Rects r = draw(tr::blinds(tr::Edge::Top, 8), Screen::Top, kHalf, {.continued = true});
    CHECK(r.n == 8);
    for (int i = 0; i < 8 && i < r.n; ++i) CHECK(is(r.r[i], 0, 30 * i + 15, 400, 15));
    r = draw(tr::blinds(tr::Edge::Left, 2), Screen::Bottom, 16384, {.continued = true}); // bars of 160: 40
    CHECK(r.n == 2 && is(r.r[0], 120, 0, 40, 240) && is(r.r[1], 280, 0, 40, 240));
}

NNH_TEST(bar_and_band_settings_are_clamped) {
    CHECK(draw(tr::blinds(tr::Edge::Top, 1), Screen::Top, kFull).n == 2);
    CHECK(draw(tr::blinds(tr::Edge::Top, 99), Screen::Top, kFull).n == 32);
    CHECK(draw(tr::blinds(tr::Edge::Top, -5), Screen::Top, kFull).n == 2);
}
