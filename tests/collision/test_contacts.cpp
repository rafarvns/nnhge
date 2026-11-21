// collision::contacts: sides flush against blocking cells (spec 012 FR-8).

#include "ascii_grid.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

using nnh::collision::Cell;
using nnh::collision::Sides;
using nnh::test::AsciiGrid;
using nnh::test::no_sides;
namespace collision = nnh::collision;

namespace {

const char* const kRoom[] = {"#####", "#...#", "#...#", "#####"};
const char* const kPlank[] = {"......", "..==..", "......"};

} // namespace

NNH_TEST(collision_contacts_reports_the_floor_under_a_standing_box) {
    const AsciiGrid level(kRoom, 4);
    const Sides s = collision::contacts(level.grid, {20, 40, 8, 8});
    CHECK(s.floor);
    CHECK(!s.left && !s.right && !s.ceiling);
}

NNH_TEST(collision_contacts_reports_walls_and_ceiling_in_corners) {
    const AsciiGrid level(kRoom, 4);
    Sides s = collision::contacts(level.grid, {16, 16, 8, 8});
    CHECK(s.left && s.ceiling);
    CHECK(!s.right && !s.floor);
    s = collision::contacts(level.grid, {56, 40, 8, 8});
    CHECK(s.right && s.floor);
    CHECK(!s.left && !s.ceiling);
}

NNH_TEST(collision_contacts_ignores_a_box_one_pixel_away) {
    const AsciiGrid level(kRoom, 4);
    CHECK(no_sides(collision::contacts(level.grid, {20, 20, 8, 8})));
    CHECK(no_sides(collision::contacts(level.grid, {17, 17, 46, 30})));
}

NNH_TEST(collision_contacts_tolerates_a_tiny_float_gap) {
    const AsciiGrid level(kRoom, 4);
    CHECK(collision::contacts(level.grid, {20, 40 - 1.0f / 128, 8, 8}).floor);
    CHECK(!collision::contacts(level.grid, {20, 40 - 1.0f / 16, 8, 8}).floor);
}

NNH_TEST(collision_contacts_counts_one_way_only_as_floor) {
    const AsciiGrid level(kPlank, 3);
    CHECK(collision::contacts(level.grid, {36, 8, 8, 8}).floor);
    // Under the plank and beside it: no ceiling, no wall.
    CHECK(no_sides(collision::contacts(level.grid, {36, 32, 8, 8})));
    CHECK(no_sides(collision::contacts(level.grid, {24, 16, 8, 8})));
    CHECK(no_sides(collision::contacts(level.grid, {64, 16, 8, 8})));
}

NNH_TEST(collision_contacts_ignores_cells_the_box_overlaps) {
    const char* const rows[] = {"...", ".#.", "..."};
    const AsciiGrid level(rows, 3);
    CHECK(no_sides(collision::contacts(level.grid, {18, 18, 8, 8})));
}

NNH_TEST(collision_contacts_sees_a_solid_outside) {
    const char* const rows[] = {"...", "..."};
    AsciiGrid level(rows, 2);
    level.grid.outside = Cell::Solid;
    const Sides s = collision::contacts(level.grid, {0, 24, 8, 8});
    CHECK(s.left && s.floor);
    CHECK(!s.right && !s.ceiling);
}

NNH_TEST(collision_contacts_of_a_box_without_area_are_none) {
    const AsciiGrid level(kRoom, 4);
    CHECK(no_sides(collision::contacts(level.grid, {16, 16, 0, 0})));
}

NNH_TEST(collision_contacts_agree_with_a_landing_move) {
    const char* const rows[] = {"....", "....", "####"};
    const AsciiGrid level(rows, 3, 16.1f, 16.1f, {0.1f, 0.3f});
    const collision::MoveResult r = collision::move(level.grid, {3.3f, 0.7f, 13.7f, 13.7f}, {0, 50});
    CHECK(r.touching.floor);
    CHECK(collision::contacts(level.grid, {r.position.x, r.position.y, 13.7f, 13.7f}).floor);
}
