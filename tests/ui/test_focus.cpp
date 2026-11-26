// Focus (spec 028 T3.1, FR-14, FR-15): what can take focus (hidden, inside hidden containers, disabled skipped or inert,
// empty lists), focus by widget, key and container, and repair after a hide, disable or destroy — never dangling.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiState;
using nnh::UiWidget;
using ui_test::K;

namespace {

struct Menu {
    Ui ui = ui_test::make({.rows = 8});
    UiWidget box, a, b, c;
    explicit Menu(bool inert = false) {
        box = ui.vbox(ui.root(Screen::Bottom), {.disabled_focusable = inert});
        a = ui.button(box, {.text = "a", .key = K::A});
        b = ui.button(box, {.text = "b", .key = K::B});
        c = ui.button(box, {.text = "c", .key = K::C});
    }
};

} // namespace

NNH_TEST(ui_focus_the_first_update_enters_the_first_root) {
    Menu m;
    CHECK(!m.ui.focused());
    ui_test::settle(m.ui);
    CHECK(m.ui.focused() == m.a && m.ui.focused_key() == nnh::UiKey(K::A));
    CHECK(m.ui.state(m.a) == UiState::Focused && m.ui.state(m.b) == UiState::Normal);
}

NNH_TEST(ui_focus_first_focus_can_be_the_top_root) {
    Ui ui = ui_test::make({.first_focus = Screen::Top});
    const UiWidget t = ui.button(ui.root(Screen::Top), {.text = "top"});
    ui.button(ui.root(Screen::Bottom), {.text = "bottom"});
    ui_test::settle(ui);
    CHECK(ui.focused() == t);
}

NNH_TEST(ui_focus_skips_hidden_and_disabled_widgets) {
    Menu m;
    m.ui.set_visible(m.a, false);
    m.ui.set_enabled(m.b, false);
    ui_test::settle(m.ui);
    CHECK(m.ui.focused() == m.c);
}

NNH_TEST(ui_focus_skips_widgets_in_hidden_containers) {
    Ui ui = ui_test::make();
    const UiWidget hidden = ui.vbox(ui.root(Screen::Bottom));
    ui.button(hidden, {.text = "in"});
    const UiWidget out = ui.button(ui.root(Screen::Bottom), {.text = "out"});
    ui.set_visible(hidden, false);
    ui_test::settle(ui);
    CHECK(ui.focused() == out);
}

NNH_TEST(ui_focus_inert_disabled_widgets_take_focus) {
    Menu m(true);
    m.ui.set_enabled(m.a, false);
    ui_test::settle(m.ui);
    CHECK(m.ui.focused() == m.a);
    CHECK(m.ui.state(m.a) == UiState::Disabled);
}

NNH_TEST(ui_focus_an_empty_list_takes_no_focus) {
    Ui ui = ui_test::make({.rows = 8});
    const UiWidget l = ui.list(ui.root(Screen::Bottom), {.rows = 4, .count = 0, .fill = [](int, nnh::UiRow&) {}});
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "b"});
    ui_test::settle(ui);
    CHECK(ui.focused() == b);
    ui.set_item_count(l, 3);
    ui.focus(l);
    CHECK(ui.focused() == l);
}

NNH_TEST(ui_focus_by_widget_key_and_container) {
    Menu m;
    ui_test::settle(m.ui);
    m.ui.focus(m.c);
    CHECK(m.ui.focused() == m.c);
    m.ui.focus(K::B);
    CHECK(m.ui.focused() == m.b);
    m.ui.focus(m.box); // its remembered child
    CHECK(m.ui.focused() == m.b);
    m.ui.focus(nnh::UiWidget{});
    CHECK(!m.ui.focused());
}

NNH_TEST(ui_focus_set_by_the_game_reports_nothing) {
    Menu m;
    ui_test::settle(m.ui);
    m.ui.focus(m.c);
    CHECK(m.ui.results().size == 0);
}

NNH_TEST(ui_focus_repair_after_hide_goes_to_the_next_then_the_previous) {
    Menu m;
    ui_test::settle(m.ui);
    m.ui.focus(m.b);
    m.ui.set_visible(m.b, false);
    CHECK(m.ui.focused() == m.c);
    m.ui.set_visible(m.c, false);
    CHECK(m.ui.focused() == m.a);
}

NNH_TEST(ui_focus_repair_after_disable_and_destroy) {
    Menu m;
    ui_test::settle(m.ui);
    m.ui.focus(m.b);
    m.ui.set_enabled(m.b, false);
    CHECK(m.ui.focused() == m.c);
    m.ui.destroy(m.c);
    CHECK(m.ui.focused() == m.a);
}

NNH_TEST(ui_focus_repair_falls_back_to_the_root_default_then_none) {
    Ui ui = ui_test::make();
    const UiWidget other = ui.button(ui.root(Screen::Bottom), {.text = "other"});
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget only = ui.button(box, {.text = "only"});
    ui.set_default(ui.root(Screen::Bottom), other);
    ui_test::settle(ui);
    ui.focus(only);
    ui.set_visible(only, false); // no sibling: the box's container is the box
    CHECK(ui.focused() == other);
    ui.set_default(ui.root(Screen::Bottom), {});
    ui.destroy(other);
    CHECK(!ui.focused());
}

NNH_TEST(ui_focus_destroying_an_ancestor_of_the_focus_repairs_it) {
    Ui ui = ui_test::make();
    const UiWidget first = ui.button(ui.root(Screen::Bottom), {.text = "1"});
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget inner = ui.button(box, {.text = "in"});
    ui_test::settle(ui);
    ui.focus(inner);
    ui.destroy(box);
    CHECK(ui.focused() == first);
}
