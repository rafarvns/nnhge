// FR-4 through the animator (T3.2): the positions shown after each update of clips of 1-frame frames, every mode
// and direction, the hold at the end, one- and two-frame clips.

#include "library_fixture.hpp"

using anim_test::clip;
using anim_test::positions;
using anim_test::same;
using nnh::SpriteDirection;
using nnh::SpriteMode;

namespace {

struct Setup {
    nnh::SpriteLibrary lib = anim_test::strip(4);
    nnh::SpriteAnimator a{lib};
};

} // namespace

NNH_TEST(a_looping_clip_wraps) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1));
    CHECK(s.a.position() == 0);
    int got[8];
    positions(s.a, got);
    CHECK(same(got, {1, 2, 3, 0, 1, 2, 3, 0}));
    CHECK(s.a.playing() && !s.a.finished() && s.a.loops() == 2);
}

NNH_TEST(a_once_clip_holds_its_last_frame) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1, SpriteMode::Once));
    int got[3];
    positions(s.a, got);
    CHECK(same(got, {1, 2, 3}) && s.a.playing());
    int held[3];
    positions(s.a, held);
    CHECK(same(held, {3, 3, 3}));
    CHECK(s.a.finished() && !s.a.playing() && s.a.frame() == 3);
}

NNH_TEST(a_repeat_clip_plays_n_times_then_holds) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1, SpriteMode::Repeat, 2));
    int got[9];
    positions(s.a, got);
    CHECK(same(got, {1, 2, 3, 0, 1, 2, 3, 3, 3}));
    CHECK(s.a.finished());
}

NNH_TEST(ping_pong_forever_never_doubles_the_turning_frames) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1, SpriteMode::PingPong));
    int got[12];
    positions(s.a, got);
    CHECK(same(got, {1, 2, 3, 2, 1, 0, 1, 2, 3, 2, 1, 0}));
    CHECK(s.a.loops() == 2 && !s.a.finished());
}

NNH_TEST(ping_pong_round_trips_end_on_the_first_frame) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1, SpriteMode::PingPong, 1)); // one round trip: 0 1 2 3 2 1 0
    int got[8];
    positions(s.a, got);
    CHECK(same(got, {1, 2, 3, 2, 1, 0, 0, 0}));
    CHECK(s.a.finished());
}

NNH_TEST(ping_pong_odd_passes_end_on_the_far_frame) {
    Setup s;
    auto c = s.lib.add_clip({.name = "c", .count = 4, .frame_time = nnh::Frames{1}, .mode = SpriteMode::PingPong,
                             .pingpong_passes = 3});
    s.a.play(c.value());
    int got[10];
    positions(s.a, got);
    CHECK(same(got, {1, 2, 3, 2, 1, 0, 1, 2, 3, 3}));
    CHECK(s.a.finished());
}

NNH_TEST(reverse_starts_from_the_last_frame_and_loops) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1, SpriteMode::Loop, 0, SpriteDirection::Reverse));
    CHECK(s.a.position() == 3 && s.a.frame() == 3);
    int got[5];
    positions(s.a, got);
    CHECK(same(got, {2, 1, 0, 3, 2}));
}

NNH_TEST(ping_pong_reverse_round_trip) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1, SpriteMode::PingPong, 1, SpriteDirection::Reverse));
    CHECK(s.a.position() == 3);
    int got[7];
    positions(s.a, got);
    CHECK(same(got, {2, 1, 0, 1, 2, 3, 3}));
    CHECK(s.a.finished());
}

NNH_TEST(a_one_frame_once_clip_finishes_after_its_duration) {
    Setup s;
    s.a.play(clip(s.lib, "c", 1, 3, SpriteMode::Once));
    s.a.update();
    s.a.update();
    CHECK(s.a.playing());
    const nnh::SpriteNotices n = s.a.update();
    CHECK(s.a.finished() && anim_test::count_kind(n, nnh::SpriteNoticeKind::Finished) == 1);
}

NNH_TEST(a_one_frame_loop_reports_a_loop_per_duration) {
    Setup s;
    s.a.play(clip(s.lib, "c", 1, 2));
    int loops = 0;
    for (int i = 0; i < 10; ++i) loops += anim_test::count_kind(s.a.update(), nnh::SpriteNoticeKind::Looped);
    CHECK(loops == 5 && s.a.loops() == 5 && s.a.position() == 0);
}

NNH_TEST(two_frame_ping_pong_alternates) {
    Setup s;
    s.a.play(clip(s.lib, "c", 2, 1, SpriteMode::PingPong));
    int got[5];
    positions(s.a, got);
    CHECK(same(got, {1, 0, 1, 0, 1}));
}

NNH_TEST(a_finished_clip_restarts_when_played_again) {
    Setup s;
    const nnh::SpriteClipId once = clip(s.lib, "c", 2, 1, SpriteMode::Once);
    s.a.play(once);
    for (int i = 0; i < 3; ++i) s.a.update();
    CHECK(s.a.finished());
    s.a.play(once);
    CHECK(s.a.playing() && s.a.position() == 0);
}
