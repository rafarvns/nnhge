// Keys (spec 028 T1.7, FR-4): the game's enum values name widgets, find() by key, duplicates assert.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiKey;
using nnh::UiWidget;
using ui_test::K;

namespace {
Ui* g_ui = nullptr;
UiWidget g_w;
} // namespace

NNH_TEST(ui_keys_come_from_enums_and_find_widgets) {
    Ui ui = ui_test::make();
    const UiWidget a = ui.button(ui.root(Screen::Bottom), {.text = "a", .key = K::A});
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "b", .key = K::B});
    CHECK(ui.find(K::A) == a && ui.find(K::B) == b);
    CHECK(!ui.find(K::C));
    CHECK(ui.key(a) == UiKey(K::A));
    CHECK(ui.key(ui.root(Screen::Top)).none());
    CHECK(UiKey(3).value == 3 && UiKey{}.none());
}

NNH_TEST(ui_keys_duplicates_assert_and_create_nothing) {
    Ui ui = ui_test::make();
    g_ui = &ui;
    ui.button(ui.root(Screen::Bottom), {.text = "a", .key = K::A});
    CHECK(ui_test::asserts_or_is_ignored([] { g_w = g_ui->button(g_ui->root(Screen::Bottom), {.key = K::A}); }));
    CHECK(ui.stats().widgets == 3);
    static UiWidget b;
    b = ui.button(ui.root(Screen::Bottom), {.text = "b", .key = K::B});
    CHECK(ui_test::asserts_or_is_ignored([] { g_ui->set_key(b, K::A); }));
    CHECK(ui.key(b) == UiKey(K::B));
}

NNH_TEST(ui_keys_free_again_after_destroy) {
    Ui ui = ui_test::make();
    const UiWidget a = ui.button(ui.root(Screen::Bottom), {.text = "a", .key = K::A});
    ui.destroy(a);
    CHECK(!ui.find(K::A));
    const UiWidget again = ui.button(ui.root(Screen::Bottom), {.text = "a2", .key = K::A});
    CHECK(ui.find(K::A) == again);
}

NNH_TEST(ui_keys_set_key_and_clear) {
    Ui ui = ui_test::make();
    const UiWidget a = ui.button(ui.root(Screen::Bottom), {.text = "a"});
    ui.set_key(a, K::C);
    CHECK(ui.find(K::C) == a);
    ui.set_key(a, UiKey{});
    CHECK(!ui.find(K::C));
}
