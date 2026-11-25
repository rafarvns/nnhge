// Animator commands (T3.1, T3.2; FR-6): play (same, restart), play_from, queue (once, looping, full, idle), stop,
// pause/resume, set_frame, flips, queries, and misuse.

#include "nnh/detail/target_layout.hpp"
#include <cstring>

#include "library_fixture.hpp"

using anim_test::clip;
using nnh::SpriteAnimator;
using nnh::SpriteClipId;
using nnh::SpriteMode;

namespace {

struct Setup {
    nnh::SpriteLibrary lib = anim_test::strip(8);
    SpriteAnimator a{lib};
};

void run(SpriteAnimator& a, int n) {
    for (int i = 0; i < n; ++i) a.update();
}

} // namespace

NNH_TEST(an_animator_is_small) {
    CHECK(sizeof(SpriteAnimator) <= (nnh::detail::kThreeDsLayout ? 64u : 96u));
}

NNH_TEST(a_default_animator_answers_no_clip) {
    SpriteAnimator a;
    CHECK(!a.playing() && !a.finished() && !a.paused() && !a.clip().valid());
    CHECK(a.position() == -1 && a.frame() == -1 && a.region().empty());
    CHECK(std::strcmp(a.clip_name(), "") == 0 && !a.state().valid());
    CHECK(a.update().count == 0 && a.progress() == 0.0f);
}

NNH_TEST(a_new_animator_has_no_clip_until_played) {
    Setup s;
    CHECK(!s.a.playing() && s.a.frame() == -1);
    s.a.play(clip(s.lib, "c", 4, 1));
    CHECK(s.a.playing() && s.a.frame() == 0 && s.a.loops() == 0);
}

NNH_TEST(playing_the_same_clip_again_does_not_restart_it_unless_asked) {
    Setup s;
    const SpriteClipId c = clip(s.lib, "c", 4, 1);
    s.a.play(c);
    run(s.a, 2);
    s.a.play(c);
    CHECK(s.a.position() == 2);
    s.a.play(c, true);
    CHECK(s.a.position() == 0 && s.a.time_in_frame().ticks == 0);
}

NNH_TEST(play_from_starts_at_a_frame) {
    Setup s;
    const SpriteClipId c = s.lib.add_clip({.name = "c", .first = 2, .count = 4, .frame_time = nnh::Frames{1}}).value();
    s.a.play_from(c, 2);
    CHECK(s.a.position() == 2 && s.a.frame() == 4);
    s.a.update();
    CHECK(s.a.position() == 3);
}

NNH_TEST(a_queued_clip_starts_when_a_once_clip_finishes_with_the_time_carried) {
    Setup s;
    const SpriteClipId once = clip(s.lib, "once", 2, 2, SpriteMode::Once);
    const SpriteClipId next = s.lib.add_clip({.name = "next", .first = 4, .count = 2, .frame_time = nnh::Frames{2}})
                                  .value();
    s.a.play(once);
    CHECK(s.a.queue(next).ok());
    (void)s.a.advance(nnh::Frames{5}); // 2 + 2 for "once", then 1 into "next"
    CHECK(s.a.clip() == next && s.a.position() == 0 && s.a.time_in_frame() == nnh::FrameTime{nnh::Frames{1}});
}

NNH_TEST(a_queued_clip_starts_at_the_end_of_a_loop) {
    Setup s;
    const SpriteClipId loop = clip(s.lib, "loop", 3, 1);
    const SpriteClipId next = clip(s.lib, "next", 2, 1);
    s.a.play(loop);
    run(s.a, 1);
    CHECK(s.a.queue(next).ok());
    run(s.a, 1);
    CHECK(s.a.clip() == loop && s.a.position() == 2);
    const nnh::SpriteNotices n = s.a.update(); // the loop wraps: "next" starts instead
    CHECK(s.a.clip() == next && s.a.position() == 0);
    CHECK(anim_test::count_kind(n, nnh::SpriteNoticeKind::Looped) == 1);
}

NNH_TEST(queued_clips_play_in_order_and_a_full_queue_is_an_error) {
    Setup s;
    const SpriteClipId a = clip(s.lib, "a", 1, 1, SpriteMode::Once);
    const SpriteClipId b = clip(s.lib, "b", 1, 1, SpriteMode::Once);
    s.a.play(a);
    for (int i = 0; i < 4; ++i) CHECK(s.a.queue(i % 2 == 0 ? b : a).ok());
    auto full = s.a.queue(b);
    CHECK(!full.ok() && full.error().code == nnh::ErrorCode::LimitReached);
    const SpriteClipId want[] = {b, a, b, a};
    for (const SpriteClipId w : want) {
        s.a.update();
        CHECK(s.a.clip() == w && s.a.playing());
    }
    s.a.update();
    CHECK(s.a.finished());
}

NNH_TEST(queueing_on_an_idle_or_finished_animator_plays_at_once) {
    Setup s;
    const SpriteClipId once = clip(s.lib, "once", 1, 1, SpriteMode::Once);
    const SpriteClipId other = clip(s.lib, "other", 2, 1);
    CHECK(s.a.queue(once).ok());
    CHECK(s.a.clip() == once && s.a.playing());
    s.a.update();
    CHECK(s.a.finished());
    CHECK(s.a.queue(other).ok());
    CHECK(s.a.clip() == other && s.a.playing());
}

NNH_TEST(play_clears_the_queue) {
    Setup s;
    const SpriteClipId once = clip(s.lib, "once", 1, 1, SpriteMode::Once);
    const SpriteClipId other = clip(s.lib, "other", 2, 1);
    s.a.play(once);
    CHECK(s.a.queue(other).ok());
    s.a.play(once, true);
    s.a.update();
    CHECK(s.a.clip() == once && s.a.finished());
    s.a.play(once, true);
    CHECK(s.a.queue(other).ok());
    s.a.clear_queue();
    s.a.update();
    CHECK(s.a.clip() == once);
}

NNH_TEST(stop_leaves_no_clip) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1));
    s.a.stop();
    CHECK(!s.a.playing() && !s.a.clip().valid() && s.a.frame() == -1);
    CHECK(s.a.update().count == 0);
}

NNH_TEST(pause_keeps_the_frame_and_the_time) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 2));
    s.a.update();
    s.a.pause();
    run(s.a, 10);
    CHECK(s.a.paused() && s.a.playing() && s.a.position() == 0 && s.a.time_in_frame() == nnh::FrameTime{nnh::Frames{1}});
    s.a.resume();
    s.a.update();
    CHECK(s.a.position() == 1);
}

NNH_TEST(set_frame_jumps_within_the_clip) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 2));
    s.a.update();
    s.a.set_frame(3);
    CHECK(s.a.position() == 3 && s.a.time_in_frame().ticks == 0);
    run(s.a, 2);
    CHECK(s.a.position() == 0 && s.a.loops() == 1);
}

NNH_TEST(set_frame_in_a_reverse_ping_pong_shows_that_frame) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1, SpriteMode::PingPong, 0, nnh::SpriteDirection::Reverse));
    s.a.set_frame(1);
    CHECK(s.a.position() == 1);
}

NNH_TEST(progress_covers_the_whole_finite_clip) {
    Setup s;
    s.a.play(clip(s.lib, "c", 2, 2, SpriteMode::Repeat, 2)); // 8 frames in all
    CHECK(s.a.progress() == 0.0f);
    run(s.a, 2);
    CHECK(s.a.progress() == 0.25f);
    run(s.a, 4);
    CHECK(s.a.progress() == 0.75f);
    run(s.a, 2);
    CHECK(s.a.finished() && s.a.progress() == 1.0f);
}

NNH_TEST(progress_of_a_loop_is_within_the_current_loop) {
    Setup s;
    s.a.play(clip(s.lib, "c", 4, 1));
    run(s.a, 5);
    CHECK(s.a.progress() == 0.25f && s.a.loops() == 1);
}

NNH_TEST(flips_are_state_the_game_sets) {
    Setup s;
    s.a.set_flip(true);
    CHECK(s.a.flip_x() && !s.a.flip_y());
    s.a.set_flip(false, true);
    CHECK(!s.a.flip_x() && s.a.flip_y());
}

namespace {
Setup* g_setup = nullptr;
}

NNH_TEST(commands_with_bad_ids_assert_or_are_ignored) {
    Setup s;
    g_setup = &s;
    s.a.play(clip(s.lib, "c", 4, 1));
    CHECK(anim_test::asserts_or_is_ignored([] { g_setup->a.play(SpriteClipId{9}); }));
    CHECK(anim_test::asserts_or_is_ignored([] { g_setup->a.play("missing"); }));
    CHECK(anim_test::asserts_or_is_ignored([] { g_setup->a.play_from(SpriteClipId{0}, 4); }));
    CHECK(anim_test::asserts_or_is_ignored([] { g_setup->a.set_frame(-1); }));
    CHECK(anim_test::asserts_or_is_ignored([] { (void)g_setup->a.queue(SpriteClipId{9}); }));
    CHECK(anim_test::asserts_or_is_ignored([] { g_setup->a.request(nnh::SpriteStateId{0}); }));
    CHECK(anim_test::asserts_or_is_ignored([] { g_setup->a.force(nnh::SpriteStateId{3}); }));
    CHECK(anim_test::asserts_or_is_ignored([] { SpriteAnimator none; none.play(SpriteClipId{0}); }));
    CHECK(s.a.clip().value == 0 && s.a.position() == 0);
}

NNH_TEST(release_logs_name_the_misuse) {
    if (nnh::build::kAssertsEnabled) return;
    anim_test::restart();
    Setup s;
    s.a.play(SpriteClipId{3});
    CHECK(anim_test::last_log_mentions("play: not a clip of the library"));
    s.a.set_speed(-2);
    CHECK(anim_test::last_log_mentions("outside 0..64"));
    nnh::shutdown();
}
