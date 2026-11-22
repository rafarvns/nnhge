// Misuse matrix (spec 015 FR-18; T3.4): every misuse asserts in Debug/Development and, in Release, is ignored
// with an error log and never becomes undefined behaviour. (Full sets, invalid times, loops and targets,
// capacities, foreign handles, speeds and timer periods have their cases next to their features; this file
// adds the rest and checks the Release logs.)

#include "nnh/memory.hpp"
#include "tween_fixture.hpp"

using nnh::Duration;
using nnh::Frames;
using nnh::TweenHandle;
using nnh::TweenSet;

namespace {

struct Done {};

struct Holder {
    TweenSet set{{.tweens = 4}};
    float x = 0, y = 0;
};
Holder* g_holder = nullptr;

void finish_with_an_event_before_init() {
    g_holder->set.to(&g_holder->x, 1.0f, Frames{1}, {.publish = Done{}});
    g_holder->set.update();
}

void destroy_the_set_from_its_own_callback() {
    g_holder->set.to(&g_holder->x, 1.0f, Frames{1}, {.on_finish = [] { g_holder->set = TweenSet{}; }});
    g_holder->set.to(&g_holder->y, 1.0f, Frames{2});
    g_holder->set.update();
}

// Lives outside the call: an assert jumps out of it, which would skip a local set's destructor (a leak).
TweenSet* g_other = nullptr;

void use_a_foreign_handle() {
    const TweenHandle h = g_other->after(Frames{1}, [] {});
    g_holder->set.complete(h);
}

void over_an_hour() {
    (void)g_holder->set.to(&g_holder->x, 1.0f, Duration::from_seconds(3601));
}
void nan_speed() {
    g_holder->set.set_speed(0.0f / 0.0f);
}

} // namespace

NNH_TEST(publishing_on_finish_before_init_asserts_or_is_ignored) {
    nnh::shutdown();
    Holder h;
    g_holder = &h;
    CHECK(tween_test::asserts_or_is_ignored(finish_with_an_event_before_init));
    tween_test::recover_after_crash(h.set);
    CHECK(h.x == 1.0f); // the tween itself finished
}

NNH_TEST(destroying_a_set_from_its_own_callback_asserts_or_is_deferred_safely) {
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::detail::tween_memory_tag());
    {
        Holder h;
        g_holder = &h;
        CHECK(tween_test::asserts_or_is_ignored(destroy_the_set_from_its_own_callback));
        tween_test::recover_after_crash(h.set);
        if (!nnh::build::kAssertsEnabled) {
            CHECK(h.set.capacity() == 0); // replaced by the empty set
            CHECK(h.y == 0.0f);           // the old block's other tweens were killed, not updated
        }
    }
    CHECK(nnh::mem::tag_stats(nnh::detail::tween_memory_tag()).count == before.count);
}

NNH_TEST(release_logs_name_the_misuse) {
    if (nnh::build::kAssertsEnabled) return; // Debug/Development stop at the assert
    tween_test::restart();
    Holder h;
    g_holder = &h;
    TweenSet other({.tweens = 1});
    g_other = &other;
    use_a_foreign_handle();
    CHECK(tween_test::last_log_mentions("handle of another set"));
    over_an_hour();
    CHECK(tween_test::last_log_mentions("over 1 hour"));
    nan_speed();
    CHECK(tween_test::last_log_mentions("outside 0..64"));
    CHECK(h.set.speed() == 1.0f && h.set.active() == 0);
    nnh::shutdown();
}

NNH_TEST(foreign_handles_and_absurd_times_assert_in_debug) {
    Holder h;
    g_holder = &h;
    TweenSet other({.tweens = 1});
    g_other = &other;
    CHECK(tween_test::asserts_or_is_ignored(use_a_foreign_handle));
    CHECK(tween_test::asserts_or_is_ignored(over_an_hour));
    CHECK(tween_test::asserts_or_is_ignored(nan_speed));
}
