// Input during transitions (spec 023 FR-12): blocked from the first transition frame's input until the frame after
// done; a control held through it stays blocked until released; block_input = false lets input through.

#include "nnh/events.hpp"
#include "nnh/input.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Button;
using nnh::Frames;

namespace {

constexpr std::uint32_t kA = static_cast<std::uint32_t>(nnh::Button::A);
constexpr std::uint32_t kB = static_cast<std::uint32_t>(nnh::Button::B);

/// Counts what the scenes could see: presses by query and by event, touches.
struct Seen {
    int queries = 0, events = 0;
    nnh::Subscription sub;
    Seen() {
        sub = nnh::events::subscribe<nnh::ButtonPressed>([this](const nnh::ButtonPressed&) { ++events; });
    }
    void look() {
        if (nnh::input::held_mask() != 0 || nnh::input::touching() || nnh::input::circle_pad().x != 0) ++queries;
    }
};

} // namespace

NNH_TEST(input_reads_idle_for_the_whole_transition) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    fake::set_held_buttons(kA);
    step(); // A pressed: the scene starts the transition this frame
    CHECK(tr::replace(b, {.out = Frames{3}, .hold = Frames{2}, .in = Frames{3}}));
    Seen seen;
    fake::set_circle_pad(0.9f, 0);
    fake::set_touch(5, 5);
    int frames = 0;
    while (tr::running()) {
        fake::set_held_buttons(frames % 2 == 0 ? kA : kA | kB); // B tapped repeatedly during the transition
        CHECK(nnh::frame());
        seen.look();
        nnh::scenes::update_and_draw();
        ++frames;
    }
    CHECK(frames == 8 && seen.queries == 0 && seen.events == 0);
    nnh::shutdown();
}

NNH_TEST(controls_held_through_a_transition_wait_for_their_release) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    step();
    CHECK(tr::replace(b, {.out = Frames{1}, .in = Frames{1}}));
    fake::set_held_buttons(kA);
    fake::set_circle_pad(0, -1);
    run_until_done();
    Seen seen;
    step(3);
    seen.look();
    CHECK(seen.queries == 0 && !nnh::input::pressed(nnh::Button::A));
    fake::set_held_buttons(0);
    fake::set_circle_pad(0, 0);
    step();
    fake::set_held_buttons(kA);
    step();
    CHECK(nnh::input::pressed(nnh::Button::A) && seen.events == 1);
    nnh::shutdown();
}

NNH_TEST(block_input_false_lets_input_through) {
    restart();
    Probe a, b;
    nnh::scenes::push(a);
    step();
    CHECK(tr::replace(b, {.out = Frames{4}, .update_out = true, .block_input = false}));
    fake::set_held_buttons(kB);
    step();
    CHECK(nnh::input::pressed(nnh::Button::B) && !nnh::input::blocked());
    nnh::shutdown();
}

NNH_TEST(the_game_block_survives_a_transition) {
    restart();
    nnh::input::set_blocked(true);
    CHECK(tr::cover({.out = Frames{1}, .in = Frames{1}}));
    run_until_done();
    CHECK(nnh::input::blocked());
    nnh::input::set_blocked(false);
    nnh::shutdown();
}
