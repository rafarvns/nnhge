// The widget tree (spec 028 T1.7, T2.2, FR-2, FR-32): appended as the last child, stale handles, subtree destroy,
// slot reuse with a new generation, a full UI, children under non-containers, per-kind creation errors.

#include <cstring>

#include "ui/ui_block.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiWidget;
using nnh::detail::UiBlock;

namespace {

const UiBlock& block(const Ui& ui) { return *nnh::UiAccess::block(ui); }

/// The children of `w` in order, as handles.
int children(const Ui& ui, UiWidget w, UiWidget* out, int capacity) {
    const UiBlock& b = block(ui);
    int n = 0;
    for (int s = b.hot[w.slot()].first_child; s != nnh::detail::kUiNone && n < capacity; s = b.hot[s].next) {
        out[n++] = nnh::detail::handle_of(b, s);
    }
    return n;
}

Ui* g_ui = nullptr;
UiWidget g_w;

} // namespace

NNH_TEST(ui_tree_new_widgets_are_appended_as_the_last_child) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget a = ui.label(box, {.text = "a"});
    const UiWidget b = ui.label(box, {.text = "b"});
    const UiWidget c = ui.button(box, {.text = "c"});
    UiWidget kids[4];
    CHECK(children(ui, box, kids, 4) == 3);
    CHECK(kids[0] == a && kids[1] == b && kids[2] == c);
    CHECK(block(ui).hot[block(ui).hot[box.slot()].first_child].prev == c.slot()); // first's prev is the last
}

NNH_TEST(ui_tree_every_container_holds_children) {
    Ui ui = ui_test::make({.widgets = 32, .rows = 8});
    const UiWidget root = ui.root(Screen::Bottom);
    const UiWidget containers[7] = {root,
                                    ui.vbox(root),
                                    ui.hbox(root),
                                    ui.grid_box(root, {.columns = 3}),
                                    ui.stack(root),
                                    ui.scroll_box(root),
                                    ui.panel(root)};
    for (const UiWidget c : containers) CHECK(ui.alive(ui.label(c, {.text = "x"})));
}

NNH_TEST(ui_tree_destroy_takes_the_subtree) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Bottom));
    const UiWidget inner = ui.hbox(box);
    const UiWidget a = ui.label(inner, {.text = "a"});
    const UiWidget b = ui.button(box, {.text = "b"});
    CHECK(ui.stats().widgets == 6);
    ui.destroy(box);
    CHECK(ui.stats().widgets == 2);
    CHECK(!ui.alive(box) && !ui.alive(inner) && !ui.alive(a) && !ui.alive(b));
    CHECK(block(ui).hot[ui.root(Screen::Bottom).slot()].first_child == nnh::detail::kUiNone);
}

NNH_TEST(ui_tree_destroy_of_a_middle_child_keeps_the_links) {
    Ui ui = ui_test::make();
    const UiWidget box = ui.vbox(ui.root(Screen::Top));
    const UiWidget a = ui.label(box, {.text = "a"});
    const UiWidget b = ui.label(box, {.text = "b"});
    const UiWidget c = ui.label(box, {.text = "c"});
    ui.destroy(b);
    UiWidget kids[4];
    CHECK(children(ui, box, kids, 4) == 2 && kids[0] == a && kids[1] == c);
    ui.destroy(c);
    CHECK(children(ui, box, kids, 4) == 1 && kids[0] == a);
    CHECK(block(ui).hot[a.slot()].prev == a.slot());
}

NNH_TEST(ui_tree_slots_are_reused_with_a_new_generation) {
    Ui ui = ui_test::make();
    const UiWidget a = ui.label(ui.root(Screen::Top), {.text = "a"});
    ui.destroy(a);
    const UiWidget b = ui.label(ui.root(Screen::Top), {.text = "b"});
    CHECK(b.slot() == a.slot() && b.generation() != a.generation());
    CHECK(!ui.alive(a) && ui.alive(b));
}

NNH_TEST(ui_tree_lowest_free_slot_first) {
    Ui ui = ui_test::make();
    const UiWidget a = ui.label(ui.root(Screen::Top), {.text = "a"});
    const UiWidget b = ui.label(ui.root(Screen::Top), {.text = "b"});
    ui.label(ui.root(Screen::Top), {.text = "c"});
    ui.destroy(b);
    ui.destroy(a);
    CHECK(ui.label(ui.root(Screen::Top), {.text = "d"}).slot() == a.slot());
}

NNH_TEST(ui_tree_stale_handles_assert) {
    Ui ui = ui_test::make();
    g_ui = &ui;
    g_w = ui.label(ui.root(Screen::Top), {.text = "a"});
    ui.destroy(g_w);
    CHECK(ui_test::asserts_or_is_ignored([] { g_ui->set_visible(g_w, false); }));
    CHECK(ui_test::asserts_or_is_ignored([] { g_ui->destroy(g_w); }));
    CHECK(ui_test::asserts_or_is_ignored([] { g_ui->label(g_w, {.text = "child of a stale parent"}); }));
    CHECK(ui_test::asserts_or_is_ignored([] { (void)g_ui->text(g_w); })); // queries assert too...
    if (!nnh::build::kAssertsEnabled) CHECK(ui.text(g_w)[0] == 0 && ui.rect(g_w).w == 0); // ...and answer nothing
    CHECK(!ui.alive(g_w));
    CHECK(ui.stats().widgets == 2);
}

NNH_TEST(ui_tree_full_gives_a_null_handle) {
    Ui ui = ui_test::make({.widgets = 4});
    g_ui = &ui;
    CHECK(ui.alive(ui.label(ui.root(Screen::Top), {.text = "1"})));
    CHECK(ui.alive(ui.label(ui.root(Screen::Top), {.text = "2"})));
    CHECK(ui_test::asserts_or_is_ignored([] { g_w = g_ui->label(g_ui->root(Screen::Top), {.text = "3"}); }));
    if (!nnh::build::kAssertsEnabled) CHECK(!g_w);
    CHECK(ui.stats().widgets == 4);
}

NNH_TEST(ui_tree_full_logs_one_error_per_ui_in_release) {
    if (nnh::build::kAssertsEnabled) return; // Debug/Development assert instead (above)
    Ui ui = ui_test::make({.widgets = 2});
    const int before = nnh::platform_fake::state().console_lines;
    for (int i = 0; i < 5; ++i) CHECK(!ui.label(ui.root(Screen::Top), {.text = "x"}));
    CHECK(nnh::platform_fake::state().console_lines == before + 1);
}

NNH_TEST(ui_tree_children_only_under_containers) {
    Ui ui = ui_test::make({.texts = 2, .rows = 8});
    g_ui = &ui;
    const UiWidget root = ui.root(Screen::Bottom);
    static UiWidget leaves[9];
    leaves[0] = ui.label(root, {.text = "l"});
    leaves[1] = ui.image(root, {});
    leaves[2] = ui.progress(root, {});
    leaves[3] = ui.spacer(root);
    leaves[4] = ui.button(root, {.text = "b"});
    leaves[5] = ui.toggle(root, {.text = "t"});
    leaves[6] = ui.slider(root, {.text = "s"});
    static const nnh::UiText opts[2] = {"x", "y"};
    leaves[7] = ui.choice(root, {.text = "c", .options = opts, .count = 2});
    leaves[8] = ui.list(root, {.rows = 4, .count = 3, .fill = [](int, nnh::UiRow&) {}});
    static int i;
    for (i = 0; i < 9; ++i) {
        CHECK(ui.alive(leaves[i]));
        CHECK(ui_test::asserts_or_is_ignored([] { g_w = g_ui->label(leaves[i], {.text = "child"}); }));
    }
}

NNH_TEST(ui_tree_a_non_box_panel_holds_one_child) {
    Ui ui = ui_test::make();
    g_ui = &ui;
    static UiWidget panel;
    panel = ui.panel(ui.root(Screen::Bottom));
    CHECK(ui.alive(ui.label(panel, {.text = "one"})));
    CHECK(ui_test::asserts_or_is_ignored([] { g_w = g_ui->label(panel, {.text = "two"}); }));
    const UiWidget box_panel = ui.panel(ui.root(Screen::Bottom), {.as_box = true});
    CHECK(ui.alive(ui.label(box_panel, {.text = "one"})) && ui.alive(ui.label(box_panel, {.text = "two"})));
}

NNH_TEST(ui_tree_bad_descs_give_null_handles) {
    Ui ui = ui_test::make({.rows = 8});
    g_ui = &ui;
    CHECK(ui_test::asserts_or_is_ignored([] { g_w = g_ui->slider(g_ui->root(Screen::Bottom), {.min = 5, .max = 1}); }));
    CHECK(ui_test::asserts_or_is_ignored([] { g_w = g_ui->slider(g_ui->root(Screen::Bottom), {.step = 0}); }));
    CHECK(ui_test::asserts_or_is_ignored([] { g_w = g_ui->choice(g_ui->root(Screen::Bottom), {.count = 0}); }));
    CHECK(ui_test::asserts_or_is_ignored([] { g_w = g_ui->list(g_ui->root(Screen::Bottom), {.count = 3}); }));
    CHECK(ui_test::asserts_or_is_ignored(
        [] { g_w = g_ui->list(g_ui->root(Screen::Bottom), {.rows = 9, .fill = [](int, nnh::UiRow&) {}}); }));
    CHECK(ui.stats().widgets == 2);
}

NNH_TEST(ui_tree_roots_cant_be_destroyed) {
    Ui ui = ui_test::make();
    g_ui = &ui;
    CHECK(ui_test::asserts_or_is_ignored([] { g_ui->destroy(g_ui->root(Screen::Top)); }));
    CHECK(ui.alive(ui.root(Screen::Top)));
}

NNH_TEST(ui_tree_copied_text_is_compacted_on_destroy) {
    Ui ui = ui_test::make({.text_bytes = 16});
    const UiWidget a = ui.label(ui.root(Screen::Top), {.text = "aaaa"}); // 5 bytes
    const UiWidget b = ui.label(ui.root(Screen::Top), {.text = "bbbb"}); // 10
    const UiWidget c = ui.label(ui.root(Screen::Top), {.text = "cccc"}); // 15
    CHECK(ui.stats().text_bytes == 15);
    ui.destroy(b);
    CHECK(ui.stats().text_bytes == 10);
    CHECK(std::strcmp(ui.text(a), "aaaa") == 0 && std::strcmp(ui.text(c), "cccc") == 0);
    const UiWidget d = ui.label(ui.root(Screen::Top), {.text = "dddd"});
    CHECK(ui.alive(d) && std::strcmp(ui.text(d), "dddd") == 0 && std::strcmp(ui.text(c), "cccc") == 0);
}
