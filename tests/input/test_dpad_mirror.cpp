#include "input/dpad_mirror.hpp"
#include "nnh/input.hpp"
#include "test.hpp"

namespace {
using nnh::detail::dpad_mirror;
using nnh::detail::kMirrorOff;
using nnh::detail::kMirrorOn;

constexpr std::uint32_t bit(nnh::Button b) { return static_cast<std::uint32_t>(b); }
constexpr std::uint32_t Up = bit(nnh::Button::Up);
constexpr std::uint32_t Down = bit(nnh::Button::Down);
constexpr std::uint32_t Left = bit(nnh::Button::Left);
constexpr std::uint32_t Right = bit(nnh::Button::Right);
} // namespace

NNH_TEST(mirror_centered_holds_nothing) { CHECK(dpad_mirror({0.0f, 0.0f}, 0) == 0); }

NNH_TEST(mirror_holds_the_direction_at_the_on_threshold) {
    CHECK(dpad_mirror({kMirrorOn, 0.0f}, 0) == Right);
    CHECK(dpad_mirror({-kMirrorOn, 0.0f}, 0) == Left);
    CHECK(dpad_mirror({0.0f, kMirrorOn}, 0) == Up);
    CHECK(dpad_mirror({0.0f, -kMirrorOn}, 0) == Down);
}

NNH_TEST(mirror_below_the_on_threshold_does_not_press) {
    CHECK(dpad_mirror({kMirrorOn - 0.01f, 0.0f}, 0) == 0);
}

NNH_TEST(mirror_keeps_holding_between_the_thresholds) {
    const float between = (kMirrorOn + kMirrorOff) / 2;
    CHECK(dpad_mirror({between, 0.0f}, Right) == Right);
    CHECK(dpad_mirror({0.0f, -between}, Down) == Down);
}

NNH_TEST(mirror_releases_below_the_off_threshold) {
    CHECK(dpad_mirror({kMirrorOff - 0.01f, 0.0f}, Right) == 0);
}

NNH_TEST(mirror_holds_two_directions_on_a_diagonal) {
    CHECK(dpad_mirror({0.7f, 0.7f}, 0) == (Up | Right));
}

NNH_TEST(mirror_switches_side_without_holding_both) {
    CHECK(dpad_mirror({-1.0f, 0.0f}, Right) == Left);
}

NNH_TEST(mirror_ignores_other_held_bits) {
    const std::uint32_t a = bit(nnh::Button::A);
    CHECK(dpad_mirror({0.0f, 0.0f}, a | Up) == 0);
}
