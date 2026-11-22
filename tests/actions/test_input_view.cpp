// The input view (spec 017 T1.1): pushes per direction, stick lengths, and the view of the public queries.

#include "actions_fixture.hpp"

using namespace actions_test;
using nnh::detail::InputView;

NNH_TEST(input_view_splits_each_stick_into_four_pushes) {
    const InputView v = view(0, {0.6f, -0.8f}, {-0.3f, 0.4f});
    CHECK(v.push[0] == 0.0f && near(v.push[1], 0.8f)); // Circle Pad up, down
    CHECK(v.push[2] == 0.0f && near(v.push[3], 0.6f)); // left, right
    CHECK(near(v.push[4], 0.4f) && v.push[5] == 0.0f); // C-Stick up, down
    CHECK(near(v.push[6], 0.3f) && v.push[7] == 0.0f); // left, right
}

NNH_TEST(input_view_precomputes_stick_lengths) {
    const InputView v = view(0, {0.6f, -0.8f}, {0.0f, 0.5f});
    CHECK(near(v.length[0], 1.0f));
    CHECK(near(v.length[1], 0.5f));
}

NNH_TEST(input_view_keeps_buttons_and_touch) {
    const InputView v = view(bit(Button::A) | bit(Button::ZL), {}, {}, true, {12, 34});
    CHECK(v.held == (bit(Button::A) | bit(Button::ZL)));
    CHECK(v.touching && v.touch_x == 12 && v.touch_y == 34);
}

NNH_TEST(the_current_view_reads_this_frames_public_queries) {
    restart();
    frame({.buttons = bit(Button::B), .cx = 1.0f, .sx = -1.0f, .touch = true, .tx = 5, .ty = 6});
    const InputView v = nnh::detail::current_input_view();
    CHECK(v.held == bit(Button::B));
    CHECK(near(v.push[3], 1.0f) && near(v.push[6], 1.0f));
    CHECK(v.touching && v.touch_x == 5 && v.touch_y == 6);
    nnh::shutdown();
}

NNH_TEST(the_current_view_reads_a_zero_c_stick_without_the_capability) {
    restart(/*old_3ds=*/true);
    frame({.sx = 1.0f, .sy = 1.0f});
    const InputView v = nnh::detail::current_input_view();
    CHECK(v.length[1] == 0.0f && v.push[4] == 0.0f && v.push[7] == 0.0f);
    nnh::shutdown();
}
