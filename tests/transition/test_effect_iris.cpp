// The iris: integer square root, radius, bands, circle and diamond, merged full rows (spec 023 FR-4, FR-21; plan
// 023 "Iris").

#include <cstdint>

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

std::uint32_t next(std::uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

bool exact_root(std::uint32_t v) {
    const std::uint64_t r = nnh::detail::isqrt(v);
    return r * r <= v && (r + 1) * (r + 1) > v;
}

} // namespace

NNH_TEST(isqrt_is_exact_for_every_value_below_2_pow_20) {
    bool ok = true;
    for (std::uint32_t v = 0; v < (1u << 20); ++v) ok = ok && exact_root(v);
    CHECK(ok);
}

NNH_TEST(isqrt_is_exact_for_large_values) {
    std::uint32_t s = 0x2545F491u;
    bool ok = exact_root(0xFFFFFFFFu) && exact_root(0x80000000u) && exact_root(65535u * 65535u);
    for (int i = 0; i < 200000; ++i) ok = ok && exact_root(next(s));
    CHECK(ok);
}

NNH_TEST(an_iris_draws_nothing_open_and_one_rectangle_closed) {
    const tr::IrisCenter centres[] = {{}, {{-30, 260}}, {{4096, -4096}}, {{0, 0}}, {{400, 240}}};
    for (const tr::IrisCenter& c : centres) {
        for (int band : {1, 4, 16}) {
            CHECK(draw(tr::iris(c, tr::IrisShape::Circle, nnh::colors::black, band), Screen::Top, 0).n == 0);
            const Rects r = draw(tr::iris(c, tr::IrisShape::Diamond, nnh::colors::black, band), Screen::Top, kFull);
            CHECK(r.n == 1 && is(r.r[0], 0, 0, 400, 240));
            const Rects b = draw(tr::iris(c, tr::IrisShape::Circle, nnh::colors::black, band), Screen::Bottom, kFull);
            CHECK(b.n == 1 && is(b.r[0], 0, 0, 320, 240));
        }
    }
}

NNH_TEST(a_half_closed_circle_has_the_documented_bands) {
    // Centre (200, 120): R0 = ⌈√(200² + 120²)⌉ = 234, r = 234 − ext(234) = 117.
    const Rects r = draw(tr::iris({{200, 120}}), Screen::Top, kHalf);
    CHECK(r.n == 118);
    CHECK(is(r.r[0], 0, 0, 400, 4)); // band 0: ym 2, dy 118 ≥ 117 → fully covered
    CHECK(is(r.r[1], 0, 4, 174, 4)); // band 1: ym 6, dy 114, w = ⌊√693⌋ = 26
    CHECK(is(r.r[2], 226, 4, 174, 4));
    CHECK(is(r.r[57], 0, 116, 84, 4)); // band 29: ym 118, dy 2, w = ⌊√13685⌋ = 116
    CHECK(is(r.r[58], 316, 116, 84, 4));
    CHECK(is(r.r[117], 0, 236, 400, 4));
}

NNH_TEST(a_half_closed_diamond_has_straight_edges) {
    const Rects r = draw(tr::iris({{200, 120}}, tr::IrisShape::Diamond), Screen::Top, kHalf);
    CHECK(r.n == 118);
    CHECK(is(r.r[1], 0, 4, 197, 4)); // w = r − dy = 3
    CHECK(is(r.r[2], 203, 4, 197, 4));
    CHECK(is(r.r[57], 0, 116, 85, 4)); // w = 115
}

NNH_TEST(the_iris_default_centre_is_the_middle_of_the_screen) {
    const Rects a = draw(tr::iris(), Screen::Bottom, kHalf);
    const Rects b = draw(tr::iris({{160, 120}}), Screen::Bottom, kHalf);
    CHECK(a.n == b.n && a.n > 0);
    bool same = true;
    for (int i = 0; i < a.n; ++i) same = same && is(a.r[i], b.r[i].x, b.r[i].y, b.r[i].w, b.r[i].h);
    CHECK(same);
}

NNH_TEST(fully_covered_rows_merge_above_and_below_the_shape) {
    // A small circle near the top: the rows below it are one rectangle.
    const Rects r = draw(tr::iris({{200, 20}}, tr::IrisShape::Circle, nnh::colors::black, 1), Screen::Top, 60000);
    CHECK(r.n > 2);
    const geo_test::PixelRect last = r.r[r.n - 1];
    CHECK(last.x == 0 && last.w == 400 && last.y + last.h == 240 && last.h > 100);
}

NNH_TEST(iris_counts_stay_within_max_rects) {
    const tr::IrisCenter centres[] = {{{200, 120}}, {{-30, 260}}, {{0, 0}}, {{399, 239}}, {{4096, 4096}},
                                      {{-4096, 100}}, {{200, 300}}};
    bool ok = true;
    for (const tr::IrisCenter& c : centres) {
        for (int band : {1, 2, 3, 4, 5, 7, 16}) {
            for (int q = 0; q <= kFull; q += 997) {
                for (Screen s : {Screen::Top, Screen::Bottom}) {
                    const tr::Effect e = tr::iris(c, tr::IrisShape::Circle, nnh::colors::black, band);
                    ok = ok && draw(e, s, q).n <= tr::max_rects(e);
                    const tr::Effect d = tr::iris(c, tr::IrisShape::Diamond, nnh::colors::black, band);
                    ok = ok && draw(d, s, q).n <= tr::max_rects(d);
                }
            }
        }
    }
    CHECK(ok);
    CHECK(tr::max_rects(tr::iris()) == 122);
    CHECK(tr::max_rects(tr::iris({}, tr::IrisShape::Circle, nnh::colors::black, 1)) == tr::kMaxEffectRects);
    CHECK(tr::max_rects(tr::iris({}, tr::IrisShape::Circle, nnh::colors::black, 16)) == 32);
}

NNH_TEST(an_iris_outside_the_screen_still_closes_monotonically) {
    for (int band : {1, 4, 16}) {
        int last = -1;
        bool monotonic = true;
        for (int q = 0; q <= kFull; q += 2048) {
            Raster raster(Screen::Top);
            raster.add(draw(tr::iris({{-30, 260}}, tr::IrisShape::Circle, nnh::colors::black, band), Screen::Top, q));
            const int covered = raster.covered();
            monotonic = monotonic && covered >= last && !raster.overlaps() && !raster.outside;
            last = covered;
        }
        CHECK(monotonic && last == 400 * 240);
    }
}

NNH_TEST(the_iris_centre_pixel_stays_open_until_closed) {
    Raster raster(Screen::Top);
    raster.add(draw(tr::iris({{100.4f, 50.6f}}, tr::IrisShape::Circle, nnh::colors::black, 1), Screen::Top, 65000));
    CHECK(!raster.at(100, 51)); // the centre, rounded half up
    CHECK(raster.at(0, 0) && raster.at(399, 239));
}
