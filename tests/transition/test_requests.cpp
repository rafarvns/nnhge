// Requests, refusals, misuse and queries (spec 023 FR-1, FR-15, FR-19, FR-22; plan 023 "Requests").

#include "scene/scene_hook.hpp"
#include "transition/transition_internal.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Frames;
using nnh::Screen;
using tr::Phase;

namespace {

Probe* g_a = nullptr;
Probe* g_b = nullptr;

void push_a_already_on_the_stack() { (void)tr::push(*g_a); }
void replace_with_a_already_on_the_stack() { (void)tr::replace(*g_a); }
void pop_an_empty_stack() { (void)tr::pop(); }
void negative_out() { (void)tr::cover({.out = Frames{-3}}); }
void eleven_seconds_in() { (void)tr::cover({.in = nnh::Duration::from_seconds(11)}); }
void effect_same() { (void)tr::cover({.effect = tr::same()}); }
void custom_without_callable() { (void)tr::cover({.effect = tr::custom()}); }
void reveal_custom_without_callable() { (void)tr::cover({.reveal = tr::custom()}); }
void one_bar() { (void)tr::cover({.effect = tr::blinds(tr::Edge::Top, 1)}); }
void band_17() { (void)tr::cover({.effect = tr::iris({}, tr::IrisShape::Circle, nnh::colors::black, 17)}); }
void gap_300() { (void)tr::cover({.tall = true, .tall_gap = 300}); }
void nan_centre() { (void)tr::cover({.effect = tr::iris({{0.0f / 0.0f, 3}})}); }
void tall_with_a_bottom_effect() { (void)tr::cover({.bottom = tr::none(), .tall = true}); }

/// Clears a transition left running by a Release-path misuse.
void settle() {
    run_until_done();
    CHECK(!tr::running());
}

} // namespace

NNH_TEST(each_request_starts_a_transition) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    step();
    CHECK(tr::replace(b, {.out = Frames{2}, .in = Frames{2}}));
    CHECK(tr::running() && tr::phase() == Phase::Out);
    CHECK(run_until_done() == 5 && nnh::scenes::top() == &b);
    CHECK(tr::push(a, {.out = Frames{1}, .in = Frames{1}}));
    CHECK(run_until_done() == 3 && nnh::scenes::top() == &a && nnh::scenes::size() == 2);
    CHECK(tr::pop({.out = Frames{1}, .in = Frames{1}}));
    CHECK(run_until_done() == 3 && nnh::scenes::top() == &b && nnh::scenes::size() == 1);
    CHECK(tr::cover({.out = Frames{1}, .in = Frames{1}}));
    CHECK(run_until_done() == 3 && nnh::scenes::top() == &b);
    nnh::shutdown();
}

NNH_TEST(plain_stack_changes_stay_instant) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    step();
    nnh::scenes::replace(b);
    step();
    CHECK(nnh::scenes::top() == &b && !tr::running());
    nnh::shutdown();
}

NNH_TEST(a_second_request_is_refused_and_logged_once) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    step();
    LogCount refused("request refused");
    CHECK(tr::replace(b));
    CHECK(!tr::push(b));
    CHECK(!tr::cover());
    CHECK(!tr::pop());
    CHECK(refused.count == (nnh::build::flavor() == nnh::build::Flavor::Release ? 0 : 1));
    run_until_done();
    CHECK(tr::cover({.out = Frames{1}, .in = Frames{1}}));
    CHECK(!tr::cover()); // a new transition logs its own refusal
    CHECK(refused.count == (nnh::build::flavor() == nnh::build::Flavor::Release ? 0 : 2));
    settle();
    nnh::shutdown();
}

NNH_TEST(a_scene_already_on_the_stack_is_refused) {
    restart();
    Probe a;
    g_a = &a;
    nnh::scenes::push(a);
    step();
    CHECK(asserts_or_is_ignored(push_a_already_on_the_stack));
    CHECK(asserts_or_is_ignored(replace_with_a_already_on_the_stack));
    CHECK(!tr::running());
    if (!nnh::build::kAssertsEnabled) CHECK(last_log_mentions("already on the stack"));
    nnh::shutdown();
}

NNH_TEST(pop_with_an_empty_stack_is_refused) {
    restart();
    CHECK(asserts_or_is_ignored(pop_an_empty_stack));
    CHECK(!tr::running());
    nnh::shutdown();
}

NNH_TEST(bad_durations_assert_or_are_clamped) {
    restart();
    CHECK(asserts_or_is_ignored(negative_out));
    if (!nnh::build::kAssertsEnabled) {
        CHECK(tr::running() && tr::timing().out.count == 0 && last_log_mentions("invalid"));
        settle();
    }
    CHECK(asserts_or_is_ignored(eleven_seconds_in));
    if (!nnh::build::kAssertsEnabled) {
        CHECK(tr::timing().in.count == 600 && last_log_mentions("over 10 s"));
        nnh::detail::transition_cancel();
    }
    nnh::shutdown();
}

NNH_TEST(bad_effects_assert_or_fall_back_to_a_fade) {
    void (*cases[])() = {effect_same, custom_without_callable, reveal_custom_without_callable};
    for (auto fn : cases) {
        restart();
        CHECK(asserts_or_is_ignored(fn));
        if (!nnh::build::kAssertsEnabled) {
            CHECK(tr::running());
            nnh::Rect r[1];
            CHECK(tr::effect_rects(Screen::Top, tr::fade(), 1.0f, r, 1) == 1);
            settle();
        }
        nnh::shutdown();
    }
}

NNH_TEST(out_of_range_settings_assert_or_are_clamped) {
    void (*cases[])() = {one_bar, band_17, gap_300, nan_centre, tall_with_a_bottom_effect};
    for (auto fn : cases) {
        restart();
        CHECK(asserts_or_is_ignored(fn));
        if (!nnh::build::kAssertsEnabled) settle();
        nnh::shutdown();
    }
}

namespace {

/// What a scene's draw() reads, frame by frame.
struct Read {
    bool running;
    Phase phase;
    int frames;
    float top, bottom, progress, loading;
};
Read g_reads[8];
int g_read_count = 0;

class Reader : public Probe {
    void draw() override {
        if (g_read_count < 8) {
            g_reads[g_read_count++] = {tr::running(),           tr::phase(),           tr::frames(),
                                       tr::coverage(Screen::Top), tr::coverage(Screen::Bottom), tr::phase_progress(),
                                       tr::loading_progress()};
        }
    }
};

} // namespace

NNH_TEST(queries_follow_the_transition_frame_by_frame) {
    restart();
    Reader reader;
    nnh::scenes::push(reader);
    step();
    CHECK(!tr::running() && tr::phase() == Phase::None && tr::coverage(Screen::Top) == 0.0f && tr::frames() == 0);
    CHECK(tr::cover({.effect = tr::fade(), .bottom = tr::none(), .out = Frames{2}, .hold = Frames{2},
                     .in = Frames{2}}));
    const tr::Timing t = tr::timing();
    CHECK(t.out.count == 2 && t.hold.count == 2 && t.in.count == 2);
    CHECK(tr::seconds(t.out) == 2.0f / 60.0f && tr::seconds(Frames{30}) == 0.5f);
    g_read_count = 0;
    step(6); // the scene reads the queries in its draw, after the frame's step
    const Phase phases[] = {Phase::Out, Phase::Out, Phase::Covered, Phase::Covered, Phase::In, Phase::In};
    const float cov[] = {0.5f, 1.0f, 1.0f, 1.0f, 0.5f, 0.0f};
    const float prog[] = {0.5f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f};
    CHECK(g_read_count == 6);
    for (int f = 0; f < 6; ++f) {
        const Read& r = g_reads[f];
        CHECK(r.running && r.phase == phases[f] && r.frames == f + 1);
        CHECK(r.top == cov[f] && r.bottom == 0.0f && r.progress == prog[f] && r.loading == 1.0f);
    }
    CHECK(!tr::running() && tr::phase() == Phase::None && tr::coverage(Screen::Top) == 0.0f);
    CHECK(tr::timing().in.count == 2); // the last transition's
    nnh::shutdown();
}

NNH_TEST(reveal_at_moves_the_in_iris_before_the_in_phase_only) {
    restart();
    CHECK(!tr::reveal_at({{10, 10}}));
    CHECK(tr::cover({.effect = tr::iris({{20, 30}}), .out = Frames{1}, .in = Frames{4}}));
    CHECK(tr::reveal_at({{300, 100}}));
    CHECK(nnh::detail::g_transition.fx[nnh::detail::kTopIn].cx == 300);
    CHECK(nnh::detail::g_transition.fx[nnh::detail::kTopOut].cx == 20);
    step(3); // out, covered, first in frame
    CHECK(tr::phase() == Phase::In && !tr::reveal_at({{5, 5}}));
    settle();
    nnh::shutdown();
}

NNH_TEST(the_waiting_scene_is_counted_as_pending) {
    restart();
    Probe a, b;
    g_b = &b;
    nnh::scenes::push(a);
    step();
    CHECK(tr::replace(b, {.out = Frames{3}}));
    CHECK(nnh::detail::scene_pending_count(b) == 1);
    step(3); // the change is enqueued at the end of the last out frame: the stack's own count now
    CHECK(nnh::detail::scene_pending_count(b) == 1);
    step();
    CHECK(nnh::detail::scene_pending_count(b) == 0 && nnh::scenes::top() == &b);
    settle();
    nnh::shutdown();
}
