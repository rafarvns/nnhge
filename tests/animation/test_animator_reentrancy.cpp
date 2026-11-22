// Re-entrancy (T4.3; FR-9): commands from a callback apply at once, drop the old clip's remaining notices and time,
// and the new clip's entry fires in the same update; chains stop at depth 8 with one logged error; other animators
// are free; update() from a callback is misuse.

#include "library_fixture.hpp"

using anim_test::clip;
using nnh::SpriteAnimator;
using nnh::SpriteClipId;
using nnh::SpriteNotice;
using nnh::SpriteNoticeKind;
using nnh::SpriteNotices;
using nnh::sprite_name;

namespace {

struct Setup {
    nnh::SpriteLibrary lib = anim_test::strip(8);
    SpriteAnimator a{lib};
    SpriteClipId walk{}, attack{};
};

} // namespace

NNH_TEST(a_callback_that_plays_another_clip_drops_the_rest_of_the_old_one) {
    Setup s;
    s.walk = clip(s.lib, "walk", 4, 1);
    s.attack = s.lib.add_clip({.name = "attack", .first = 4, .count = 2, .frame_time = nnh::Frames{1}}).value();
    CHECK(s.lib.add_marker(s.walk, 1, "go").ok());
    CHECK(s.lib.add_marker(s.walk, 1, "dropped").ok());
    CHECK(s.lib.add_marker(s.attack, 0, "swing").ok());
    s.a.on_notice([p = &s](SpriteAnimator& a, const SpriteNotice& n) {
        if (n.marker == sprite_name("go")) a.play(p->attack);
    });
    s.a.play(s.walk);
    const SpriteNotices n = s.a.advance(nnh::Frames{3}); // would cross into frames 1, 2, 3
    CHECK(n.count == 2);
    CHECK(n.data[0].marker == sprite_name("go") && n.data[1].marker == sprite_name("swing"));
    CHECK(s.a.clip() == s.attack && s.a.position() == 0 && s.a.time_in_frame().ticks == 0); // the time was dropped
}

NNH_TEST(a_callback_may_stop_or_queue_on_its_own_animator) {
    Setup s;
    s.walk = clip(s.lib, "walk", 4, 1);
    CHECK(s.lib.add_marker(s.walk, 2, "halt").ok());
    s.a.on_notice([](SpriteAnimator& a, const SpriteNotice&) { a.stop(); });
    s.a.play(s.walk);
    (void)s.a.advance(nnh::Frames{3});
    CHECK(!s.a.clip().valid());
}

NNH_TEST(a_chain_of_callbacks_stops_at_depth_eight_with_one_error) {
    anim_test::restart();
    Setup s;
    s.walk = clip(s.lib, "walk", 2, 1);
    s.attack = s.lib.add_clip({.name = "attack", .first = 4, .count = 2, .frame_time = nnh::Frames{1}}).value();
    CHECK(s.lib.add_marker(s.walk, 0, "ping").ok());
    CHECK(s.lib.add_marker(s.attack, 0, "pong").ok());
    static int calls = 0;
    calls = 0;
    s.a.on_notice([p = &s](SpriteAnimator& a, const SpriteNotice& n) {
        ++calls;
        a.play(n.marker == sprite_name("ping") ? p->attack : p->walk);
    });
    s.a.play(s.walk);
    const int lines = nnh::platform_fake::state().console_lines;
    const SpriteNotices n = s.a.update();
    CHECK(calls == nnh::kSpriteMaxCallbackDepth && n.count == nnh::kSpriteMaxCallbackDepth);
    CHECK(nnh::platform_fake::state().console_lines == lines + 1);
    CHECK(anim_test::last_log_mentions("callbacks changed the clip 8 times"));
    CHECK(s.a.update().count == nnh::kSpriteMaxCallbackDepth); // the last clip's entry fires next time
    nnh::shutdown();
}

NNH_TEST(a_callback_may_command_other_animators) {
    Setup s;
    SpriteAnimator other{s.lib};
    s.walk = clip(s.lib, "walk", 4, 1);
    s.attack = clip(s.lib, "attack", 2, 1);
    CHECK(s.lib.add_marker(s.walk, 1, "hit").ok());
    s.a.on_notice([o = &other, p = &s](SpriteAnimator&, const SpriteNotice&) { o->play(p->attack); });
    s.a.play(s.walk);
    (void)s.a.advance(nnh::Frames{2});
    CHECK(other.clip() == s.attack && s.a.clip() == s.walk && s.a.position() == 2);
}

NNH_TEST(updating_from_a_callback_asserts_or_is_ignored) {
    static Setup* g = nullptr;
    Setup s;
    g = &s;
    s.walk = clip(s.lib, "walk", 4, 1);
    CHECK(s.lib.add_marker(s.walk, 0, "m").ok());
    s.a.on_notice([](SpriteAnimator& a, const SpriteNotice&) { (void)a.update(); });
    s.a.play(s.walk);
    CHECK(anim_test::asserts_or_is_ignored([] { (void)g->a.update(); }));
    if (!nnh::build::kAssertsEnabled) CHECK(s.a.position() == 1);
}
