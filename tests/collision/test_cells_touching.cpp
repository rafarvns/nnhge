// collision::cells_touching: the non-empty cells a box overlaps (spec 012 FR-9).

#include "ascii_grid.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

using nnh::collision::Cell;
using nnh::collision::CellHit;
using nnh::test::AsciiGrid;
namespace collision = nnh::collision;

namespace {

const char* const kRows[] = {"#.=.", "..#.", "...."};

bool hit_is(const CellHit& h, int x, int y, Cell kind) { return h.x == x && h.y == y && h.kind == kind; }

} // namespace

NNH_TEST(collision_cells_touching_lists_non_empty_cells_in_row_order) {
    const AsciiGrid level(kRows, 3);
    CellHit hits[8];
    const int n = collision::cells_touching(level.grid, {8, 8, 40, 16}, hits, 8);
    CHECK(n == 3);
    CHECK(hit_is(hits[0], 0, 0, Cell::Solid));
    CHECK(hit_is(hits[1], 2, 0, Cell::OneWay));
    CHECK(hit_is(hits[2], 2, 1, Cell::Solid));
}

NNH_TEST(collision_cells_touching_leaves_out_flush_neighbours) {
    const AsciiGrid level(kRows, 3);
    CellHit hits[4];
    // Covers exactly cell (1, 0): the solid (0, 0), one-way (2, 0) and solid (2, 1) only touch its edges.
    CHECK(collision::cells_touching(level.grid, {16, 0, 16, 16}, hits, 4) == 0);
}

NNH_TEST(collision_cells_touching_returns_the_total_beyond_capacity) {
    const AsciiGrid level(kRows, 3);
    CellHit hits[2] = {{-9, -9, Cell::Empty}, {-9, -9, Cell::Empty}};
    CHECK(collision::cells_touching(level.grid, {0, 0, 64, 48}, hits, 1) == 3);
    CHECK(hit_is(hits[0], 0, 0, Cell::Solid));
    CHECK(hit_is(hits[1], -9, -9, Cell::Empty));
    CHECK(collision::cells_touching(level.grid, {0, 0, 64, 48}, nullptr, 0) == 3);
}

NNH_TEST(collision_cells_touching_includes_non_empty_outside_cells) {
    AsciiGrid level(kRows, 3);
    level.grid.outside = Cell::Solid;
    CellHit hits[4];
    const int n = collision::cells_touching(level.grid, {-8, 40, 16, 16}, hits, 4);
    CHECK(n == 3);
    CHECK(hit_is(hits[0], -1, 2, Cell::Solid));
    CHECK(hit_is(hits[1], -1, 3, Cell::Solid));
    CHECK(hit_is(hits[2], 0, 3, Cell::Solid));
}

NNH_TEST(collision_cells_touching_skips_an_empty_outside_cheaply) {
    const AsciiGrid level(kRows, 3);
    CHECK(collision::cells_touching(level.grid, {-1e9f, -1e9f, 2e9f, 2e9f}, nullptr, 0) == 3);
}

NNH_TEST(collision_cells_touching_honours_a_negative_origin) {
    const AsciiGrid level(kRows, 3, 16, 16, {-64, -48});
    CellHit hits[2];
    CHECK(collision::cells_touching(level.grid, {-32, -32, 4, 4}, hits, 2) == 1);
    CHECK(hit_is(hits[0], 2, 1, Cell::Solid));
}

NNH_TEST(collision_cells_touching_of_a_box_without_area_is_zero) {
    const AsciiGrid level(kRows, 3);
    CHECK(collision::cells_touching(level.grid, {0, 0, 0, 16}, nullptr, 0) == 0);
}
