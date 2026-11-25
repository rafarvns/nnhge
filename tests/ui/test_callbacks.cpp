// Callbacks (spec 028 T4.4, FR-20, FR-32): called once per result after the sort, in results order; a callback that
// creates, destroys and hides widgets is laid out before update() returns and drawn the same frame; destroying the UI
// or updating it from its own callback asserts.

#include "ui/ui_block.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiResult;
using nnh::UiWidget;

namespace {

struct Log {
    int calls = 0;
    int order[8] = {};
    int values[8] = {};
};

} // namespace

NNH_TEST(ui_callbacks_run_once_per_result_in_order) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget s = ui.slider(box, {.text = "s", .value = 3});
    static Log log;
    log = Log{};
    ui.on_change(s, [](const UiResult& r) {
        log.order[log.calls] = 2;
        log.values[log.calls++] = r.value;
    });
    ui.on_activate(s, [](const UiResult&) { log.order[log.calls++] = 1; });
    ui_test::settle(ui);
    ui.update(ui_test::press(ui_test::kRight | ui_test::kConfirm));
    CHECK(log.calls == 2 && log.order[0] == 1 && log.order[1] == 2 && log.values[1] == 4);
    ui.update(UiInput{});
    CHECK(log.calls == 2);
}

NNH_TEST(ui_callbacks_capture_this_and_a_pointer) {
    Ui ui = ui_test::make();
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "b"});
    int hits = 0;
    int* where = &hits;
    ui.on_activate(b, [where](const UiResult&) { ++*where; });
    ui_test::settle(ui);
    ui_test::tap(ui, ui_test::kConfirm);
    CHECK(hits == 1 && ui.stats().functions == 1);
    ui.on_activate(b, {}); // cleared: its slot freed
    CHECK(ui.stats().functions == 0);
}

NNH_TEST(ui_callbacks_may_change_the_tree_and_it_draws_the_same_frame) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        static Ui* g;
        static UiWidget panel_a, panel_b, label_b;
        g = &ui;
        panel_a = ui.vbox(ui.root(Screen::Bottom));
        const UiWidget go = ui.button(panel_a, {.text = "Go"});
        panel_b = ui.vbox(ui.root(Screen::Bottom));
        ui.set_visible(panel_b, false);
        ui.on_activate(go, [](const UiResult&) {
            g->set_visible(panel_a, false);
            g->set_visible(panel_b, true);
            label_b = g->label(panel_b, {.text = "created in a callback"});
            g->destroy(panel_a);
        });
        ui_test::settle(ui);
        CHECK(nnh::frame());
        ui.update(ui_test::press(ui_test::kConfirm));
        CHECK(!ui.alive(panel_a) && ui.alive(label_b));
        CHECK(ui.rect(label_b).w == 21 * 8);
        const int layouts = ui.stats().layouts;
        ui.draw();
        CHECK(ui.stats().layouts == layouts); // already laid out by update()
        CHECK(nnh::frame());
        CHECK(nnh::platform_fake::state().last_frame_item_count == 18); // the label's glyphs (3 spaces)
    }
    nnh::shutdown();
}

NNH_TEST(ui_callbacks_cant_update_or_destroy_their_ui) {
    static Ui* g;
    static bool ran;
    Ui ui = ui_test::make();
    g = &ui;
    const UiWidget b = ui.button(ui.root(Screen::Bottom), {.text = "b"});
    ui.on_activate(b, [](const UiResult&) {
        ran = true;
        g->update(UiInput{});
    });
    ui_test::settle(ui);
    ran = false;
    CHECK(ui_test::asserts_or_is_ignored([] { g->update(ui_test::press(ui_test::kConfirm)); }));
    CHECK(ran);
    nnh::UiAccess::block(ui)->busy = 0; // Debug: the assert left the callback loop half way
}

NNH_TEST(ui_callbacks_destroying_the_ui_asserts_and_is_safe_in_release) {
    static Ui* g;
    Ui* owned = new Ui(ui_test::make());
    g = owned;
    const UiWidget b = owned->button(owned->root(Screen::Bottom), {.text = "b"});
    owned->on_activate(b, [](const UiResult&) { delete g; });
    ui_test::settle(*owned);
    const bool crashed = ui_test::crashes([] { g->update(ui_test::press(ui_test::kConfirm)); });
    CHECK(crashed == nnh::build::kAssertsEnabled);
    // Release: the block was freed when the update ended; Debug: the assert stopped the delete half way (leaked)
}
