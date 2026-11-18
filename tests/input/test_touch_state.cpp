#include "input/touch_state.hpp"
#include "test.hpp"

namespace {
using nnh::detail::TouchState;
} // namespace

NNH_TEST(no_touch_reports_nothing) {
    TouchState t;
    t.update(false, 0, 0);
    CHECK(!t.touching && !t.started() && !t.ended());
}

NNH_TEST(touch_start_is_reported_only_on_first_frame) {
    TouchState t;
    t.update(true, 10, 20);
    CHECK(t.touching && t.started() && !t.ended());
    t.update(true, 11, 21);
    CHECK(t.touching && !t.started());
}

NNH_TEST(touch_end_is_reported_only_on_first_frame) {
    TouchState t;
    t.update(true, 10, 20);
    t.update(false, 0, 0);
    CHECK(!t.touching && t.ended() && !t.started());
    t.update(false, 0, 0);
    CHECK(!t.ended());
}

NNH_TEST(touch_position_follows_the_touch) {
    TouchState t;
    t.update(true, 10, 20);
    t.update(true, 300, 200);
    CHECK(t.position.x == 300 && t.position.y == 200);
}

NNH_TEST(touch_position_keeps_the_last_point_after_release) {
    TouchState t;
    t.update(true, 50, 60);
    t.update(false, 0, 0);
    CHECK(t.position.x == 50 && t.position.y == 60);
    t.update(false, 7, 7);
    CHECK(t.position.x == 50 && t.position.y == 60);
}

NNH_TEST(touch_position_is_clamped_to_the_bottom_screen) {
    TouchState t;
    t.update(true, -5, 400);
    CHECK(t.position.x == 0 && t.position.y == 239);
    t.update(true, 320, -1);
    CHECK(t.position.x == 319 && t.position.y == 0);
}
