// Small 2D value types (nnh/math.hpp).

#include "nnh/math.hpp"
#include "test.hpp"

using nnh::Region;
using nnh::Vec2;

NNH_TEST(vec2_adds_and_subtracts) {
    constexpr Vec2 a{1, 2};
    constexpr Vec2 b{3, -5};
    static_assert(a + b == Vec2{4, -3});
    static_assert(a - b == Vec2{-2, 7});
    static_assert(-a == Vec2{-1, -2});
    CHECK((a + b) == (Vec2{4, -3}));
}

NNH_TEST(vec2_scales_by_a_number) {
    constexpr Vec2 a{2, -4};
    static_assert(a * 0.5f == Vec2{1, -2});
    static_assert(0.5f * a == Vec2{1, -2});
    static_assert(a / 2.0f == Vec2{1, -2});
    CHECK((a * 3.0f) == (Vec2{6, -12}));
}

NNH_TEST(vec2_compound_assignment_changes_the_left_side) {
    Vec2 p{1, 1};
    p += Vec2{2, 3};
    CHECK(p == (Vec2{3, 4}));
    p -= Vec2{1, 1};
    CHECK(p == (Vec2{2, 3}));
    p *= 2.0f;
    CHECK(p == (Vec2{4, 6}));
    CHECK(p != (Vec2{4, 7}));
}

NNH_TEST(region_is_empty_without_width_or_height) {
    static_assert(Region{}.empty());
    static_assert(Region{0, 0, 16, 0}.empty());
    static_assert(!Region{16, 0, 16, 16}.empty());
    CHECK((Region{0, 0, -1, 4}.empty()));
}
