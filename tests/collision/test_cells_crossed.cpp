// cells_crossed (spec 019 FR-11): the walk with kinds and entry fractions, the buffer contract, clipping.

#include "ascii_grid.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::Cell;
using collision::CellCrossing;
using nnh::Vec2;
using nnh::test::AsciiGrid;

namespace {

const char* const kRows[] = {
    "....",
    ".#=.",
    "....",
};

} // namespace

NNH_TEST(cells_crossed_lists_kinds_and_entry_fractions_in_order) {
    const AsciiGrid g(kRows, 3);
    CellCrossing cells[8];
    const int n = collision::cells_crossed(g.grid, {0, 24}, {64, 24}, cells, 8);
    CHECK(n == 4);
    CHECK(cells[0].x == 0 && cells[0].kind == Cell::Empty && cells[0].fraction == 0);
    CHECK(cells[1].x == 1 && cells[1].kind == Cell::Solid && cells[1].fraction == 0.25f);
    CHECK(cells[2].x == 2 && cells[2].kind == Cell::OneWay && cells[2].fraction == 0.5f);
    CHECK(cells[3].x == 3 && cells[3].y == 1 && cells[3].fraction == 0.75f);
}

NNH_TEST(cells_crossed_fractions_never_decrease) {
    const AsciiGrid g(kRows, 3);
    CellCrossing cells[32];
    const int n = collision::cells_crossed(g.grid, {1, 47}, {63, 1}, cells, 32);
    CHECK(n > 4);
    for (int i = 1; i < n; ++i) CHECK(cells[i - 1].fraction <= cells[i].fraction);
}

NNH_TEST(cells_crossed_starts_at_the_clip_fraction_from_outside) {
    const AsciiGrid g(kRows, 3);
    CellCrossing cells[8];
    const int n = collision::cells_crossed(g.grid, {-64, 8}, {64, 8}, cells, 8);
    CHECK(n == 4 && cells[0].x == 0 && cells[0].fraction == 0.5f);
}

NNH_TEST(cells_crossed_lists_outside_cells_when_the_outside_is_not_empty) {
    AsciiGrid g(kRows, 3);
    g.grid.outside = Cell::Solid;
    CellCrossing cells[8];
    const int n = collision::cells_crossed(g.grid, {-24, 8}, {24, 8}, cells, 8);
    CHECK(n == 4 && cells[0].x == -2 && cells[0].kind == Cell::Solid && cells[1].x == -1 && cells[2].x == 0);
    CHECK(cells[2].kind == Cell::Empty);
}

NNH_TEST(cells_crossed_counts_past_the_capacity_and_with_null) {
    const AsciiGrid g(kRows, 3);
    CellCrossing cells[2];
    CHECK(collision::cells_crossed(g.grid, {0, 24}, {64, 24}, cells, 2) == 4);
    CHECK(cells[1].x == 1);
    CHECK(collision::cells_crossed(g.grid, {0, 24}, {64, 24}, nullptr, 0) == 4);
}

NNH_TEST(a_segment_on_a_grid_line_or_of_zero_length_crosses_nothing) {
    const AsciiGrid g(kRows, 3);
    CellCrossing cells[8];
    CHECK(collision::cells_crossed(g.grid, {0, 16}, {64, 16}, cells, 8) == 0);
    CHECK(collision::cells_crossed(g.grid, {32, 0}, {32, 48}, cells, 8) == 0);
    CHECK(collision::cells_crossed(g.grid, {20, 20}, {20, 20}, cells, 8) == 0);
}
