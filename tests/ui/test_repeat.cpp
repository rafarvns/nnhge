// Repeat (spec 028 T4.1, FR-22): a direction or page fires on its press, then at delay, delay + interval, ... frames
// held; one direction per frame (the newest pressed, ties Up, Down, Left, Right); delay 0 takes the input's own repeats.

#include "test.hpp"
#include "ui/ui_repeat.hpp"

using nnh::detail::RepeatFired;
using nnh::detail::RepeatState;
using nnh::detail::repeat_step;

namespace {

constexpr std::uint8_t kUp = 1, kDown = 2, kLeft = 4, kRight = 8, kConfirm = 16, kPageBack = 64;

/// Frames (0 = the press) at which `bit` fires while held for `frames` frames.
int fires(int delay, int interval, int frames, std::uint8_t bit, int* out, int capacity) {
    RepeatState s;
    int n = 0;
    for (int f = 0; f < frames; ++f) {
        const RepeatFired r = repeat_step(s, bit, f == 0 ? bit : 0, 0, delay, interval);
        if (((r.fresh | r.repeat) & bit) != 0 && n < capacity) out[n++] = f;
    }
    return n;
}

} // namespace

NNH_TEST(ui_repeat_24_6_fires_at_the_press_then_24_30_36) {
    int at[16];
    const int n = fires(24, 6, 40, kLeft, at, 16);
    CHECK(n == 4 && at[0] == 0 && at[1] == 24 && at[2] == 30 && at[3] == 36);
}

NNH_TEST(ui_repeat_1_1_fires_every_frame) {
    int at[16];
    CHECK(fires(1, 1, 5, kDown, at, 16) == 5);
}

NNH_TEST(ui_repeat_255_255) {
    int at[16];
    const int n = fires(255, 255, 600, kDown, at, 16);
    CHECK(n == 3 && at[1] == 255 && at[2] == 510);
}

NNH_TEST(ui_repeat_marks_the_press_fresh_and_the_rest_repeats) {
    RepeatState s;
    RepeatFired r = repeat_step(s, kUp, kUp, 0, 2, 1);
    CHECK(r.fresh == kUp && r.repeat == 0);
    r = repeat_step(s, kUp, 0, 0, 2, 1);
    CHECK(r.fresh == 0 && r.repeat == 0);
    r = repeat_step(s, kUp, 0, 0, 2, 1);
    CHECK(r.fresh == 0 && r.repeat == kUp);
}

NNH_TEST(ui_repeat_pages_repeat_and_confirm_doesnt) {
    int at[16];
    CHECK(fires(4, 2, 9, kPageBack, at, 16) == 4); // 0, 4, 6, 8
    CHECK(at[1] == 4 && at[3] == 8);
}

NNH_TEST(ui_repeat_confirm_fires_only_on_its_press) {
    int at[16];
    CHECK(fires(4, 2, 20, kConfirm, at, 16) == 1);
}

NNH_TEST(ui_repeat_the_newest_direction_wins) {
    RepeatState s;
    repeat_step(s, kDown, kDown, 0, 24, 6);
    RepeatFired r = repeat_step(s, kDown | kRight, kRight, 0, 24, 6);
    CHECK(r.fresh == kRight);
    // Down still held: it doesn't fire while Right is the newest
    for (int f = 2; f < 30; ++f) {
        r = repeat_step(s, kDown | kRight, 0, 0, 24, 6);
        CHECK((r.repeat & kDown) == 0);
    }
}

NNH_TEST(ui_repeat_ties_go_up_down_left_right) {
    RepeatState s;
    CHECK(repeat_step(s, kLeft | kDown, kLeft | kDown, 0, 24, 6).fresh == kDown);
    RepeatState t;
    CHECK(repeat_step(t, kRight | kLeft, kRight | kLeft, 0, 24, 6).fresh == kLeft);
}

NNH_TEST(ui_repeat_delay_0_takes_the_given_repeats) {
    RepeatState s;
    RepeatFired r = repeat_step(s, kUp, kUp, 0, 0, 6);
    CHECK(r.fresh == kUp);
    r = repeat_step(s, kUp, 0, 0, 0, 6);
    CHECK(r.repeat == 0);
    r = repeat_step(s, kUp, 0, kUp, 0, 6);
    CHECK(r.repeat == kUp);
}

NNH_TEST(ui_repeat_release_restarts_the_count) {
    RepeatState s;
    for (int f = 0; f < 20; ++f) repeat_step(s, kUp, f == 0 ? kUp : 0, 0, 24, 6);
    repeat_step(s, 0, 0, 0, 24, 6);
    int fired = 0;
    for (int f = 0; f < 30; ++f) {
        const RepeatFired r = repeat_step(s, kUp, f == 0 ? kUp : 0, 0, 24, 6);
        fired += ((r.fresh | r.repeat) & kUp) != 0 ? 1 : 0;
    }
    CHECK(fired == 2); // the press and 24
}
