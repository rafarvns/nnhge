#include "input/input_state.hpp"
#include "nnh/input.hpp"
#include "test.hpp"

namespace {
constexpr std::uint32_t A = static_cast<std::uint32_t>(nnh::Button::A);
constexpr std::uint32_t B = static_cast<std::uint32_t>(nnh::Button::B);
} // namespace

NNH_TEST(press_is_reported_only_on_first_frame) {
    nnh::detail::InputState s;
    s.update(A);
    CHECK(s.pressed() == A);
    s.update(A);
    CHECK(s.pressed() == 0);
    CHECK(s.held == A);
}

NNH_TEST(release_is_reported_only_on_first_frame) {
    nnh::detail::InputState s;
    s.update(A | B);
    s.update(B);
    CHECK(s.released() == A);
    s.update(B);
    CHECK(s.released() == 0);
}

NNH_TEST(buttons_are_independent) {
    nnh::detail::InputState s;
    s.update(A);
    s.update(B);
    CHECK(s.pressed() == B);
    CHECK(s.released() == A);
    CHECK(s.held == B);
}
