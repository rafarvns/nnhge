// The frame clock (T3.2; FR-7, FR-14, AC-2): exact loop counts, milliseconds, speeds, a Duration that is not a whole
// number of frames over 1000 loops, advance() with its remainder, determinism and the crossing bound.

#include "library_fixture.hpp"

using nnh::Duration;
using nnh::FrameTime;
using nnh::Frames;
using nnh::SpriteAnimator;

namespace {

struct Setup {
    nnh::SpriteLibrary lib = anim_test::strip(4);
    SpriteAnimator a{lib};
};

/// Updates until `loops()` reaches `n`; returns how many updates that took (or -1 past `limit`).
int updates_until_loops(SpriteAnimator& a, int n, int limit) {
    for (int u = 1; u <= limit; ++u) {
        a.update();
        if (a.loops() == n) return u;
    }
    return -1;
}

SpriteAnimator* g_bound = nullptr;
void cross_forty_frames() {
    (void)g_bound->advance(Frames{40});
}

} // namespace

NNH_TEST(four_frames_of_six_loop_every_24_updates_and_end_loop_ten_on_update_240) {
    Setup s;
    s.a.play(anim_test::clip(s.lib, "c", 4, 6));
    CHECK(updates_until_loops(s.a, 1, 1000) == 24);
    CHECK(updates_until_loops(s.a, 10, 1000) == 240 - 24);
    CHECK(s.a.position() == 0 && s.a.time_in_frame().ticks == 0);
}

NNH_TEST(a_hundred_milliseconds_is_six_updates) {
    Setup s;
    s.a.play(s.lib.add_clip({.name = "c", .count = 4, .frame_time = Duration::from_ms(100)}).value());
    for (int i = 0; i < 5; ++i) s.a.update();
    CHECK(s.a.position() == 0);
    s.a.update();
    CHECK(s.a.position() == 1 && s.a.time_in_frame().ticks == 0);
}

NNH_TEST(half_speed_doubles_the_counts_exactly) {
    Setup s;
    s.a.set_speed(0.5f);
    CHECK(s.a.speed() == 0.5f);
    s.a.play(anim_test::clip(s.lib, "c", 4, 6));
    CHECK(updates_until_loops(s.a, 1, 1000) == 48);
    CHECK(updates_until_loops(s.a, 10, 1000) == 480 - 48);
}

NNH_TEST(speed_one_and_a_half_is_exact_and_zero_freezes) {
    Setup s;
    s.a.set_speed(1.5f);
    s.a.play(anim_test::clip(s.lib, "c", 4, 6));
    for (int i = 0; i < 4; ++i) s.a.update(); // 4 × 6144 = 24 576: one frame
    CHECK(s.a.position() == 1 && s.a.time_in_frame().ticks == 0);
    s.a.set_speed(0);
    for (int i = 0; i < 100; ++i) s.a.update();
    CHECK(s.a.position() == 1 && s.a.time_in_frame().ticks == 0);
}

NNH_TEST(a_duration_of_no_whole_frames_never_drifts_over_a_thousand_loops) {
    Setup s;
    // 70 ms = 17 203.2 ticks → 17 203 per frame, 51 609 per loop: loop 1000 ends at 51 609 000 ticks, inside update
    // 12 600 (12 599 × 4096 = 51 605 504 < 51 609 000 ≤ 12 600 × 4096).
    s.a.play(s.lib.add_clip({.name = "c", .count = 3, .frame_time = Duration::from_ms(70)}).value());
    for (int i = 0; i < 12599; ++i) s.a.update();
    CHECK(s.a.loops() == 999);
    s.a.update();
    CHECK(s.a.loops() == 1000);
    CHECK(s.a.time_in_frame().ticks == 12600 * 4096 - 51609000);
}

NNH_TEST(advance_carries_its_sub_tick_remainder) {
    Setup s;
    s.a.play(anim_test::clip(s.lib, "c", 4, 6));
    s.a.set_speed(1.5f);
    (void)s.a.advance(FrameTime::from_ticks(1)); // 1.5 ticks: 1, remainder 0.5
    CHECK(s.a.time_in_frame().ticks == 1);
    (void)s.a.advance(FrameTime::from_ticks(1)); // 1.5 + 0.5: 2
    CHECK(s.a.time_in_frame().ticks == 3);
}

NNH_TEST(fixed_step_advances_match_updates) {
    Setup s;
    SpriteAnimator b{s.lib};
    const nnh::SpriteClipId c = anim_test::clip(s.lib, "c", 4, 5);
    s.a.play(c);
    b.play(c);
    const FrameTime step = Duration{16'666'667}; // time::FixedStep(60).step(), rounded to 4096 ticks
    for (int i = 0; i < 77; ++i) {
        s.a.update();
        (void)b.advance(step);
    }
    CHECK(s.a.position() == b.position() && s.a.loops() == b.loops());
    CHECK(s.a.time_in_frame() == b.time_in_frame());
}

NNH_TEST(an_advance_crosses_every_frame_in_order) {
    Setup s;
    const nnh::SpriteClipId c = anim_test::clip(s.lib, "c", 4, 2);
    for (int p = 0; p < 4; ++p) CHECK(s.lib.add_marker(c, p, "m", p).ok());
    s.a.play(c);
    const nnh::SpriteNotices n = s.a.advance(Frames{9}); // entry 0, then 1 2 3, wrap, 0: 4 crossings and 1 left
    CHECK(n.count == 6);
    const int values[] = {0, 1, 2, 3, -1, 0};
    for (int i = 0; i < 6; ++i) {
        if (values[i] < 0) CHECK(n.data[i].kind == nnh::SpriteNoticeKind::Looped);
        else CHECK(n.data[i].kind == nnh::SpriteNoticeKind::Marker && n.data[i].value == values[i]);
    }
    CHECK(s.a.time_in_frame() == FrameTime{Frames{1}});
}

NNH_TEST(two_identical_runs_give_identical_streams) {
    std::uint32_t sums[2] = {};
    for (std::uint32_t& sum : sums) {
        Setup s;
        const nnh::SpriteClipId c = anim_test::clip(s.lib, "c", 4, 3, nnh::SpriteMode::PingPong);
        CHECK(s.lib.add_marker(c, 2, "m").ok());
        s.a.play(c);
        s.a.set_speed(1.25f);
        for (int i = 0; i < 300; ++i) {
            for (const nnh::SpriteNotice& n : s.a.update()) sum = sum * 31 + static_cast<std::uint32_t>(n.kind) + n.marker;
            sum = sum * 31 + static_cast<std::uint32_t>(s.a.frame());
        }
    }
    CHECK(sums[0] == sums[1] && sums[0] != 0);
}

NNH_TEST(crossing_more_than_32_frames_in_one_call_asserts_or_jumps_silently) {
    Setup s;
    s.a.play(anim_test::clip(s.lib, "c", 4, 1));
    g_bound = &s.a;
    CHECK(anim_test::asserts_or_is_ignored(cross_forty_frames));
    if (!nnh::build::kAssertsEnabled) CHECK(s.a.position() == 0 && s.a.loops() == 10);
}

NNH_TEST(the_release_jump_skips_whole_loops_at_once) {
    if (nnh::build::kAssertsEnabled) return; // Debug/Development stop at the assert
    Setup s;
    s.a.play(anim_test::clip(s.lib, "c", 4, 1));
    const nnh::SpriteNotices n = s.a.advance(Duration::from_seconds(3000)); // 180 000 frames, 45 000 loops
    CHECK(n.count <= nnh::kSpriteMaxNotices);
    CHECK(s.a.position() == 0 && s.a.loops() == 45000);
}

NNH_TEST(invalid_speeds_and_times_assert_or_are_ignored) {
    static Setup* s = nullptr;
    Setup setup;
    s = &setup;
    s->a.play(anim_test::clip(s->lib, "c", 4, 1));
    CHECK(anim_test::asserts_or_is_ignored([] { s->a.set_speed(-1.0f); }));
    CHECK(anim_test::asserts_or_is_ignored([] { s->a.set_speed(65.0f); }));
    CHECK(anim_test::asserts_or_is_ignored([] { (void)s->a.advance(FrameTime::from_ticks(-5)); }));
    CHECK(s->a.speed() == 1.0f && s->a.position() == 0);
}
