// Sets in the frame loop (spec 017 T3.2, FR-1, FR-15): each set registers its hook at creation and removes it at
// destruction; several sets run in creation order; moving a set keeps it registered.

#include "actions_fixture.hpp"
#include "core/input_hooks.hpp"
#include "nnh/events.hpp"

#include <utility>

using namespace actions_test;
using nnh::ActionSet;

namespace {
enum class Act { Jump };

ActionSet make(Button b) {
    ActionSet set{{.actions = 1}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(b)}}));
    return set;
}
} // namespace

NNH_TEST(a_set_is_evaluated_by_every_frame) {
    restart();
    ActionSet set = make(Button::A);
    frame();
    frame({.buttons = bit(Button::A)});
    CHECK(set.pressed(Act::Jump));
    frame({.buttons = bit(Button::A)});
    CHECK(set.held(Act::Jump) && set.held_frames(Act::Jump) == 2);
    nnh::shutdown();
}

NNH_TEST(queries_in_the_frame_a_set_was_created_read_idle) {
    restart();
    ActionSet set = make(Button::A);
    CHECK(!set.held(Act::Jump) && set.frames_since_pressed(Act::Jump) == UINT32_MAX);
    nnh::shutdown();
}

NNH_TEST(sets_run_in_creation_order_and_publish_in_that_order) {
    restart();
    ActionSet first = make(Button::A);
    ActionSet second = make(Button::A);
    std::uint16_t order[2] = {};
    int n = 0;
    nnh::Subscription sub = nnh::events::subscribe<nnh::ActionPressed>([&](const nnh::ActionPressed& e) {
        if (n < 2) order[n] = e.set;
        ++n;
    });
    frame();
    frame({.buttons = bit(Button::A)});
    CHECK(n == 2 && order[0] == first.id() && order[1] == second.id());
    nnh::shutdown();
}

NNH_TEST(destroying_a_set_removes_it_from_the_frame_and_keeps_the_others) {
    restart();
    ActionSet keep = make(Button::B);
    {
        ActionSet gone = make(Button::A);
        frame();
    }
    frame({.buttons = bit(Button::B)});
    CHECK(keep.pressed(Act::Jump));
    keep = ActionSet{};
    CHECK(nnh::detail::g_input_hooks == nullptr && nnh::detail::g_run_input_hooks == nullptr);
    frame();
    nnh::shutdown();
}

NNH_TEST(a_moved_set_is_still_evaluated) {
    restart();
    ActionSet a = make(Button::A);
    frame();
    ActionSet b = std::move(a);
    frame({.buttons = bit(Button::A)});
    CHECK(b.pressed(Act::Jump));
    nnh::shutdown();
}
