// Notices (T4.2; FR-8): markers once per entered frame, in order; entries by play() fire at the next update; loops,
// ping-pong turns, finishes; the callback sees exactly the returned view; nothing while paused.

#include "library_fixture.hpp"

using anim_test::clip;
using nnh::SpriteAnimator;
using nnh::SpriteNotice;
using nnh::SpriteNoticeKind;
using nnh::SpriteNotices;
using nnh::sprite_name;

namespace {

struct Setup {
    nnh::SpriteLibrary lib = anim_test::strip(4);
    SpriteAnimator a{lib};
};

struct Log {
    SpriteNotice items[64];
    int count = 0;
};

} // namespace

NNH_TEST(play_never_notifies_its_entry_fires_at_the_next_update) {
    Setup s;
    const nnh::SpriteClipId c = clip(s.lib, "c", 4, 6);
    CHECK(s.lib.add_marker(c, 0, "start").ok());
    Log log;
    s.a.on_notice([l = &log](SpriteAnimator&, const SpriteNotice& n) { l->items[l->count++] = n; });
    s.a.play(c);
    CHECK(log.count == 0);
    const SpriteNotices n = s.a.update();
    CHECK(n.count == 1 && n.data[0].marker == sprite_name("start") && log.count == 1);
    CHECK(s.a.update().count == 0); // still on frame 0: entered once
}

NNH_TEST(markers_fire_on_every_entry_including_loops_and_ping_pong_turns) {
    Setup s;
    const nnh::SpriteClipId c = clip(s.lib, "c", 3, 1, nnh::SpriteMode::PingPong); // 0 1 2 1 0 1 …
    CHECK(s.lib.add_marker(c, 1, "mid").ok());
    s.a.play(c);
    int mids = 0, loops = 0;
    for (int i = 0; i < 8; ++i) { // shows 1 2 1 0 1 2 1 0
        const SpriteNotices n = s.a.update();
        mids += anim_test::count_kind(n, SpriteNoticeKind::Marker);
        loops += anim_test::count_kind(n, SpriteNoticeKind::Looped);
    }
    CHECK(mids == 4 && loops == 2);
}

NNH_TEST(looped_comes_before_the_markers_of_the_frame_it_enters) {
    Setup s;
    const nnh::SpriteClipId c = clip(s.lib, "c", 2, 1);
    CHECK(s.lib.add_marker(c, 0, "first").ok());
    s.a.play(c);
    s.a.update();
    const SpriteNotices n = s.a.update();
    CHECK(n.count == 2 && n.data[0].kind == SpriteNoticeKind::Looped && n.data[1].kind == SpriteNoticeKind::Marker);
    CHECK(n.data[0].clip == c);
}

NNH_TEST(a_finished_clip_notifies_once) {
    Setup s;
    s.a.play(clip(s.lib, "c", 2, 1, nnh::SpriteMode::Once));
    int finished = 0;
    for (int i = 0; i < 10; ++i) finished += anim_test::count_kind(s.a.update(), SpriteNoticeKind::Finished);
    CHECK(finished == 1);
}

NNH_TEST(the_callback_sees_the_returned_view_in_order) {
    Setup s;
    const nnh::SpriteClipId c = clip(s.lib, "c", 4, 1, nnh::SpriteMode::Repeat, 2);
    for (int p = 0; p < 4; ++p) CHECK(s.lib.add_marker(c, p, "m", p).ok());
    Log log;
    s.a.on_notice([l = &log](SpriteAnimator&, const SpriteNotice& n) { l->items[l->count++] = n; });
    s.a.play(c);
    int seen = 0;
    bool same = true;
    for (int i = 0; i < 6; ++i) {
        for (const SpriteNotice& n : s.a.advance(nnh::Frames{2})) {
            const SpriteNotice& l = log.items[seen++];
            same = same && l.kind == n.kind && l.marker == n.marker && l.value == n.value && l.position == n.position;
        }
    }
    CHECK(same && seen == log.count && seen == 8 + 1 + 1); // 8 entries, 1 loop, 1 finish
}

NNH_TEST(a_paused_animator_fires_nothing) {
    Setup s;
    const nnh::SpriteClipId c = clip(s.lib, "c", 2, 1);
    CHECK(s.lib.add_marker(c, 0, "m").ok());
    s.a.play(c);
    s.a.pause();
    CHECK(s.a.update().count == 0);
    s.a.resume();
    CHECK(s.a.update().count == 1);
}

NNH_TEST(set_frame_fires_the_entered_frame_at_the_next_update) {
    Setup s;
    const nnh::SpriteClipId c = clip(s.lib, "c", 4, 6);
    CHECK(s.lib.add_marker(c, 2, "two").ok());
    s.a.play(c);
    s.a.update();
    s.a.set_frame(2);
    const SpriteNotices n = s.a.update();
    CHECK(n.count == 1 && n.data[0].position == 2);
}

NNH_TEST(the_view_lasts_until_the_next_update_of_any_animator) {
    Setup s;
    SpriteAnimator b{s.lib};
    const nnh::SpriteClipId c = clip(s.lib, "c", 2, 1);
    CHECK(s.lib.add_marker(c, 0, "m").ok());
    s.a.play(c);
    b.play(c);
    const SpriteNotices first = s.a.update();
    CHECK(first.count == 1);
    const SpriteNotices second = b.update();
    CHECK(second.data == first.data); // one module-wide buffer
}

NNH_TEST(more_than_32_notices_in_one_update_assert_or_are_cut) {
    static Setup* g = nullptr;
    Setup s;
    g = &s;
    const nnh::SpriteClipId c = clip(s.lib, "c", 4, 1);
    for (int p = 0; p < 4; ++p) {
        for (int k = 0; k < 8; ++k) CHECK(s.lib.add_marker(c, p, "m", k).ok());
    }
    s.a.play(c);
    CHECK(anim_test::asserts_or_is_ignored([] { (void)g->a.advance(nnh::Frames{4}); })); // 8 × 5 entries + 1 loop
    if (!nnh::build::kAssertsEnabled) CHECK(s.a.position() == 0 && s.a.loops() == 1);
}
