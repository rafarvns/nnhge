// The tween set's frame clock (spec 015 FR-16, FR-17, AC-2; T2.4): whole updates, no drift, exact speed,
// advance with a remainder, identical runs give identical bits. (The Duration → ticks table is
// tests/time/test_frame_time.cpp.)

#include "nnh/gfx.hpp"
#include "tween_fixture.hpp"

#include <bit>
#include <cstdint>

using nnh::Duration;
using nnh::Ease;
using nnh::Frames;
using nnh::FrameTime;
using nnh::TweenHandle;
using nnh::TweenSet;

namespace {

/// Updates until `h` stops running; returns the update count (or -1 past `limit`).
int updates_to_finish(TweenSet& set, TweenHandle h, int limit = 100'000) {
    for (int i = 1; i <= limit; ++i) {
        set.update();
        if (!set.running(h)) return i;
    }
    return -1;
}

int length_in_updates(FrameTime time, float speed = 1.0f, int loops = 1) {
    TweenSet set({.tweens = 2});
    set.set_speed(speed);
    float x = 0;
    return updates_to_finish(set, set.to(&x, 1.0f, time, {.loops = loops}));
}

/// A fixed script of tweens; returns an FNV-1a hash of every value's bits after every update.
std::uint32_t script_hash() {
    TweenSet set({.tweens = 8});
    float a = 0;
    nnh::Vec2 b{1, 2};
    nnh::Color c{10, 20, 30, 255};
    set.to(&a, 3.0f, Duration::from_ms(700),
           {.ease = Ease::ElasticOut, .loops = 3, .loop_mode = nnh::LoopMode::Yoyo});
    set.by(&b, {100, -7}, Frames{41}, {.ease = Ease::SineInOut, .delay = Frames{5}});
    set.to(&c, nnh::Color{200, 0, 90, 0}, Duration::from_ms(333), {.ease = Ease::BounceOut});
    std::uint32_t h = 2166136261u;
    auto mix = [&h](float v) {
        const std::uint32_t bits = std::bit_cast<std::uint32_t>(v);
        for (int k = 0; k < 4; ++k) {
            h = (h ^ ((bits >> (8 * k)) & 0xFF)) * 16777619u;
        }
    };
    for (int i = 0; i < 200; ++i) {
        set.update();
        mix(a);
        mix(b.x);
        mix(b.y);
        mix(static_cast<float>(c.r + c.g * 256 + c.b * 65536 + c.a * 16777216.0f));
    }
    return h;
}

TweenSet* g_set = nullptr;
void set_negative_speed() {
    g_set->set_speed(-1.0f);
}
void set_huge_speed() {
    g_set->set_speed(65.0f);
}
void advance_negative() {
    g_set->advance(Duration{-1});
}

} // namespace

NNH_TEST(half_a_second_takes_30_updates_and_a_second_60) {
    CHECK(length_in_updates(Duration::from_ms(500)) == 30);
    CHECK(length_in_updates(Duration::from_seconds(1)) == 60);
    CHECK(length_in_updates(Duration::from_us(16'667)) == 1);
}

NNH_TEST(n_frames_end_on_update_n) {
    for (int n = 1; n <= 61; n += 6) {
        CHECK(length_in_updates(Frames{n}) == n);
    }
}

NNH_TEST(ten_loops_of_a_second_end_on_update_600) {
    CHECK(length_in_updates(Duration::from_seconds(1), 1.0f, 10) == 600);
    CHECK(length_in_updates(Duration::from_ms(100), 1.0f, 50) == 300);
}

NNH_TEST(speed_scales_the_clock_exactly) {
    CHECK(length_in_updates(Duration::from_seconds(1), 0.5f) == 120);
    CHECK(length_in_updates(Duration::from_seconds(1), 2.0f) == 30);
    CHECK(length_in_updates(Duration::from_seconds(1), 0.25f, 10) == 2400);
    TweenSet set({.tweens = 1});
    set.set_speed(0.5f);
    CHECK(set.speed() == 0.5f);
    set.set_speed(1.0f / 3.0f);
    CHECK(set.speed() == 1365.0f / 4096.0f); // rounded once to 1/4096
    set.set_speed(0.9999f);                  // 4095.59 ticks: rounded to nearest, not truncated
    CHECK(set.speed() == 1.0f);
}

NNH_TEST(a_frozen_set_starts_nothing_and_finishes_nothing) {
    TweenSet set({.tweens = 4});
    set.set_speed(0.0f);
    float x = 7, y = 0;
    int calls = 0;
    set.to(&x, 1.0f, Frames{0});                                  // would finish on its first update
    set.from_to(&y, 5.0f, 6.0f, Frames{4}, {.delay = Frames{0}}); // started at creation
    set.after(Frames{0}, [&calls] { ++calls; });
    tween_test::run(set, 10);
    CHECK(x == 7.0f && calls == 0 && set.active() == 3);
}

NNH_TEST(speed_zero_freezes_the_set) {
    TweenSet set({.tweens = 2});
    float x = 0;
    const TweenHandle h = set.to(&x, 1.0f, Frames{2});
    set.update();
    set.set_speed(0.0f);
    tween_test::run(set, 100);
    CHECK(x == 0.5f && set.running(h));
    set.set_speed(1.0f);
    set.update();
    CHECK(x == 1.0f);
}

NNH_TEST(advance_scales_by_speed_and_keeps_the_remainder) {
    TweenSet set({.tweens = 2});
    float x = 0;
    const TweenHandle h = set.to(&x, 1.0f, FrameTime::from_ticks(4));
    set.set_speed(0.5f);
    set.advance(FrameTime::from_ticks(1)); // 0.5 tick: nothing yet
    CHECK(set.progress(h) == 0.0f);
    set.advance(FrameTime::from_ticks(1)); // 1 tick in all
    CHECK(set.progress(h) == 0.25f);
    set.advance(FrameTime::from_ticks(5)); // + 2.5 → 3.5 ticks
    CHECK(set.progress(h) == 0.75f);
    set.advance(FrameTime::from_ticks(1)); // 4 ticks
    CHECK(!set.running(h));
}

NNH_TEST(advance_by_a_duration_matches_updates) {
    TweenSet a({.tweens = 2}), b({.tweens = 2});
    float xa = 0, xb = 0;
    a.to(&xa, 1.0f, Duration::from_seconds(1), {.ease = Ease::CubicInOut});
    b.to(&xb, 1.0f, Duration::from_seconds(1), {.ease = Ease::CubicInOut});
    tween_test::run(a, 15);
    b.advance(Duration::from_ms(250));
    CHECK(xa == xb);
}

NNH_TEST(identical_runs_give_identical_bits) {
    const std::uint32_t first = script_hash();
    CHECK(script_hash() == first);
}

NNH_TEST(invalid_speeds_and_times_assert_or_are_ignored) {
    TweenSet set({.tweens = 2});
    g_set = &set;
    CHECK(tween_test::asserts_or_is_ignored(set_negative_speed));
    CHECK(tween_test::asserts_or_is_ignored(set_huge_speed));
    CHECK(tween_test::asserts_or_is_ignored(advance_negative));
    if (!nnh::build::kAssertsEnabled) CHECK(set.speed() == 1.0f);
}
