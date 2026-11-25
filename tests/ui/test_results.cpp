// Results (spec 028 T4.4, FR-20): the fixed order whatever the processing order, keys carried, per-key and per-widget
// queries, valid until the next update, drops counted (and asserted) past UiDesc::results.

#include "ui/ui_block.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiKey;
using nnh::UiResultKind;
using nnh::UiWidget;
using ui_test::K;

NNH_TEST(ui_results_come_in_the_documented_order) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    ui.button(box, {.text = "a", .key = K::A});
    ui.button(box, {.text = "b", .key = K::B});
    ui_test::settle(ui);
    // confirm, cancel, page and a move in one frame: processed move → page → confirm → cancel, reported in kind order
    nnh::UiInput in = ui_test::press(ui_test::kDown | ui_test::kConfirm | ui_test::kCancel | ui_test::kPageForward);
    ui.update(in);
    const nnh::UiResults r = ui.results();
    CHECK(r.size == 4);
    CHECK(r.data[0].kind == UiResultKind::FocusChanged && r.data[0].key == UiKey(K::B));
    CHECK(r.data[0].other == ui.find(K::A));
    CHECK(r.data[1].kind == UiResultKind::Activated && r.data[1].key == UiKey(K::B));
    CHECK(r.data[2].kind == UiResultKind::Cancelled);
    CHECK(r.data[3].kind == UiResultKind::Page && r.data[3].value == 1);
}

NNH_TEST(ui_results_queries_by_key_and_widget) {
    Ui ui = ui_test::make();
    const UiWidget s = ui.slider(ui.root(Screen::Bottom), {.text = "s", .value = 4, .key = K::Slider});
    ui_test::settle(ui);
    ui_test::tap(ui, ui_test::kRight);
    CHECK(ui.changed(K::Slider) && ui.value(K::Slider) == 5 && !ui.changed(K::A));
    ui_test::tap(ui, ui_test::kConfirm);
    CHECK(ui.activated(K::Slider) && ui.activated(s) && !ui.activated(K::B));
    ui.update(UiInput{});
    CHECK(ui.results().size == 0 && !ui.activated(s));
}

NNH_TEST(ui_results_past_capacity_are_dropped_and_counted) {
    static nnh::detail::UiBlock* g;
    Ui ui = ui_test::make({.results = 8});
    g = nnh::UiAccess::block(ui);
    for (int i = 0; i < 8; ++i) nnh::detail::add_result(*g, UiResultKind::Blocked, -1);
    CHECK(ui.results().size == 8 && ui.stats().dropped_results == 0);
    CHECK(ui_test::asserts_or_is_ignored([] { nnh::detail::add_result(*g, UiResultKind::Blocked, -1); }));
    CHECK(ui.results().size == 8 && ui.stats().dropped_results == 1);
}
