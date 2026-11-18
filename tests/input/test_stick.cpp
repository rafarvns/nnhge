#include <cmath>
#include <limits>

#include "input/stick.hpp"
#include "test.hpp"

namespace {
using nnh::detail::shape_stick;
using nnh::detail::kStickDeadZone;

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
float length(nnh::Stick s) { return std::sqrt(s.x * s.x + s.y * s.y); }
} // namespace

NNH_TEST(a_stick_at_rest_reads_zero) {
    const nnh::Stick s = shape_stick(0.0f, 0.0f);
    CHECK(s.x == 0.0f && s.y == 0.0f);
}

NNH_TEST(a_stick_inside_the_dead_zone_reads_exactly_zero) {
    const nnh::Stick s = shape_stick(kStickDeadZone * 0.9f, -kStickDeadZone * 0.3f);
    CHECK(s.x == 0.0f && s.y == 0.0f);
}

NNH_TEST(a_stick_just_outside_the_dead_zone_reads_a_small_value) {
    const nnh::Stick s = shape_stick(kStickDeadZone + 0.01f, 0.0f);
    CHECK(s.x > 0.0f && s.x < 0.05f);
    CHECK(s.y == 0.0f);
}

NNH_TEST(a_stick_pushed_fully_reads_one_in_that_direction) {
    CHECK(near(shape_stick(1.0f, 0.0f).x, 1.0f));
    CHECK(near(shape_stick(0.0f, -1.0f).y, -1.0f));
}

NNH_TEST(a_stick_halfway_reads_about_half_after_rescale) {
    const float expected = (0.55f - kStickDeadZone) / (1.0f - kStickDeadZone);
    CHECK(near(shape_stick(0.55f, 0.0f).x, expected));
}

NNH_TEST(a_stick_keeps_its_direction) {
    const nnh::Stick s = shape_stick(0.3f, 0.4f); // length 0.5
    CHECK(near(s.y / s.x, 0.4f / 0.3f));
}

NNH_TEST(a_stick_beyond_the_range_is_clamped_to_the_unit_circle) {
    const nnh::Stick s = shape_stick(1.0f, 1.0f);
    CHECK(near(length(s), 1.0f));
    CHECK(near(s.x, s.y));
    CHECK(near(shape_stick(3.0f, 0.0f).x, 1.0f));
}

NNH_TEST(a_stick_with_non_finite_input_reads_zero) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const nnh::Stick a = shape_stick(nan, 0.5f);
    const nnh::Stick b = shape_stick(0.5f, inf);
    CHECK(a.x == 0.0f && a.y == 0.0f);
    CHECK(b.x == 0.0f && b.y == 0.0f);
}
