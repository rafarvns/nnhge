// Cost (spec 028 T6.7, FR-31): nothing allocates after create (operator new and engine memory) over 100 000 operations
// of every kind; an idle update lays out nothing and reports nothing; a list's work doesn't grow with its count
// (test_list.cpp); the block's size per widget.

#include "alloc_counter.hpp"
#include "nnh/memory.hpp"
#include "ui/ui_block.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiWidget;

NNH_TEST(ui_cost_nothing_allocates_after_create) {
    ui_test::restart();
    {
        Ui ui = ui_test::make({.widgets = 128, .texts = 8, .rows = 16, .functions = 8});
        const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
        static const nnh::UiText opts[3] = {"a", "b", "c"};
        for (int k = 0; k < 10; ++k) ui.button(box, {.text = "keep"});
        ui.slider(box, {.text = "s"});
        const std::uint64_t news = alloc_counter::news();
        const int live = nnh::mem::live_allocations();
        std::uint32_t seed = 1;
        for (int i = 0; i < 100000; ++i) {
            seed = seed * 1664525u + 1013904223u;
            switch ((seed >> 24) % 8) {
            case 0: ui.destroy(ui.button(box, {.text = "button"})); break; // create and free: slots, text bytes
            case 1: ui.destroy(ui.choice(box, {.text = "c", .options = opts, .count = 3})); break;
            case 2: ui.update(ui_test::press(static_cast<std::uint8_t>(1u << ((seed >> 8) % 8)))); break;
            case 3: ui.update(UiInput{}); break;
            case 4: (void)ui.rect(box); break;
            case 5: ui.set_text(box, (seed & 2) != 0 ? "changed" : "again!"); break;
            case 6: ui.set_visible(box, (seed & 1) != 0); break;
            default:
                if (i % 997 == 0) {
                    CHECK(nnh::frame());
                    ui.draw();
                }
                break;
            }
        }
        CHECK(alloc_counter::news() == news);
        CHECK(nnh::mem::live_allocations() == live);
    }
    nnh::shutdown();
}

NNH_TEST(ui_cost_an_idle_update_lays_out_nothing_and_reports_nothing) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    for (int i = 0; i < 20; ++i) ui.button(box, {.text = "x"});
    ui_test::settle(ui);
    const int layouts = ui.stats().layouts;
    for (int i = 0; i < 100; ++i) ui.update(UiInput{});
    CHECK(ui.stats().layouts == layouts && ui.results().size == 0);
}

NNH_TEST(ui_cost_bytes_per_widget_row_and_function) {
    CHECK(sizeof(nnh::detail::UiHot) + sizeof(nnh::detail::UiCold) <= 96);
    CHECK(!nnh::detail::kThreeDsLayout || sizeof(nnh::detail::UiHot) + sizeof(nnh::detail::UiCold) == 88);
    CHECK(!nnh::detail::kThreeDsLayout || sizeof(nnh::detail::UiRowSlot) == 72);
    CHECK(nnh::detail::ui_block_bytes({.widgets = 64}) < 64 * 96 + 4096);
}
