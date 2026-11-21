// Grids the game describes: grid_view, cell lookup with `outside`, cell ranges of a box (spec 012 FR-8,
// FR-9).

#include "collision/grid_internal.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

using nnh::Rect;
using nnh::Vec2;
using nnh::collision::Cell;
using nnh::collision::Grid;
namespace collision = nnh::collision;
namespace detail = nnh::collision::detail;

namespace {

constexpr Cell E = Cell::Empty;
constexpr Cell S = Cell::Solid;
constexpr Cell O = Cell::OneWay;

// 3 x 2 cells.
constexpr Cell kCells[] = {
    E, S, O, //
    S, E, E, //
};

Cell diagonal(const void*, int x, int y) { return x == y ? Cell::Solid : Cell::Empty; }

bool range_is(detail::CellRange r, int x0, int y0, int x1, int y1) {
    return r.x0 == x0 && r.y0 == y0 && r.x1 == x1 && r.y1 == y1;
}

} // namespace

NNH_TEST(collision_grid_view_describes_a_row_major_array) {
    const Grid g = collision::grid_view(kCells, 3, 2, 16, 8, {4, 5});
    CHECK(g.width == 3);
    CHECK(g.height == 2);
    CHECK(g.cell_w == 16);
    CHECK(g.cell_h == 8);
    CHECK(g.origin == (Vec2{4, 5}));
    CHECK(g.outside == Cell::Empty);
    CHECK(detail::cell_at(g, 0, 0) == E);
    CHECK(detail::cell_at(g, 1, 0) == S);
    CHECK(detail::cell_at(g, 2, 0) == O);
    CHECK(detail::cell_at(g, 0, 1) == S);
    CHECK(detail::cell_at(g, 2, 1) == E);
}

NNH_TEST(collision_cells_beyond_the_edges_are_the_outside_kind) {
    Grid g = collision::grid_view(kCells, 3, 2, 16, 16);
    CHECK(detail::cell_at(g, -1, 0) == Cell::Empty);
    CHECK(detail::cell_at(g, 3, 0) == Cell::Empty);
    g.outside = Cell::Solid;
    CHECK(detail::cell_at(g, -1, 0) == Cell::Solid);
    CHECK(detail::cell_at(g, 3, 1) == Cell::Solid);
    CHECK(detail::cell_at(g, 0, -1) == Cell::Solid);
    CHECK(detail::cell_at(g, 0, 2) == Cell::Solid);
    CHECK(detail::cell_at(g, 0, 0) == Cell::Empty);
}

NNH_TEST(collision_grid_asks_its_function_for_cells_inside) {
    Grid g;
    g.width = 4;
    g.height = 4;
    g.cell = diagonal;
    g.outside = Cell::OneWay;
    CHECK(detail::cell_at(g, 2, 2) == Cell::Solid);
    CHECK(detail::cell_at(g, 2, 3) == Cell::Empty);
    CHECK(detail::cell_at(g, 4, 4) == Cell::OneWay);
}

NNH_TEST(collision_zero_size_grid_is_all_outside) {
    Grid g;
    g.outside = Cell::Solid;
    CHECK(detail::cell_at(g, 0, 0) == Cell::Solid);
    g.outside = Cell::Empty;
    CHECK(detail::cell_at(g, 0, 0) == Cell::Empty);
}

NNH_TEST(collision_cell_index_uses_floor_for_starts_and_half_open_ends) {
    CHECK(detail::first_cell(0, 16, 0) == 0);
    CHECK(detail::first_cell(0, 16, 15.99f) == 0);
    CHECK(detail::first_cell(0, 16, 16) == 1);
    CHECK(detail::first_cell(0, 16, -0.01f) == -1);
    CHECK(detail::first_cell(0, 16, -16) == -1);
    CHECK(detail::last_cell(0, 16, 16) == 0);
    CHECK(detail::last_cell(0, 16, 16.01f) == 1);
    CHECK(detail::last_cell(0, 16, 0) == -1);
    CHECK(detail::last_cell(0, 16, -16) == -2);
}

NNH_TEST(collision_cell_index_saturates_for_huge_values) {
    CHECK(detail::first_cell(0, 1, 1e30f) == detail::kMaxCellIndex);
    CHECK(detail::first_cell(0, 1, -1e30f) == -detail::kMaxCellIndex);
    CHECK(detail::last_cell(0, 1, 1e30f) == detail::kMaxCellIndex);
}

NNH_TEST(collision_cell_range_of_a_box_aligned_to_cells) {
    const Grid g = collision::grid_view(kCells, 3, 2, 16, 16);
    CHECK(range_is(detail::cell_range(g, {16, 0, 16, 16}), 1, 0, 1, 0));
    CHECK(range_is(detail::cell_range(g, {0, 0, 32, 32}), 0, 0, 1, 1));
}

NNH_TEST(collision_cell_range_includes_partly_covered_cells) {
    const Grid g = collision::grid_view(kCells, 3, 2, 16, 16);
    CHECK(range_is(detail::cell_range(g, {8, 4, 10, 20}), 0, 0, 1, 1));
}

NNH_TEST(collision_cell_range_honours_origin_and_non_square_cells) {
    const Grid g = collision::grid_view(kCells, 3, 2, 8, 32, {-100, 50});
    CHECK(range_is(detail::cell_range(g, {-100, 50, 8, 32}), 0, 0, 0, 0));
    CHECK(range_is(detail::cell_range(g, {-101, 49, 10, 34}), -1, -1, 1, 1));
}

NNH_TEST(collision_cell_range_of_an_empty_box_is_empty) {
    const Grid g = collision::grid_view(kCells, 3, 2, 16, 16);
    CHECK(detail::cell_range(g, {16, 16, 0, 10}).empty());
    CHECK(detail::cell_range(g, {16, 16, 10, 0}).empty());
    CHECK(detail::cell_range(g, {8, 8, 0, 4}).empty());
}
