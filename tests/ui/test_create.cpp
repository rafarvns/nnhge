// Creating a UI (spec 028 T1.1, FR-1, FR-3, FR-33): every desc bound both ways with the field named, OutOfMemory, the
// block's bytes counted under its tag, a game tag, one or two roots and their rectangles, independent UIs, moves, empty
// UIs answering zero.

#include <cmath>
#include <cstring>

#include "nnh/memory.hpp"
#include "ui/ui_block.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiDesc;

NNH_MEM_TAG(ui_test_tag);

namespace {

bool refused(const UiDesc& d, const char* field) {
    const auto r = Ui::create(d);
    return !r.ok() && r.error().code == nnh::ErrorCode::InvalidArgument && std::strstr(r.error().message, field);
}

} // namespace

NNH_TEST(ui_records_are_32_bytes_hot_and_at_most_64_cold) {
    CHECK(sizeof(nnh::detail::UiHot) == 32);
    CHECK(sizeof(nnh::detail::UiCold) <= 64);
    CHECK(sizeof(nnh::detail::UiHot) + sizeof(nnh::detail::UiCold) <= 96); // FR-1: ≤ 96 bytes per widget
    CHECK(sizeof(nnh::UiResult) == 20);
}

NNH_TEST(ui_desc_widgets_1_to_4096) {
    CHECK(refused({.widgets = 0}, "widgets"));
    CHECK(refused({.widgets = 4097}, "widgets"));
    CHECK(Ui::create({.widgets = 2}).ok());
    CHECK(Ui::create({.widgets = 4096}).ok());
}

NNH_TEST(ui_desc_tables_in_their_ranges) {
    CHECK(refused({.text_bytes = -1}, "text_bytes"));
    CHECK(refused({.text_bytes = 65536}, "text_bytes"));
    CHECK(Ui::create({.text_bytes = 65535}).ok());
    CHECK(refused({.texts = -1}, "texts"));
    CHECK(refused({.texts = 4097}, "texts"));
    CHECK(refused({.styles = -1}, "styles"));
    CHECK(refused({.styles = 241}, "styles"));
    CHECK(Ui::create({.styles = 240}).ok());
    CHECK(refused({.rows = -1}, "rows"));
    CHECK(refused({.rows = 1025}, "rows"));
    CHECK(Ui::create({.rows = 1024}).ok());
    CHECK(refused({.functions = -1}, "functions"));
    CHECK(refused({.functions = 256}, "functions"));
    CHECK(Ui::create({.functions = 255}).ok());
    CHECK(refused({.results = 7}, "results"));
    CHECK(refused({.results = 257}, "results"));
    CHECK(Ui::create({.results = 8}).ok());
    CHECK(Ui::create({.results = 256}).ok());
}

NNH_TEST(ui_desc_needs_a_root_and_its_first_focus) {
    CHECK(refused({.top = false, .bottom = false}, "root"));
    CHECK(refused({.bottom = false, .first_focus = Screen::Bottom}, "first_focus"));
    CHECK(Ui::create({.bottom = false, .first_focus = Screen::Top}).ok());
    CHECK(refused({.widgets = 1}, "widgets")); // two roots need two widgets
    CHECK(Ui::create({.widgets = 1, .top = false}).ok());
}

NNH_TEST(ui_desc_input_settings_checked) {
    CHECK(refused({.input = {.repeat_delay = -1}}, "repeat_delay"));
    CHECK(refused({.input = {.repeat_delay = 256}}, "repeat_delay"));
    CHECK(Ui::create({.input = {.repeat_delay = 0}}).ok());
    CHECK(refused({.input = {.repeat_interval = 0}}, "repeat_interval"));
    CHECK(refused({.input = {.repeat_interval = 256}}, "repeat_interval"));
    CHECK(refused({.input = {.circle_press = NAN}}, "circle"));
    CHECK(refused({.input = {.circle_press = 0.3f, .circle_release = 0.4f}}, "circle"));
    CHECK(refused({.input = {.circle_release = 0.0f}}, "circle"));
    CHECK(refused({.input = {.circle_press = 1.5f, .circle_release = 0.4f}}, "circle"));
    CHECK(refused({.input = {.confirm = nnh::Button::B}}, "confirm"));
    CHECK(refused({.input = {.confirm = static_cast<nnh::Button>(3u)}}, "confirm"));
    CHECK(refused({.input = {.page_back = nnh::Button::R}}, "page"));
    CHECK(Ui::create({.input = {.confirm = nnh::Button::B, .cancel = nnh::Button::A}}).ok());
}

NNH_TEST(ui_block_bytes_are_the_documented_formula_under_the_ui_tag) {
    const UiDesc d{.widgets = 40, .text_bytes = 300, .texts = 10, .styles = 2, .rows = 12, .functions = 6, .results = 16};
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::detail::ui_memory_tag());
    {
        Ui ui = ui_test::make(d);
        const nnh::MemStats now = nnh::mem::tag_stats(nnh::detail::ui_memory_tag());
        CHECK(now.count == before.count + 1);
        CHECK(now.bytes - before.bytes == nnh::detail::ui_block_bytes(d));
        CHECK(ui.stats().block_bytes == nnh::detail::ui_block_bytes(d));
    }
    CHECK(nnh::mem::tag_stats(nnh::detail::ui_memory_tag()).bytes == before.bytes);
    // each widget adds hot + cold bytes, each row a pooled row, each style a style
    UiDesc more = d;
    more.widgets += 10;
    CHECK(nnh::detail::ui_block_bytes(more) - nnh::detail::ui_block_bytes(d) ==
          10 * (sizeof(nnh::detail::UiHot) + sizeof(nnh::detail::UiCold)));
}

NNH_TEST(ui_uses_the_game_tag) {
    Ui ui = ui_test::make({.widgets = 8, .tag = &NNH_MEM_TAG_REF(ui_test_tag)});
    CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(ui_test_tag)).bytes == nnh::detail::ui_block_bytes({.widgets = 8}));
}

NNH_TEST(ui_without_memory_is_out_of_memory) {
    nnh::platform_fake::reset();
    nnh::platform_fake::set_region_capacity(nnh::MemRegion::Heap, 64 * 1024);
    const auto r = Ui::create({.widgets = 4096});
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::OutOfMemory);
    nnh::platform_fake::reset();
}

NNH_TEST(ui_roots_cover_their_screens) {
    Ui ui = ui_test::make();
    CHECK(ui.valid());
    CHECK((ui.rect(ui.root(Screen::Top)) == nnh::UiRect{0, 0, 400, 240}));
    CHECK((ui.rect(ui.root(Screen::Bottom)) == nnh::UiRect{0, 0, 320, 240}));
    CHECK(ui.kind(ui.root(Screen::Top)) == nnh::UiKind::Root);
    CHECK(ui.screen(ui.root(Screen::Bottom)) == Screen::Bottom);
    CHECK(ui.stats().widgets == 2 && ui.stats().capacity == 64);
}

NNH_TEST(ui_with_one_root_has_no_other) {
    Ui ui = ui_test::make({.top = false});
    CHECK(!ui.root(Screen::Top));
    CHECK(ui.root(Screen::Bottom));
    CHECK(ui.stats().widgets == 1);
}

NNH_TEST(ui_two_uis_are_independent) {
    Ui a = ui_test::make();
    Ui b = ui_test::make();
    const nnh::UiWidget wa = a.label(a.root(Screen::Top), {.text = "A"});
    CHECK(a.alive(wa));
    CHECK(a.stats().widgets == 3 && b.stats().widgets == 2);
    CHECK(std::strcmp(a.text(wa), "A") == 0);
}

NNH_TEST(ui_move_leaves_the_source_empty) {
    Ui a = ui_test::make();
    const nnh::UiWidget w = a.label(a.root(Screen::Top), {.text = "moved"});
    Ui b(static_cast<Ui&&>(a));
    CHECK(!a.valid() && b.valid() && b.alive(w));
    Ui c = ui_test::make();
    c = static_cast<Ui&&>(b);
    CHECK(!b.valid() && c.valid() && std::strcmp(c.text(w), "moved") == 0);
}

NNH_TEST(ui_empty_answers_zero) {
    const Ui ui;
    CHECK(!ui.valid());
    CHECK(!ui.root(Screen::Top));
    CHECK(!ui.focused());
    CHECK(ui.stats().widgets == 0 && ui.stats().capacity == 0);
    CHECK(ui.results().size == 0);
    CHECK(!ui.alive(nnh::UiWidget::from_bits(0x10001u)));
    ui.draw(); // nothing
}
