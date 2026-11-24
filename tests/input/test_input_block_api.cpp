// The public input block through nnh::frame() (spec 023 FR-12, FR-23; plan 023 "Input is blocked at its source"):
// queries idle, no input or action events while blocked, held controls blocked until released, remote input
// filtered, engine controls (the overlay toggle) never blocked, the game's and a transition's blocks independent.

#include "input/input_block.hpp"
#include "input/input_internal.hpp"
#include "../remote/remote_fixture.hpp"
#include "nnh/actions.hpp"
#include "nnh/app.hpp"
#include "nnh/debug.hpp"
#include "nnh/events.hpp"
#include "nnh/input.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::Button;

namespace {

constexpr std::uint32_t bit(Button b) { return static_cast<std::uint32_t>(b); }

void restart() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init());
}

/// Counts every input event while alive.
struct Seen {
    int pressed = 0, released = 0, touch_started = 0, touch_ended = 0;
    nnh::Subscription a, b, c, d;
    Seen() {
        a = nnh::events::subscribe<nnh::ButtonPressed>([this](const nnh::ButtonPressed&) { ++pressed; });
        b = nnh::events::subscribe<nnh::ButtonReleased>([this](const nnh::ButtonReleased&) { ++released; });
        c = nnh::events::subscribe<nnh::TouchStarted>([this](const nnh::TouchStarted&) { ++touch_started; });
        d = nnh::events::subscribe<nnh::TouchEnded>([this](const nnh::TouchEnded&) { ++touch_ended; });
    }
    int total() const { return pressed + released + touch_started + touch_ended; }
};

bool queries_idle() {
    return nnh::input::held_mask() == 0 && !nnh::input::pressed(Button::A) && !nnh::input::released(Button::A) &&
           nnh::input::circle_pad().x == 0 && nnh::input::circle_pad().y == 0 && !nnh::input::touching() &&
           !nnh::input::touch_started() && !nnh::input::touch_ended();
}

} // namespace

NNH_TEST(set_blocked_makes_queries_read_idle_from_the_next_frame) {
    restart();
    fake::set_held_buttons(bit(Button::A));
    fake::set_circle_pad(0.9f, 0);
    fake::set_touch(100, 50);
    CHECK(nnh::frame());
    CHECK(nnh::input::held(Button::A) && nnh::input::touching());
    nnh::input::set_blocked(true);
    CHECK(nnh::input::blocked());
    CHECK(nnh::input::held(Button::A)); // this frame's sample is already read
    CHECK(nnh::frame());
    CHECK(queries_idle());
    nnh::shutdown();
}

NNH_TEST(no_input_event_is_queued_while_blocked) {
    restart();
    fake::set_held_buttons(bit(Button::A));
    fake::set_touch(10, 10);
    CHECK(nnh::frame());
    Seen seen;
    nnh::input::set_blocked(true);
    CHECK(nnh::frame()); // A and the touch were held: no release, no touch end
    fake::set_held_buttons(bit(Button::B));
    fake::release_touch();
    CHECK(nnh::frame());
    fake::set_touch(20, 20);
    CHECK(nnh::frame());
    CHECK(seen.total() == 0);
    nnh::shutdown();
}

NNH_TEST(held_controls_reach_the_game_only_after_release_and_press) {
    restart();
    Seen seen;
    nnh::input::set_blocked(true);
    fake::set_held_buttons(bit(Button::A));
    fake::set_touch(30, 40);
    CHECK(nnh::frame());
    nnh::input::set_blocked(false);
    CHECK(!nnh::input::blocked());
    CHECK(nnh::frame()); // still held: filtered
    CHECK(queries_idle() && seen.total() == 0);
    fake::set_held_buttons(0);
    fake::release_touch();
    CHECK(nnh::frame());
    CHECK(seen.total() == 0);
    fake::set_held_buttons(bit(Button::A));
    fake::set_touch(30, 40);
    CHECK(nnh::frame());
    CHECK(nnh::input::pressed(Button::A) && nnh::input::touch_started());
    CHECK(seen.pressed == 1 && seen.touch_started == 1);
    nnh::shutdown();
}

NNH_TEST(the_filter_leaves_the_input_path_once_nothing_is_held) {
    restart();
    nnh::input::set_blocked(true);
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    nnh::input::set_blocked(false);
    CHECK(nnh::frame());
    CHECK(nnh::detail::g_input_filter != nullptr); // A still filtered
    fake::set_held_buttons(0);
    CHECK(nnh::frame());
    CHECK(nnh::detail::g_input_filter == nullptr); // back to the plain path: one null check per frame
    nnh::shutdown();
}

NNH_TEST(touch_position_keeps_the_last_unfiltered_point) {
    restart();
    fake::set_touch(70, 80);
    CHECK(nnh::frame());
    fake::release_touch();
    CHECK(nnh::frame());
    nnh::input::set_blocked(true);
    fake::set_touch(5, 6);
    CHECK(nnh::frame());
    CHECK(nnh::input::touch_position().x == 70 && nnh::input::touch_position().y == 80);
    nnh::shutdown();
}

NNH_TEST(the_game_block_never_lifts_a_transitions_block) {
    restart();
    nnh::detail::input_block(nnh::detail::InputBlocker::Transition, true);
    nnh::input::set_blocked(true);
    nnh::input::set_blocked(false);
    CHECK(nnh::input::blocked());
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    CHECK(queries_idle());
    nnh::detail::input_block(nnh::detail::InputBlocker::Transition, false);
    CHECK(!nnh::input::blocked());
    nnh::shutdown();
}

NNH_TEST(the_block_is_lifted_by_shutdown_and_init) {
    restart();
    nnh::input::set_blocked(true);
    restart();
    CHECK(!nnh::input::blocked());
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    CHECK(nnh::input::pressed(Button::A));
    nnh::shutdown();
}

NNH_TEST(the_overlay_toggle_works_while_blocked) {
    if (!nnh::build::kDevToolsEnabled) return;
    restart();
    nnh::debug::set_overlay_visible(false);
    nnh::input::set_blocked(true);
    fake::set_held_buttons(bit(Button::Select));
    CHECK(nnh::frame());
    fake::set_held_buttons(bit(Button::Select) | bit(Button::L));
    CHECK(nnh::frame());
    CHECK(nnh::debug::overlay_visible());
    CHECK(queries_idle());
    nnh::debug::set_overlay_visible(false);
    nnh::shutdown();
}

NNH_TEST(remote_input_is_blocked_like_real_input) {
    if (!nnh::build::kDevToolsEnabled) return;
    nnh::shutdown();
    fake::reset();
    fake::remote_connect();
    CHECK(nnh::init());
    remote_test::send(remote_test::hello());
    remote_test::send(remote_test::buttons(2, 3, bit(Button::A)));
    remote_test::send(remote_test::touch(3, 3, true, 50, 60));
    nnh::input::set_blocked(true);
    Seen seen;
    for (int f = 1; f <= 5; ++f) {
        CHECK(nnh::frame());
        CHECK(queries_idle());
    }
    CHECK(seen.total() == 0);
    nnh::shutdown();
}

NNH_TEST(actions_report_nothing_while_blocked_and_held_bindings_wait_for_release) {
    restart();
    nnh::ActionSet actions{{.actions = 1}};
    const nnh::ActionId jump{0};
    CHECK(actions.add(jump, {.name = "jump", .bindings = {{nnh::Source::button(Button::A)}}}));
    int presses = 0;
    nnh::Subscription sub = nnh::events::subscribe<nnh::ActionPressed>([&presses](const nnh::ActionPressed&) {
        ++presses;
    });
    nnh::input::set_blocked(true);
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    CHECK(!actions.pressed(jump) && !actions.held(jump));
    nnh::input::set_blocked(false);
    CHECK(nnh::frame());
    CHECK(!actions.pressed(jump) && !actions.held(jump));
    fake::set_held_buttons(0);
    CHECK(nnh::frame());
    fake::set_held_buttons(bit(Button::A));
    CHECK(nnh::frame());
    CHECK(actions.pressed(jump));
    CHECK(presses == 1);
    sub.reset();
    actions = nnh::ActionSet{};
    nnh::shutdown();
}
