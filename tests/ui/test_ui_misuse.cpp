// Misuse (spec 028 T6.7, FR-32): each case of plan.md asserts in Debug/Development and is harmless in Release (the
// UI stays consistent and keeps working).

#include <limits>

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiDir;
using nnh::UiWidget;
using ui_test::asserts_or_is_ignored;
using ui_test::K;

namespace {

Ui* g = nullptr;
UiWidget g_box, g_button, g_label, g_stale;

struct Fixture {
    Ui ui = ui_test::make({.texts = 4, .rows = 8});
    Fixture() {
        g = &ui;
        g_box = ui.vbox(ui.root(Screen::Bottom));
        g_button = ui.button(g_box, {.text = "b", .key = K::B});
        g_label = ui.label(g_box, {.text = "l"});
        g_stale = ui.button(g_box, {.text = "gone"});
        ui.destroy(g_stale);
        ui_test::settle(ui);
    }
    /// Still working after the misuse.
    bool healthy() {
        ui.update(nnh::UiInput{});
        return ui.alive(g_button) && ui.stats().widgets == 5 && ui.focused() == g_button;
    }
};

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

} // namespace

NNH_TEST(ui_misuse_stale_handles) {
    Fixture f;
    CHECK(asserts_or_is_ignored([] { g->set_text(g_stale, "x"); }));
    CHECK(asserts_or_is_ignored([] { g->set_enabled(g_stale, false); }));
    CHECK(asserts_or_is_ignored([] { g->set_value(g_stale, 1); }));
    CHECK(asserts_or_is_ignored([] { g->set_neighbour(g_button, UiDir::Up, g_stale); }));
    CHECK(asserts_or_is_ignored([] { g->focus(g_stale); }));
    CHECK(asserts_or_is_ignored([] { (void)g->rect(g_stale); }));
    CHECK(f.healthy());
}

NNH_TEST(ui_misuse_duplicate_keys_and_bad_parents) {
    Fixture f;
    CHECK(asserts_or_is_ignored([] { (void)g->button(g_box, {.key = K::B}); }));
    CHECK(asserts_or_is_ignored([] { (void)g->button(g_label, {.text = "under a label"}); }));
    CHECK(asserts_or_is_ignored([] { (void)g->button(g_stale, {.text = "under a stale parent"}); }));
    CHECK(f.healthy());
}

NNH_TEST(ui_misuse_bad_values_are_refused) {
    Fixture f;
    CHECK(asserts_or_is_ignored([] { (void)g->slider(g_box, {.min = 3, .max = 1}); }));
    CHECK(asserts_or_is_ignored([] { (void)g->slider(g_box, {.step = 0}); }));
    CHECK(asserts_or_is_ignored([] { (void)g->list(g_box, {.count = 3}); }));
    CHECK(asserts_or_is_ignored([] { g->set_render_offset(g_button, {kNaN, 0}); }));
    CHECK(asserts_or_is_ignored([] { g->set_opacity(g_button, kNaN); }));
    CHECK(asserts_or_is_ignored([] { g->set_value(g_label, 3); }));
    CHECK(asserts_or_is_ignored([] { g->set_progress_max(g_button, 5); }));
    CHECK(asserts_or_is_ignored([] { g->set_reveal(g_button, 2); }));
    CHECK(asserts_or_is_ignored([] { g->set_item_count(g_button, 2); }));
    CHECK(asserts_or_is_ignored([] { g->set_scroll(g_button, 2); }));
    CHECK(asserts_or_is_ignored([] { g->set_layer(Screen::Top, 16); }));
    CHECK(asserts_or_is_ignored([] { g->set_style(g_button, nnh::UiStyleId{3}); }));
    CHECK(f.healthy());
}

NNH_TEST(ui_misuse_containers_and_modals) {
    Fixture f;
    CHECK(asserts_or_is_ignored([] { g->set_default(g_button, g_label); }));
    CHECK(asserts_or_is_ignored([] { g->set_back(g_box, g->root(Screen::Top)); })); // not inside it
    CHECK(asserts_or_is_ignored([] { g->open_modal(g->root(Screen::Bottom)); }));
    CHECK(asserts_or_is_ignored([] { g->close_modal(); }));
    CHECK(asserts_or_is_ignored([] { g->destroy(g->root(Screen::Bottom)); }));
    CHECK(f.healthy());
}

NNH_TEST(ui_misuse_an_empty_ui_ignores_everything) {
    Ui ui;
    ui.update();
    ui.update(nnh::UiInput{});
    ui.draw();
    ui.focus(nnh::UiKey(1));
    ui.set_theme(nnh::ui::default_theme());
    ui.text_changed();
    CHECK(!ui.label(UiWidget{}, {.text = "x"}));
    CHECK(ui.results().size == 0 && !ui.cancelled() && ui.value(nnh::UiKey(1)) == 0);
}
