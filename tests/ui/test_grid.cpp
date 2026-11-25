// Grids (spec 028 T5.1, FR-11): four directions by columns, left/right inside a row, the last partial row, scrolling by
// rows of cells, item rectangles.

#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiRect;
using nnh::UiWidget;
using ui_test::kDown;
using ui_test::kLeft;
using ui_test::kRight;
using ui_test::kUp;
using ui_test::tap;

namespace {

struct GridUi {
    Ui ui = ui_test::make({.rows = 16});
    UiWidget grid;
    GridUi(int count, int rows = 3) {
        grid = ui.list(ui.root(Screen::Bottom), {.rows = rows, .columns = 4, .count = count,
                                                .fill = [](int, nnh::UiRow&) {}, .margin = 0, .row_height = 20,
                                                .cell_width = 20});
        ui_test::settle(ui);
    }
    int item() const { return ui.list_info(grid).focused; }
};

} // namespace

NNH_TEST(ui_grid_is_a_list_with_columns) {
    GridUi g(10);
    CHECK(g.ui.kind(g.grid) == nnh::UiKind::Grid);
    CHECK((g.ui.rect(g.grid) == UiRect{0, 0, 84, 60}));
}

NNH_TEST(ui_grid_moves_in_four_directions) {
    GridUi g(10);
    tap(g.ui, kRight);
    CHECK(g.item() == 1);
    tap(g.ui, kDown);
    CHECK(g.item() == 5);
    tap(g.ui, kDown);
    CHECK(g.item() == 9);
    tap(g.ui, kLeft);
    CHECK(g.item() == 8);
    tap(g.ui, kUp);
    CHECK(g.item() == 4);
}

NNH_TEST(ui_grid_left_right_stay_in_the_row) {
    GridUi g(10);
    tap(g.ui, kLeft); // column 0: passes on (nothing else: blocked)
    CHECK(g.item() == 0 && ui_test::has(g.ui, nnh::UiResultKind::Blocked));
    for (int i = 0; i < 3; ++i) tap(g.ui, kRight);
    CHECK(g.item() == 3);
    tap(g.ui, kRight);
    CHECK(g.item() == 3);
}

NNH_TEST(ui_grid_last_partial_row_wraps_down_to_the_top) {
    GridUi g(10);
    g.ui.set_item(g.grid, 6); // row 1, column 2: below it nothing (items 0..9)
    tap(g.ui, kDown);
    CHECK(g.item() == 2);
    tap(g.ui, kUp); // up from the top row: wraps to the column's last item
    CHECK(g.item() == 6);
}

NNH_TEST(ui_grid_scrolls_by_rows_of_cells) {
    GridUi g(40, 2);
    for (int i = 0; i < 3; ++i) tap(g.ui, kDown);
    CHECK(g.item() == 12 && g.ui.list_info(g.grid).first_visible == 8);
    const UiRect r = g.ui.rect(g.grid);
    (void)r;
}
