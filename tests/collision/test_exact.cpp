// Exact predicates of the queries (spec 019 FR-8, FR-16, D34).

#include <cstdint>
#include <cstring>

#include "collision/exact.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::detail::compare;
using collision::detail::Ratio;

namespace {

std::uint32_t bits(float v) {
    std::uint32_t b;
    std::memcpy(&b, &v, sizeof b);
    return b;
}

} // namespace

NNH_TEST(equal_ratios_compare_equal_without_dividing) {
    CHECK(compare({1, 3}, {2, 6}) == 0);
    CHECK(compare({0, 5}, collision::detail::kRatioZero) == 0);
    CHECK(compare({7, 7}, collision::detail::kRatioOne) == 0);
    CHECK(compare({1, 3}, {1, 2}) == -1 && compare({1, 2}, {1, 3}) == 1);
}

NNH_TEST(ratios_that_round_to_one_float_still_compare_apart) {
    const Ratio third{1, 3};
    const Ratio above{16777217, 50331648}; // 1/3 + 1/(3·2^24)
    CHECK(collision::detail::to_fraction(third) == collision::detail::to_fraction(above));
    CHECK(compare(third, above) == -1 && compare(above, third) == 1);
}

NNH_TEST(lattice_ratios_at_the_bound_are_exact) {
    // Differences of 1/256-px coordinates within ±2^17 px: the smallest gap between two ratios still shows.
    const double x = 262144.0; // 2^18
    const double e = 1.0 / 256;
    CHECK(compare({x - e, x}, {x - 2 * e, x - e}) == 1);
    CHECK(compare({x - 2 * e, x - e}, {x - e, x}) == -1);
}

NNH_TEST(fractions_follow_the_order_of_the_ratios) {
    std::uint32_t seed = 99;
    Ratio previous{0, 1};
    for (int i = 0; i < 2000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const Ratio r{static_cast<double>(seed >> 12), 1048576.0};
        if (compare(previous, r) < 0) CHECK(collision::detail::to_fraction(previous) <= collision::detail::to_fraction(r));
        if (compare(previous, r) > 0) CHECK(collision::detail::to_fraction(previous) >= collision::detail::to_fraction(r));
        previous = r;
    }
}

NNH_TEST(a_segment_keeps_exact_differences_and_signs) {
    const collision::detail::Segment s = collision::detail::make_segment({1.5f, 100000.0f}, {-2.25f, 100000.0f});
    CHECK(s.dx == -3.75 && s.dy == 0 && s.ax == 3.75 && s.ay == 0 && s.sx == -1 && s.sy == 0);
    CHECK(!collision::detail::zero_length(s));
    CHECK(collision::detail::zero_length(collision::detail::make_segment({3, 4}, {3, 4})));
    // 2^24 + 1 isn't a float, but the difference of two floats around it is exact in double.
    const collision::detail::Segment far = collision::detail::make_segment({16777216.0f, 0}, {0.25f, 0});
    CHECK(far.dx == 0.25 - 16777216.0);
}

NNH_TEST(along_rounds_once) {
    CHECK(collision::detail::along(10, 30, {1, 3}) == 20.0f);
    CHECK(collision::detail::along(-4, 8, {3, 4}) == 2.0f);
    CHECK(collision::detail::clamp_to(5, 6, 9) == 6 && collision::detail::clamp_to(10, 6, 9) == 9);
}

NNH_TEST(steps_move_one_ulp_across_powers_of_two_and_zero) {
    using collision::detail::step_down;
    using collision::detail::step_up;
    CHECK(bits(step_up(1.0f)) == bits(1.0f) + 1);
    CHECK(step_down(1.0f) == 1.0f - 1.0f / 16777216); // below a power of two the spacing halves
    CHECK(step_up(step_down(1.0f)) == 1.0f);
    CHECK(step_up(-1.0f) == -(1.0f - 1.0f / 16777216));
    CHECK(bits(step_up(0.0f)) == 1u && bits(step_up(-0.0f)) == 1u);
    CHECK(bits(step_down(0.0f)) == 0x80000001u);
    CHECK(step_up(step_down(0.0f)) == 0.0f);
    CHECK(step_up(__builtin_inff()) == __builtin_inff());
    const float nan = __builtin_nanf("");
    CHECK(step_up(nan) != step_up(nan));
}
