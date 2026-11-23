// raycast_all on a grid (spec 019 FR-11): a piercing laser, every blocking cell in order.

#include "ascii_grid.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::Cell;
using collision::RayHit;
using nnh::Vec2;
using nnh::test::AsciiGrid;

namespace {

const char* const kRows[] = {
    "........",
    ".#.=.##.",
    "........",
};

} // namespace

NNH_TEST(a_piercing_laser_reports_every_blocking_cell_in_order) {
    const AsciiGrid g(kRows, 3);
    RayHit hits[8];
    const int n = collision::raycast_all(g.grid, {0, 24}, {128, 24}, hits, 8);
    CHECK(n == 3); // the one-way cell is entered from the side: not blocking
    CHECK(hits[0].cell.x == 1 && hits[1].cell.x == 5 && hits[2].cell.x == 6);
    CHECK(hits[0].point == Vec2(16, 24) && hits[1].point == Vec2(80, 24) && hits[2].point == Vec2(96, 24));
    CHECK(hits[2].distance == 96 && hits[2].normal == Vec2(-1, 0));
    CHECK(hits[0].fraction < hits[1].fraction && hits[1].fraction < hits[2].fraction);
}

NNH_TEST(the_laser_keeps_the_nearest_cells_and_counts_the_rest) {
    const AsciiGrid g(kRows, 3);
    RayHit hits[2];
    CHECK(collision::raycast_all(g.grid, {128, 24}, {0, 24}, hits, 2) == 3);
    CHECK(hits[0].cell.x == 6 && hits[1].cell.x == 5);
    CHECK(collision::raycast_all(g.grid, {128, 24}, {0, 24}, nullptr, 0) == 3);
}

NNH_TEST(the_laser_includes_the_start_cell_only_when_asked) {
    const AsciiGrid g(kRows, 3);
    RayHit hits[8];
    CHECK(collision::raycast_all(g.grid, {84, 24}, {128, 24}, hits, 8) == 1 && hits[0].cell.x == 6);
    CHECK(collision::raycast_all(g.grid, {84, 24}, {128, 24}, hits, 8, {.hit_from_inside = true}) == 2);
    CHECK(hits[0].inside && hits[0].cell.x == 5 && hits[1].cell.x == 6);
}

NNH_TEST(a_falling_laser_stops_on_one_way_cells_unless_ignored) {
    const AsciiGrid g(kRows, 3);
    RayHit hits[4];
    CHECK(collision::raycast_all(g.grid, {56, 0}, {56, 48}, hits, 4) == 1 && hits[0].cell.kind == Cell::OneWay);
    CHECK(collision::raycast_all(g.grid, {56, 0}, {56, 48}, hits, 4, {.ignore_one_way = true}) == 0);
}
