// Geometry properties over seeded random effects (spec 023 FR-4, FR-21; plan 023 T3.5): every rectangle inside its
// screen, no two overlapping (so the covered area is the sum of their areas), nothing at coverage 0, the whole screen
// at 1, the covered area never shrinking as coverage grows, counts within max_rects. 2000 effects × 10 coverages.

#include <cstdint>

#include "geometry_fixture.hpp"

namespace tr = nnh::transition;
using geo_test::draw;
using geo_test::Rects;
using nnh::Screen;

namespace {

struct Random {
    std::uint32_t s;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    int below(int n) { return static_cast<int>(next() % static_cast<std::uint32_t>(n)); }
};

tr::Effect random_effect(Random& r) {
    tr::Effect e;
    const tr::Kind kinds[] = {tr::Kind::Fade, tr::Kind::Wipe, tr::Kind::Split, tr::Kind::Blinds, tr::Kind::Iris};
    e.kind = kinds[r.below(5)];
    e.edge = static_cast<tr::Edge>(r.below(4));
    e.split = static_cast<tr::SplitAxis>(r.below(2));
    e.bars = 2 + r.below(31);
    e.shape = static_cast<tr::IrisShape>(r.below(2));
    e.band = 1 + r.below(16);
    if (r.below(4) != 0) {
        e.center = tr::IrisCenter{{static_cast<float>(r.below(1200) - 400), static_cast<float>(r.below(900) - 300)},
                                  r.below(2) == 0 ? Screen::Top : Screen::Bottom};
    }
    e.color = nnh::rgb(static_cast<std::uint8_t>(r.next()), 0, 0);
    return e;
}

} // namespace

NNH_TEST(random_effects_keep_their_geometry_promises) {
    Random r{20231124u};
    bool inside = true, empty_at_zero = true, full_at_one = true, monotonic = true, disjoint = true, counted = true;
    for (int i = 0; i < 2000; ++i) {
        const tr::Effect e = random_effect(r);
        const geo_test::Where where{r.below(2) == 0, r.below(3) == 0, r.below(241)};
        const Screen screen = r.below(2) == 0 ? Screen::Top : Screen::Bottom;
        const int max = tr::max_rects(e, where.tall);
        empty_at_zero = empty_at_zero && draw(e, screen, 0, where).n == 0;
        int last = -1;
        // Ten growing coverages, the last one full.
        int q = 0;
        for (int step = 0; step < 10; ++step) {
            q = step == 9 ? nnh::detail::kFullQ : q + 1 + r.below(7000);
            if (q > nnh::detail::kFullQ) q = nnh::detail::kFullQ;
            const Rects rects = draw(e, screen, q, where);
            counted = counted && rects.n <= max && rects.n <= tr::kMaxEffectRects;
            // Disjoint rectangles inside the screen: their areas add up to the covered area.
            int covered = 0;
            const int w = nnh::screen_width(screen);
            for (int a = 0; a < rects.n; ++a) {
                const geo_test::PixelRect& ra = rects.r[a];
                inside = inside && ra.w > 0 && ra.h > 0 && ra.x >= 0 && ra.y >= 0 && ra.x + ra.w <= w &&
                         ra.y + ra.h <= 240;
                covered += ra.w * ra.h;
                for (int b = a + 1; b < rects.n; ++b) {
                    const geo_test::PixelRect& rb = rects.r[b];
                    disjoint = disjoint && (ra.x + ra.w <= rb.x || rb.x + rb.w <= ra.x || ra.y + ra.h <= rb.y ||
                                            rb.y + rb.h <= ra.y);
                }
            }
            monotonic = monotonic && (e.kind == tr::Kind::Fade || covered >= last);
            last = covered;
        }
        full_at_one = full_at_one && last == nnh::screen_width(screen) * 240;
    }
    CHECK(inside);
    CHECK(empty_at_zero);
    CHECK(full_at_one);
    CHECK(monotonic);
    CHECK(disjoint);
    CHECK(counted);
}
