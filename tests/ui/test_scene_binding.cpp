// Scene binding (spec 028 T4.6, FR-17, FR-21, FR-22; D30 unchanged): a bound UI is updated right before its scene's
// update(), frozen under a pushed pause scene, reset after on_exit with its focus memory kept; a held confirm after a
// pop is ignored; update() by hand on a bound UI asserts; moved and destroyed while bound.

#include "nnh/scene.hpp"
#include "ui_fixture.hpp"

using nnh::Button;
using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;

namespace {

namespace fake = nnh::platform_fake;

void frames(int n) {
    for (int i = 0; i < n; ++i) {
        CHECK(nnh::frame());
        nnh::scenes::update_and_draw();
    }
}

class Menu : public nnh::Scene {
public:
    Menu() : ui(ui_test::make()) {
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        a = ui.button(box, {.text = "a"});
        b = ui.button(box, {.text = "b"});
        ui.bind(*this);
    }
    void update() override {
        ++updates;
        if (ui.activated(a) || ui.activated(b)) ++activations;
        seen_focus_b = ui.focused() == b;
    }
    Ui ui;
    UiWidget a, b;
    int updates = 0, activations = 0;
    bool seen_focus_b = false;
};

class Pause : public nnh::Scene {};

Ui* g_ui = nullptr;

} // namespace

NNH_TEST(ui_bound_update_runs_before_the_scenes) {
    ui_test::restart();
    {
        Menu m;
        nnh::scenes::push(m);
        frames(2);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::Down));
        frames(1);
        CHECK(m.seen_focus_b); // the scene saw this frame's move
        fake::set_held_buttons(0);
        nnh::shutdown();
    }
}

NNH_TEST(ui_bound_is_frozen_under_a_pause_scene_and_ignores_a_held_press_after) {
    ui_test::restart();
    {
        Menu m;
        Pause p;
        nnh::scenes::push(m);
        frames(2);
        nnh::scenes::push(p);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::Down));
        frames(3);
        CHECK(m.ui.focused() == m.a); // not updated under the pause
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::A));
        nnh::scenes::pop();
        frames(3); // A held across the pop: ignored (a frame gap)
        CHECK(m.activations == 0);
        fake::set_held_buttons(0);
        frames(1);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::A));
        frames(1);
        CHECK(m.activations == 1);
        fake::set_held_buttons(0);
        nnh::shutdown();
    }
}

NNH_TEST(ui_bound_reset_after_exit_keeps_the_focus_memory) {
    ui_test::restart();
    {
        Menu m;
        nnh::scenes::push(m);
        frames(1);
        m.ui.focus(m.b);
        fake::set_held_buttons(static_cast<std::uint32_t>(Button::A));
        frames(1);
        CHECK(m.activations == 1);
        nnh::scenes::pop();
        frames(1); // on_exit: reset
        CHECK(m.ui.focused() == m.b);
        nnh::scenes::push(m);
        frames(2); // A still held: ignored
        CHECK(m.activations == 1 && m.ui.focused() == m.b);
        fake::set_held_buttons(0);
        nnh::shutdown();
    }
}

NNH_TEST(ui_bound_update_by_hand_asserts) {
    ui_test::restart();
    {
        Menu m;
        g_ui = &m.ui;
        CHECK(ui_test::asserts_or_is_ignored([] { g_ui->update(); }));
        m.ui.unbind();
        m.ui.update(); // fine once unbound
        nnh::shutdown();
    }
}

NNH_TEST(ui_bound_moved_and_destroyed_while_bound) {
    ui_test::restart();
    {
        Menu m;
        nnh::scenes::push(m);
        frames(1);
        Ui moved(static_cast<Ui&&>(m.ui)); // the block (and its attachment) moves with it
        frames(2);
        {
            Ui gone(static_cast<Ui&&>(moved));
        } // destroyed: detached from the scene
        frames(2);
        CHECK(m.updates == 5);
        nnh::shutdown();
    }
}
