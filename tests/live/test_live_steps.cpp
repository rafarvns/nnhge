// Ranges and stepping (spec 030 T1.2, FR-8, FR-9, FR-13, Q9): grids, steps by 1 and 10, the max stop, drag rounding,
// bar fill, colour channels, enums, refusals. Expected grid points are written as the plan's formula on literals
// (min + float(k) * step, folded by the compiler) or as hand-computed values.

#include <cmath>
#include <cstdint>
#include <limits>

#include "live/live_steps.hpp"
#include "test.hpp"

using namespace nnh::detail::live;

namespace {

FloatGrid fgrid(float min, float max, float step) {
    FloatGrid g;
    CHECK(make_float_grid(min, max, step, g) == RangeError::None);
    return g;
}
IntGrid igrid(std::int32_t min, std::int32_t max, std::int32_t step) {
    IntGrid g;
    CHECK(make_int_grid(min, max, step, g) == RangeError::None);
    return g;
}
bool same_bits(float a, float b) { return float_bits(a) == float_bits(b); }

} // namespace

NNH_TEST(live_steps_quarter_grid_has_forty_steps) {
    const FloatGrid g = fgrid(0, 10, 0.25f);
    CHECK(g.n == 40);
    CHECK(grid_value(g, 18) == 4.5f);
    CHECK(grid_value(g, 40) == 10.0f);
}

NNH_TEST(live_steps_quarter_grid_steps_by_one_and_ten) {
    const FloatGrid g = fgrid(0, 10, 0.25f);
    CHECK(step_value(g, 4.5f, 1) == 4.75f);
    CHECK(step_value(g, 4.5f, -1) == 4.25f);
    CHECK(step_value(g, 4.5f, 10) == 7.0f);
    CHECK(step_value(g, 4.5f, -10) == 2.0f);
}

NNH_TEST(live_steps_stop_at_both_ends) {
    const FloatGrid g = fgrid(0, 10, 0.25f);
    CHECK(step_value(g, 10.0f, 1) == 10.0f);
    CHECK(step_value(g, 0.0f, -1) == 0.0f);
    CHECK(step_value(g, 9.5f, 10) == 10.0f);
    CHECK(step_value(g, 0.5f, -10) == 0.0f);
    CHECK(step_value(g, 7.75f, 10) == 10.0f); // k' = 41 + ... past n
    CHECK(step_value(g, 7.5f, 10) == 10.0f);  // first above is 31 (7.75), +9 = 40: exactly max
    CHECK(step_value(g, 7.25f, 10) == 9.75f);
}

NNH_TEST(live_steps_off_grid_values_move_to_a_neighbour) {
    const FloatGrid g = fgrid(0, 10, 0.25f);
    CHECK(step_value(g, 4.6f, 1) == 4.75f);
    CHECK(step_value(g, 4.6f, -1) == 4.5f);
    CHECK(step_value(g, 4.6f, 10) == 7.0f);
}

NNH_TEST(live_steps_tenth_grid_matches_the_formula) {
    const FloatGrid g = fgrid(-1, 1, 0.1f);
    CHECK(g.n == 20);
    constexpr float k3 = -1.0f + 3.0f * 0.1f;
    constexpr float k4 = -1.0f + 4.0f * 0.1f;
    CHECK(same_bits(grid_value(g, 3), k3));
    CHECK(same_bits(step_value(g, k3, 1), k4));
    CHECK(same_bits(step_value(g, k4, -1), k3));
    CHECK(same_bits(step_value(g, -1.0f, 20), 1.0f));
}

NNH_TEST(live_steps_tenth_grid_never_drifts) {
    const FloatGrid g = fgrid(-1, 1, 0.1f);
    float v = -1.0f;
    for (int i = 0; i < 20; ++i) v = step_value(g, v, 1);
    CHECK(v == 1.0f);
    for (int i = 0; i < 20; ++i) v = step_value(g, v, -1);
    CHECK(same_bits(v, -1.0f));
    for (int i = 0; i < 7; ++i) v = step_value(g, v, 1);
    constexpr float k7 = -1.0f + 7.0f * 0.1f;
    CHECK(same_bits(v, k7));
}

NNH_TEST(live_steps_max_off_the_grid_is_a_stop) {
    const FloatGrid g = fgrid(0, 1, 0.3f);
    CHECK(g.n == 3);
    constexpr float k3 = 0.0f + 3.0f * 0.3f;
    CHECK(same_bits(step_value(g, 0.6f, 1), k3));
    CHECK(step_value(g, k3, 1) == 1.0f);
    CHECK(same_bits(step_value(g, 1.0f, -1), k3));
    CHECK(step_value(g, 0.0f, 10) == 1.0f);
}

NNH_TEST(live_steps_negative_zero_steps_like_zero) {
    const FloatGrid g = fgrid(-1, 1, 1);
    CHECK(step_value(g, -0.0f, 1) == 1.0f);
    CHECK(step_value(g, -0.0f, -1) == -1.0f);
    CHECK(step_value(g, 0.5f, -1) == 0.0f);
}

NNH_TEST(live_steps_min_bits_are_kept_at_index_zero) {
    const FloatGrid g = fgrid(-0.0f, 1, 0.5f);
    CHECK(same_bits(grid_value(g, 0), -0.0f));
    CHECK(same_bits(step_value(g, 0.5f, -1), -0.0f));
}

NNH_TEST(live_steps_two_to_the_24_steps_is_the_limit) {
    FloatGrid g;
    CHECK(make_float_grid(0, 16777216.0f, 1, g) == RangeError::None);
    CHECK(g.n == 16777216u);
    CHECK(make_float_grid(0, 33554432.0f, 1, g) == RangeError::TooManySteps);
    CHECK(step_value(g, 16777215.0f, 1) == 16777216.0f);
    IntGrid i;
    CHECK(make_int_grid(0, 16777216, 1, i) == RangeError::None);
    CHECK(make_int_grid(0, 16777217, 1, i) == RangeError::TooManySteps);
}

NNH_TEST(live_steps_refuse_bad_float_ranges) {
    FloatGrid g;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    CHECK(make_float_grid(nan, 1, 0.1f, g) == RangeError::NotFinite);
    CHECK(make_float_grid(0, inf, 0.1f, g) == RangeError::NotFinite);
    CHECK(make_float_grid(1, 1, 0.1f, g) == RangeError::Empty);
    CHECK(make_float_grid(2, 1, 0.1f, g) == RangeError::Empty);
    CHECK(make_float_grid(0, 1, 0, g) == RangeError::BadStep);
    CHECK(make_float_grid(0, 1, -0.5f, g) == RangeError::BadStep);
    CHECK(make_float_grid(0, 1, nan, g) == RangeError::BadStep);
}

NNH_TEST(live_steps_refuse_bad_int_ranges) {
    IntGrid g;
    CHECK(make_int_grid(3, 3, 1, g) == RangeError::Empty);
    CHECK(make_int_grid(0, 10, 0, g) == RangeError::BadStep);
    CHECK(make_int_grid(0, 10, -1, g) == RangeError::BadStep);
}

NNH_TEST(live_steps_int_step_one) {
    const IntGrid g = igrid(0, 32, 1);
    CHECK(g.n == 32);
    CHECK(step_value(g, 6, 1) == 7);
    CHECK(step_value(g, 6, -10) == 0);
    CHECK(step_value(g, 30, 10) == 32);
}

NNH_TEST(live_steps_int_step_five_moves_off_grid_values_to_neighbours) {
    const IntGrid g = igrid(0, 100, 5);
    CHECK(step_value(g, 7, 1) == 10);
    CHECK(step_value(g, 7, -1) == 5);
    CHECK(step_value(g, 10, 10) == 60);
}

NNH_TEST(live_steps_int_step_seven_stops_at_an_off_grid_max) {
    const IntGrid g = igrid(0, 20, 7);
    CHECK(g.n == 2);
    CHECK(step_value(g, 14, 1) == 20);
    CHECK(step_value(g, 20, -1) == 14);
    CHECK(step_value(g, 3, 10) == 20);
    CHECK(step_value(g, 0, -1) == 0);
}

NNH_TEST(live_steps_int_extreme_range_has_no_overflow) {
    const IntGrid g = igrid(-2147483647 - 1, 2147483647, 1 << 24);
    CHECK(g.n == 255);
    CHECK(step_value(g, 2147483647, 1) == 2147483647);
    CHECK(step_value(g, -2147483647 - 1, -1) == -2147483647 - 1);
    CHECK(step_value(g, -2147483647 - 1, 1) == -2147483647 - 1 + (1 << 24));
    CHECK(step_value(g, 2147483647, -1) == 2130706432); // INT32_MIN + 255 · 2^24
}

NNH_TEST(live_steps_drag_on_every_pixel_of_a_thirteen_pixel_bar) {
    const std::uint32_t expected[14] = {0, 0, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4};
    for (int x = 0; x <= 13; ++x) CHECK(drag_index(100 + x, 100, 13, 4) == expected[x]);
    CHECK(drag_index(90, 100, 13, 4) == 0);
    CHECK(drag_index(140, 100, 13, 4) == 4);
}

NNH_TEST(live_steps_drag_values_are_grid_points) {
    const FloatGrid g = fgrid(0, 1, 0.25f);
    CHECK(drag_value(g, 105, 100, 13) == 0.5f);
    CHECK(drag_value(g, 113, 100, 13) == 1.0f);
    const IntGrid i = igrid(0, 20, 7);
    CHECK(drag_value(i, 113, 100, 13) == 14); // k = (13·2 + 6) / 13 = 2
    CHECK(drag_index(113, 100, 13, 16777216u) == 16777216u);
}

NNH_TEST(live_steps_bar_fill_truncates_and_clamps) {
    const FloatGrid g = fgrid(0, 10, 0.25f);
    CHECK(bar_fill(g, 2.5f, 170) == 42);
    CHECK(bar_fill(g, 10.0f, 170) == 170);
    CHECK(bar_fill(g, 0.0f, 170) == 0);
    CHECK(bar_fill(g, -5.0f, 170) == 0);
    CHECK(bar_fill(g, 50.0f, 170) == 170);
    const IntGrid i = igrid(0, 32, 1);
    CHECK(bar_fill(i, 6, 170) == 31);
    CHECK(bar_fill(i, 32, 170) == 170);
}

NNH_TEST(live_steps_colour_channels_clamp) {
    CHECK(step_channel(64, 1) == 65);
    CHECK(step_channel(64, -10) == 54);
    CHECK(step_channel(250, 10) == 255);
    CHECK(step_channel(3, -10) == 0);
    CHECK(step_channel(255, 1) == 255);
}

NNH_TEST(live_steps_colour_channel_helpers) {
    const std::uint32_t c = pack_rgba(0xFF, 0xC0, 0x40, 0x80);
    CHECK(c == 0x8040C0FFu);
    CHECK(channel(c, 2) == 0x40);
    CHECK(with_channel(c, 1, 0x11) == 0x804011FFu);
}

NNH_TEST(live_steps_enums_stop_at_the_ends) {
    CHECK(step_enum(1, 4, 1) == 2);
    CHECK(step_enum(3, 4, 1) == 3);
    CHECK(step_enum(0, 4, -1) == 0);
    CHECK(step_enum(1, 4, 10) == 3);
    CHECK(step_enum(2, 4, -10) == 0);
    CHECK(step_enum(0, 1, 1) == 0);
}

NNH_TEST(live_steps_refusals) {
    const ValueRule f{Kind::Float, float_bits(0.0f), float_bits(10.0f), 0};
    CHECK(check_value(f, float_bits(5.25f)) == Refusal::None);
    CHECK(check_value(f, float_bits(10.0f)) == Refusal::None);
    CHECK(check_value(f, float_bits(-0.0f)) == Refusal::None);
    CHECK(check_value(f, float_bits(10.25f)) == Refusal::OutOfRange);
    CHECK(check_value(f, float_bits(std::numeric_limits<float>::quiet_NaN())) == Refusal::NotFinite);
    CHECK(check_value(f, float_bits(std::numeric_limits<float>::infinity())) == Refusal::NotFinite);
    const ValueRule i{Kind::Int32, static_cast<std::uint32_t>(-5), 5, 0};
    CHECK(check_value(i, static_cast<std::uint32_t>(-5)) == Refusal::None);
    CHECK(check_value(i, static_cast<std::uint32_t>(-6)) == Refusal::OutOfRange);
    CHECK(check_value(i, 6) == Refusal::OutOfRange);
    const ValueRule b{Kind::Bool, 0, 1, 0};
    CHECK(check_value(b, 1) == Refusal::None);
    CHECK(check_value(b, 2) == Refusal::OutOfRange);
    const ValueRule e{Kind::Enum, 0, 0, 3};
    CHECK(check_value(e, 2) == Refusal::None);
    CHECK(check_value(e, 3) == Refusal::OutOfRange);
    const ValueRule c{Kind::Color, 0, 0, 0};
    CHECK(check_value(c, 0xFFFFFFFFu) == Refusal::None);
    CHECK(check_value(ValueRule{Kind::Action, 0, 0, 0}, 0) == Refusal::WrongKind);
    CHECK(check_value(ValueRule{Kind::Vec2, 0, 0, 0}, 0) == Refusal::WrongKind);
}
