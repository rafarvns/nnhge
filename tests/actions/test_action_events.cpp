// Action events (spec 017 T3.3, FR-14, FR-15): published in the same frame as the input, after its button and
// touch events; per set presses then releases, each in declaration order; analog actions on held changes; none
// while disabled or blocked, or with events off; queries agree inside handlers.

#include "actions_fixture.hpp"
#include "nnh/events.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::Source;

namespace {
enum class Act { Jump, Fire, Move, Tap };
enum class Ctx { Play };

/// What was delivered in one frame, in order: 'b' ButtonPressed, 't' TouchStarted, 'P' + action / 'R' + action.
struct Log {
    char text[64] = {};
    int n = 0;
    void add(char c) {
        if (n < 63) text[n++] = c;
    }
    void clear() { *this = Log{}; }
};

Log g_log;
ActionSet* g_set = nullptr;
bool g_agreed = true;

char letter(std::uint16_t action) { return static_cast<char>('0' + action); }

struct Listener {
    nnh::Subscription b = nnh::events::subscribe<nnh::ButtonPressed>([](const nnh::ButtonPressed&) { g_log.add('b'); });
    nnh::Subscription t = nnh::events::subscribe<nnh::TouchStarted>([](const nnh::TouchStarted&) { g_log.add('t'); });
    nnh::Subscription p = nnh::events::subscribe<nnh::ActionPressed>([](const nnh::ActionPressed& e) {
        g_log.add('P');
        g_log.add(letter(e.action));
        g_agreed = g_agreed && g_set->pressed(nnh::ActionId(e.action)) && g_set->held(nnh::ActionId(e.action));
    });
    nnh::Subscription r = nnh::events::subscribe<nnh::ActionReleased>([](const nnh::ActionReleased& e) {
        g_log.add('R');
        g_log.add(letter(e.action));
        g_agreed = g_agreed && g_set->released(nnh::ActionId(e.action)) && !g_set->held(nnh::ActionId(e.action));
    });
};

/// Declared in the order Fire, Jump, Move, Tap (not id order).
ActionSet make() {
    ActionSet set{{.actions = 4}};
    CHECK(set.add(Act::Fire, {.name = "fire", .bindings = {btn(Button::B)}}));
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(nnh::AnalogStick::CirclePad)}}}));
    CHECK(set.add(Act::Tap, {.name = "tap", .bindings = {{Source::touch(0, 0, 50, 50)}}}));
    return set;
}

bool logged(const char* expected) { return std::strcmp(g_log.text, expected) == 0; }
} // namespace

NNH_TEST(action_events_follow_the_frames_button_and_touch_events) {
    restart();
    ActionSet set = make();
    g_set = &set;
    Listener listener;
    frame();
    g_log.clear();
    frame({.buttons = bit(Button::A), .touch = true, .tx = 10, .ty = 10});
    CHECK(logged("btP0P3"));
    nnh::shutdown();
}

NNH_TEST(presses_come_before_releases_each_in_declaration_order) {
    restart();
    ActionSet set = make();
    g_set = &set;
    Listener listener;
    frame();
    frame({.buttons = bit(Button::A)});
    g_log.clear();
    frame({.buttons = bit(Button::B)}); // Jump released, Fire pressed
    CHECK(logged("bP1R0"));
    g_log.clear();
    frame({.buttons = bit(Button::A) | bit(Button::B)});
    frame();
    CHECK(logged("bP0R1R0"));
    nnh::shutdown();
}

NNH_TEST(queries_agree_with_events_inside_handlers) {
    restart();
    ActionSet set = make();
    g_set = &set;
    g_agreed = true;
    Listener listener;
    frame();
    frame({.buttons = bit(Button::A) | bit(Button::B)});
    frame();
    CHECK(g_agreed);
    nnh::shutdown();
}

NNH_TEST(analog_actions_publish_when_held_changes) {
    restart();
    ActionSet set = make();
    g_set = &set;
    Listener listener;
    frame();
    g_log.clear();
    frame({.cx = 0.3f});
    frame({.cx = 1.0f});
    frame({.cx = 0.9f});
    CHECK(logged("P2"));
    g_log.clear();
    frame();
    CHECK(logged("R2"));
    nnh::shutdown();
}

NNH_TEST(no_action_events_while_disabled_or_blocked) {
    restart();
    ActionSet set = make();
    g_set = &set;
    Listener listener;
    frame({.buttons = bit(Button::A)}); // held at the first evaluation: blocked
    frame({.buttons = bit(Button::A)});
    frame();
    set.enable(Ctx::Play, false);
    frame();
    g_log.clear();
    frame({.buttons = bit(Button::B)});
    frame();
    CHECK(logged("b"));
    nnh::shutdown();
}

NNH_TEST(a_set_with_events_off_publishes_nothing_but_still_answers_queries) {
    restart();
    ActionSet set = make();
    g_set = &set;
    set.set_events_enabled(false);
    Listener listener;
    frame();
    g_log.clear();
    frame({.buttons = bit(Button::A)});
    CHECK(logged("b") && set.pressed(Act::Jump));
    nnh::shutdown();
}

NNH_TEST(an_action_event_tells_its_set_and_action) {
    restart();
    ActionSet set = make();
    ActionSet other{{.actions = 1}};
    const nnh::ActionPressed e{set.id(), static_cast<std::uint16_t>(Act::Jump)};
    CHECK(e.is(set, Act::Jump) && !e.is(set, Act::Fire) && !e.is(other, Act::Jump));
    const nnh::ActionReleased r{set.id(), static_cast<std::uint16_t>(Act::Fire)};
    CHECK(r.is(set, Act::Fire) && !r.is(other, Act::Fire));
    static_assert(sizeof(nnh::ActionPressed) == 4 && std::is_trivially_copyable_v<nnh::ActionPressed>);
    nnh::shutdown();
}
