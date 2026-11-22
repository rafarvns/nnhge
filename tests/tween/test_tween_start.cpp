// Starting tweens (spec 015 FR-4..FR-6; T2.3): to/by/from_to, start values captured when the tween starts,
// `from` written at creation, a full set, invalid requests.

#include "tween_fixture.hpp"

using nnh::Duration;
using nnh::Frames;
using nnh::TweenHandle;
using nnh::TweenSet;
using nnh::Vec2;
using tween_test::run;

namespace {

TweenSet* g_set = nullptr;
float g_value = 0;

void start_in_full_set() {
    for (int i = 0; i < 3; ++i) {
        (void)g_set->to(&g_value, 1.0f, Frames{10});
    }
}
void start_with_negative_time() {
    (void)g_set->to(&g_value, 1.0f, Duration{-1});
}
void start_with_negative_delay() {
    (void)g_set->to(&g_value, 1.0f, Frames{1}, {.delay = Duration{-3}});
}
void start_with_zero_loops() {
    (void)g_set->to(&g_value, 1.0f, Frames{1}, {.loops = 0});
}
void start_forever_with_zero_length() {
    (void)g_set->to(&g_value, 1.0f, Frames{0}, {.loops = nnh::kForever});
}
void start_with_null_target() {
    (void)g_set->to(static_cast<float*>(nullptr), 1.0f, Frames{1});
}
void start_in_default_set() {
    TweenSet empty;
    (void)empty.to(&g_value, 1.0f, Frames{1});
}

} // namespace

NNH_TEST(to_captures_the_start_value_on_the_first_update) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 10.0f, Frames{10});
    CHECK(set.running(h));
    CHECK(x == 0.0f); // nothing written at creation
    x = 5;            // changed before the tween starts: the tween starts from here
    run(set, 5);
    CHECK(x == 7.5f);
}

NNH_TEST(to_after_a_delay_captures_the_value_when_the_delay_ends) {
    TweenSet set({.tweens = 4});
    float x = 0;
    set.to(&x, 10.0f, Frames{10}, {.delay = Frames{3}});
    run(set, 2);
    x = 4;
    run(set, 1); // the delay ends: start = 4
    CHECK(x == 4.0f);
    run(set, 5);
    CHECK(x == 7.0f);
}

NNH_TEST(by_moves_relative_to_the_captured_start) {
    TweenSet set({.tweens = 4});
    Vec2 p{1, 2};
    set.by(&p, {10, -10}, Frames{2});
    p = {5, 5};
    run(set, 1);
    CHECK(p == (Vec2{10, 0}));
    run(set, 1);
    CHECK(p == (Vec2{15, -5}));
}

NNH_TEST(from_to_without_delay_writes_from_at_creation) {
    TweenSet set({.tweens = 4});
    float x = 99;
    set.from_to(&x, 1.0f, 3.0f, Frames{2});
    CHECK(x == 1.0f);
    run(set, 1);
    CHECK(x == 2.0f);
}

NNH_TEST(from_to_with_a_delay_waits_before_writing) {
    TweenSet set({.tweens = 4});
    float x = 99;
    set.from_to(&x, 1.0f, 3.0f, Frames{2}, {.delay = Frames{2}});
    CHECK(x == 99.0f);
    run(set, 1);
    CHECK(x == 99.0f);
    run(set, 2);
    CHECK(x == 2.0f);
}

NNH_TEST(a_duration_and_frames_give_the_same_tween) {
    TweenSet set({.tweens = 4});
    float a = 0, b = 0;
    set.to(&a, 1.0f, Duration::from_ms(500));
    set.to(&b, 1.0f, Frames{30});
    for (int i = 0; i < 30; ++i) {
        set.update();
        CHECK(a == b);
    }
    CHECK(a == 1.0f && set.active() == 0);
}

NNH_TEST(a_zero_length_tween_finishes_on_its_first_update) {
    TweenSet set({.tweens = 4});
    float x = 0;
    const TweenHandle h = set.to(&x, 5.0f, Frames{0});
    CHECK(x == 0.0f);
    set.update();
    CHECK(x == 5.0f);
    CHECK(!set.running(h));
}

NNH_TEST(a_full_set_returns_an_empty_handle) {
    tween_test::restart();
    TweenSet set({.tweens = 2});
    g_set = &set;
    CHECK(tween_test::asserts_or_is_ignored(start_in_full_set));
    if (!nnh::build::kAssertsEnabled) {
        CHECK(set.active() == 2);
        CHECK(set.to(&g_value, 1.0f, Frames{10}) == TweenHandle{});
        CHECK(tween_test::last_log_mentions("full"));
    }
    nnh::shutdown();
}

NNH_TEST(invalid_start_requests_assert_or_are_ignored) {
    TweenSet set({.tweens = 8});
    g_set = &set;
    CHECK(tween_test::asserts_or_is_ignored(start_with_negative_time));
    CHECK(tween_test::asserts_or_is_ignored(start_with_negative_delay));
    CHECK(tween_test::asserts_or_is_ignored(start_with_zero_loops));
    CHECK(tween_test::asserts_or_is_ignored(start_forever_with_zero_length));
    CHECK(tween_test::asserts_or_is_ignored(start_with_null_target));
    CHECK(tween_test::asserts_or_is_ignored(start_in_default_set));
    if (!nnh::build::kAssertsEnabled) {
        CHECK(set.active() == 0);
        CHECK(set.to(&g_value, 1.0f, Duration{-1}) == TweenHandle{});
    }
}
