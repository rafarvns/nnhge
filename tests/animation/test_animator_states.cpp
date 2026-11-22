// The animator's state machine (T5.2; FR-12, FR-13): start state, requests through the rule table (now, at the end,
// never), keep-time, pending requests, next-on-finish, force, StateChanged order and the state queries.

#include "library_fixture.hpp"

using nnh::FrameTime;
using nnh::Frames;
using nnh::SpriteAnimator;
using nnh::SpriteClipId;
using nnh::SpriteMode;
using nnh::SpriteNoticeKind;
using nnh::SpriteNotices;
using nnh::SpriteStateId;
using nnh::SpriteSwitch;

namespace {

/// idle (1 frame), run (4 × 2, loop, `step` on 1 and 3), run_shoot (4 × 2), jump (2 × 1, once → fall), fall (1),
/// attack (3 × 1, once → idle; can't be cut short except by hurt), hurt (2 × 1, once → idle).
struct Hero {
    nnh::SpriteLibrary lib = anim_test::strip(8);
    SpriteStateId idle, run, run_shoot, jump, fall, attack, hurt;

    Hero() {
        auto add = [&](const char* name, int first, int count, int frames, SpriteMode mode) {
            const SpriteClipId c = lib.add_clip({.name = name, .first = first, .count = count,
                                                 .frame_time = Frames{frames}, .mode = mode})
                                       .value();
            return lib.add_state(name, c).value();
        };
        idle = add("idle", 0, 1, 1, SpriteMode::Loop);
        run = add("run", 0, 4, 2, SpriteMode::Loop);
        run_shoot = add("run_shoot", 4, 4, 2, SpriteMode::Loop);
        jump = add("jump", 2, 2, 1, SpriteMode::Once);
        fall = add("fall", 4, 1, 1, SpriteMode::Loop);
        attack = add("attack", 5, 3, 1, SpriteMode::Once);
        hurt = add("hurt", 6, 2, 1, SpriteMode::Once);
        CHECK(lib.add_marker(lib.find_clip("run"), 1, "step").ok());
        CHECK(lib.add_marker(lib.find_clip("run"), 3, "step").ok());
        CHECK(lib.set_next_on_finish(jump, fall).ok());
        CHECK(lib.set_next_on_finish(attack, idle).ok());
        CHECK(lib.set_next_on_finish(hurt, idle).ok());
        CHECK(lib.add_transition({.from = attack, .when = SpriteSwitch::Never}).ok());
        CHECK(lib.add_transition({.from = attack, .to = hurt}).ok());
        CHECK(lib.add_transition({.from = run, .to = run_shoot, .keep_time = true}).ok());
        CHECK(lib.add_transition({.from = run_shoot, .to = run, .keep_time = true}).ok());
        CHECK(lib.add_transition({.to = fall, .when = SpriteSwitch::AtEnd}).ok());
    }
};

int find(SpriteNotices n, SpriteNoticeKind kind) {
    for (int i = 0; i < n.count; ++i) {
        if (n.data[i].kind == kind) return i;
    }
    return -1;
}

} // namespace

NNH_TEST(an_animator_starts_in_its_state_without_a_state_change) {
    Hero h;
    SpriteAnimator a{h.lib, h.idle};
    CHECK(a.state() == h.idle && !a.previous_state().valid() && a.clip() == h.lib.find_clip("idle"));
    CHECK(find(a.update(), SpriteNoticeKind::StateChanged) < 0);
}

NNH_TEST(requesting_the_current_state_does_nothing) {
    Hero h;
    SpriteAnimator a{h.lib, h.run};
    for (int i = 0; i < 3; ++i) a.update();
    a.request(h.run);
    CHECK(a.position() == 1 && a.time_in_frame() == FrameTime{Frames{1}});
}

NNH_TEST(a_request_with_no_rule_switches_now_and_notifies_at_the_next_update) {
    Hero h;
    SpriteAnimator a{h.lib, h.idle};
    a.update();
    a.request(h.run);
    CHECK(a.state() == h.run && a.previous_state() == h.idle && a.position() == 0);
    const SpriteNotices n = a.update();
    CHECK(n.count == 1 && n.data[0].kind == SpriteNoticeKind::StateChanged);
    CHECK(n.data[0].state == h.run && n.data[0].previous == h.idle && n.data[0].clip == h.lib.find_clip("run"));
}

NNH_TEST(an_at_end_request_waits_for_the_end_of_the_loop_and_carries_the_time) {
    Hero h;
    SpriteAnimator a{h.lib, h.run};
    a.update();
    a.request(h.fall); // any → fall: at the end
    CHECK(a.state() == h.run);
    for (int i = 0; i < 6; ++i) a.update(); // 7 of run's 8 frames
    CHECK(a.state() == h.run);
    // The loop ends 1 frame in: fall, with 1 frame carried, which is fall's whole (looping) frame.
    const SpriteNotices n = a.advance(Frames{2});
    CHECK(a.state() == h.fall && a.time_in_frame() == FrameTime{} && a.time_in_state() == FrameTime{Frames{1}});
    const int looped = find(n, SpriteNoticeKind::Looped);
    const int changed = find(n, SpriteNoticeKind::StateChanged);
    CHECK(looped >= 0 && changed == looped + 1);
}

NNH_TEST(a_later_request_replaces_a_waiting_one_and_requesting_the_current_state_cancels_it) {
    Hero h;
    SpriteAnimator a{h.lib, h.run};
    a.request(h.fall);
    a.request(h.idle); // no rule: now (and the waiting fall is forgotten)
    CHECK(a.state() == h.idle);
    for (int i = 0; i < 4; ++i) a.update();
    CHECK(a.state() == h.idle);
    a.request(h.fall);
    a.request(h.idle); // cancels
    for (int i = 0; i < 4; ++i) a.update();
    CHECK(a.state() == h.idle);
}

NNH_TEST(never_ignores_requests_but_the_most_specific_rule_lets_hurt_through) {
    Hero h;
    SpriteAnimator a{h.lib, h.attack};
    a.request(h.idle);
    a.request(h.run);
    CHECK(a.state() == h.attack);
    a.request(h.hurt);
    CHECK(a.state() == h.hurt && a.previous_state() == h.attack);
}

NNH_TEST(a_finished_once_clip_moves_on_by_itself) {
    Hero h;
    SpriteAnimator a{h.lib, h.jump};
    a.update();
    const SpriteNotices n = a.update(); // jump's two frames end: fall
    CHECK(a.state() == h.fall && a.previous_state() == h.jump && a.playing());
    const int finished = find(n, SpriteNoticeKind::Finished);
    CHECK(finished >= 0 && find(n, SpriteNoticeKind::StateChanged) == finished + 1);
}

NNH_TEST(a_waiting_request_wins_over_next_on_finish) {
    Hero h;
    SpriteAnimator a{h.lib, h.attack};
    CHECK(h.lib.add_transition({.from = h.attack, .to = h.run, .when = SpriteSwitch::AtEnd}).ok());
    a.request(h.run);
    CHECK(a.state() == h.attack);
    for (int i = 0; i < 3; ++i) a.update();
    CHECK(a.state() == h.run);
}

NNH_TEST(keep_time_switches_on_the_same_frame_and_time) {
    Hero h;
    SpriteAnimator a{h.lib, h.run};
    for (int i = 0; i < 5; ++i) a.update(); // position 2, 1 frame in
    a.request(h.run_shoot);
    CHECK(a.state() == h.run_shoot && a.position() == 2 && a.frame() == 6);
    CHECK(a.time_in_frame() == FrameTime{Frames{1}});
    a.request(h.run);
    CHECK(a.position() == 2 && a.frame() == 2);
}

NNH_TEST(force_ignores_the_rules_and_clears_a_waiting_request) {
    Hero h;
    SpriteAnimator a{h.lib, h.attack};
    a.force(h.idle);
    CHECK(a.state() == h.idle);
    SpriteAnimator b{h.lib, h.run};
    b.request(h.fall);
    b.force(h.run); // the current state: only the waiting request goes
    for (int i = 0; i < 9; ++i) b.update();
    CHECK(b.state() == h.run);
}

NNH_TEST(time_in_state_counts_and_resets_on_each_change) {
    Hero h;
    SpriteAnimator a{h.lib, h.run};
    for (int i = 0; i < 10; ++i) a.update();
    CHECK(a.time_in_state() == FrameTime{Frames{10}});
    a.request(h.idle);
    CHECK(a.time_in_state() == FrameTime{});
    a.update();
    CHECK(a.time_in_state() == FrameTime{Frames{1}});
}

NNH_TEST(an_at_end_request_on_a_finished_clip_switches_now) {
    Hero h;
    CHECK(h.lib.set_next_on_finish(h.hurt, {}).ok());
    SpriteAnimator a{h.lib, h.hurt};
    for (int i = 0; i < 3; ++i) a.update();
    CHECK(a.finished() && a.state() == h.hurt);
    a.request(h.fall);
    CHECK(a.state() == h.fall && a.playing());
}

NNH_TEST(play_leaves_the_state_machine_and_request_then_acts_as_force) {
    Hero h;
    SpriteAnimator a{h.lib, h.attack};
    a.play(h.lib.find_clip("run"));
    CHECK(!a.state().valid() && a.time_in_state().ticks == 0);
    a.request(h.idle);
    CHECK(a.state() == h.idle);
}

NNH_TEST(the_example_drive_loop_picks_states_from_the_games_conditions) {
    Hero h;
    SpriteAnimator a{h.lib, h.idle};
    int steps = 0;
    a.on_notice([n = &steps](SpriteAnimator&, const nnh::SpriteNotice& x) {
        if (x.kind == SpriteNoticeKind::Marker && x.marker == nnh::sprite_name("step")) ++*n;
    });
    for (int frame = 0; frame < 32; ++frame) {
        const bool moving = frame >= 4 && frame < 20;
        a.request(moving ? h.run : h.idle);
        a.update();
    }
    CHECK(steps == 4 && a.state() == h.idle); // run for 16 frames: 2 loops, 2 steps each
}
