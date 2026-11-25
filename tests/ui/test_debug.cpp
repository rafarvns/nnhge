// Debugging (spec 028 T6.6, FR-30): the dump's exact lines for a small tree, debug-draw items (one outline per visible
// widget, not counted as the UI's draws), stats; nothing in Release.

#include <cstring>

#include "ui_draw_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;
using ui_test::K;

NNH_TEST(ui_debug_dump_writes_one_line_per_widget) {
    Ui ui = ui_test::make({.top = false});
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    ui.button(box, {.text = "Go", .key = K::B});
    ui_test::settle(ui);
    const int before = nnh::platform_fake::state().console_lines;
    ui.dump();
    const auto& s = nnh::platform_fake::state();
    if (nnh::build::kDevToolsEnabled) {
        CHECK(s.console_lines == before + 3);
        CHECK(std::strstr(s.last_console_line, "ui dump:     button key=1 rect=0,0,28,16 state=focused value=0") !=
              nullptr);
    } else {
        CHECK(s.console_lines == before);
    }
}

NNH_TEST(ui_debug_draw_outlines_visible_widgets) {
    ui_test::restart();
    {
        Ui ui = ui_test::make();
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        ui.button(box, {.text = "a"});
        const UiWidget hidden = ui.button(box, {.text = "b"});
        ui.set_visible(hidden, false);
        ui_test::Item items[16];
        const int n = ui_test::capture([&] { ui.debug_draw(Screen::Bottom); }, items, 16);
        CHECK(n == (nnh::build::kDevToolsEnabled ? 3 : 0)); // root, box, button
        if (n > 0) CHECK(items[0].kind == ui_test::Item::Kind::RectOutline && items[0].layer == 15);
        CHECK(ui.stats().draws == 0);
    }
    nnh::shutdown();
}

NNH_TEST(ui_debug_stats_count_what_is_used) {
    Ui ui = ui_test::make({.widgets = 10, .text_bytes = 50, .rows = 8, .functions = 4});
    ui.label(ui.root(Screen::Top), {.text = "four"});
    ui.list(ui.root(Screen::Bottom), {.rows = 3, .count = 9, .fill = [](int, nnh::UiRow&) {}});
    const nnh::UiStats s = ui.stats();
    CHECK(s.widgets == 4 && s.capacity == 10 && s.text_bytes == 5 && s.text_capacity == 50);
    CHECK(s.rows == 3 && s.functions == 1 && s.block_bytes > 0);
}
