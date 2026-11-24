// AC-4: an ActionSet whose contexts are bound to the outgoing and incoming scenes. The confirm press that starts
// the transition, a button held through it and a tap during it produce no action, button or touch event and no
// pressed() in either scene; the held button reaches the new scene only after it is released and pressed again.

#include "nnh/actions.hpp"
#include "nnh/events.hpp"
#include "nnh/input.hpp"
#include "transition_fixture.hpp"

using namespace tr_test;
using nnh::Button;
using nnh::Frames;

namespace {

enum class Act { Confirm, Jump };
enum class Ctx { Menu, Play };

constexpr std::uint32_t bit(Button b) { return static_cast<std::uint32_t>(b); }

struct Game {
    nnh::ActionSet actions{{.actions = 2, .contexts = 2}};
    int presses_seen = 0;  ///< pressed() true in a scene's update
    int events_seen = 0;   ///< ActionPressed, ButtonPressed, TouchStarted
    bool started = false;
};
Game* g_game = nullptr;

class Menu : public Probe {
public:
    nnh::Scene* next = nullptr;
    void on_enter() override {
        Probe::on_enter();
        g_game->actions.bind_context(Ctx::Menu, *this);
    }
    void update() override {
        Probe::update();
        if (!g_game->actions.pressed(Act::Confirm)) return;
        if (g_game->started) {
            ++g_game->presses_seen; // a second confirm reached the menu
            return;
        }
        g_game->started = tr::replace(*next, {.out = Frames{4}, .hold = Frames{2}, .in = Frames{4}});
    }
};

class Play : public Probe {
public:
    void on_enter() override {
        Probe::on_enter();
        g_game->actions.bind_context(Ctx::Play, *this);
    }
    void update() override {
        Probe::update();
        if (g_game->actions.pressed(Act::Jump) || g_game->actions.pressed(Act::Confirm)) ++g_game->presses_seen;
    }
};

} // namespace

NNH_TEST(actions_see_nothing_of_the_input_around_a_transition) {
    restart();
    {
        Game game;
        g_game = &game;
        CHECK(game.actions.add_context(Ctx::Play, "play", false));
        CHECK(game.actions.add(Act::Confirm, {.name = "confirm", .context = Ctx::Menu,
                                              .bindings = {{nnh::Source::button(Button::A)}}}));
        CHECK(game.actions.add(Act::Jump, {.name = "jump", .context = Ctx::Play,
                                           .bindings = {{nnh::Source::button(Button::A)}}}));
        nnh::Subscription a = nnh::events::subscribe<nnh::ActionPressed>([](const nnh::ActionPressed&) {
            if (g_game->started) ++g_game->events_seen;
        });
        nnh::Subscription b = nnh::events::subscribe<nnh::ButtonPressed>([](const nnh::ButtonPressed&) {
            if (g_game->started) ++g_game->events_seen;
        });
        nnh::Subscription c = nnh::events::subscribe<nnh::TouchStarted>([](const nnh::TouchStarted&) {
            if (g_game->started) ++g_game->events_seen;
        });
        Menu menu;
        Play play;
        menu.next = &play;
        nnh::scenes::push(menu);
        step(2);
        fake::set_held_buttons(bit(Button::A)); // the confirm press: held through the whole transition
        step();
        CHECK(game.started && tr::running());
        int frame = 0;
        while (tr::running()) {
            if (frame == 3) fake::set_touch(100, 100); // a tap during the transition
            if (frame == 5) fake::release_touch();
            step();
            ++frame;
        }
        CHECK(nnh::scenes::top() == &play);
        step(5); // A still held: blocked
        CHECK(game.presses_seen == 0 && game.events_seen == 0);
        fake::set_held_buttons(0);
        step();
        fake::set_held_buttons(bit(Button::A));
        step();
        CHECK(game.presses_seen == 1 && game.events_seen == 2); // ButtonPressed and ActionPressed{jump}
        nnh::shutdown();
    }
}
