// One-way cells (solid only from above) and drop_through (spec 012 FR-7, FR-8).

#include "ascii_grid.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

using nnh::Vec2;
using nnh::collision::Cell;
using nnh::collision::MoveResult;
using nnh::test::AsciiGrid;
using nnh::test::no_sides;
namespace collision = nnh::collision;

namespace {

// A plank (row 2, top y = 32, x 32..64) above solid ground (row 4, top y = 64).
const char* const kPlank[] = {"......", "......", "..==..", "......", "######"};
constexpr collision::MoveOptions kDrop{.drop_through = true};

} // namespace

NNH_TEST(collision_one_way_lands_a_box_falling_from_above) {
    const AsciiGrid level(kPlank, 5);
    const MoveResult r = collision::move(level.grid, {36, 4, 8, 8}, {0, 40});
    CHECK(r.position == (Vec2{36, 24}));
    CHECK(r.touching.floor);
}

NNH_TEST(collision_one_way_holds_a_box_standing_exactly_on_its_top) {
    const AsciiGrid level(kPlank, 5);
    const MoveResult r = collision::move(level.grid, {36, 24, 8, 8}, {0, 2});
    CHECK(r.position == (Vec2{36, 24}));
    CHECK(r.moved == (Vec2{0, 0}));
    CHECK(r.touching.floor);
}

NNH_TEST(collision_one_way_lands_a_fast_fall_from_far_above) {
    const AsciiGrid level(kPlank, 5);
    const MoveResult r = collision::move(level.grid, {36, -500, 8, 8}, {0, 600});
    CHECK(r.position == (Vec2{36, 24}));
    CHECK(r.touching.floor);
}

NNH_TEST(collision_one_way_lets_a_box_jump_up_through_it) {
    const AsciiGrid level(kPlank, 5);
    const MoveResult r = collision::move(level.grid, {36, 50, 8, 8}, {0, -40});
    CHECK(r.position == (Vec2{36, 10}));
    CHECK(no_sides(r.touching));
}

NNH_TEST(collision_one_way_does_not_catch_a_box_whose_bottom_started_below_its_top) {
    const AsciiGrid level(kPlank, 5);
    // Half way up through the plank (bottom 36 > top 32): falls on through, then lands on the ground.
    const MoveResult r = collision::move(level.grid, {36, 28, 8, 8}, {0, 10});
    CHECK(r.position == (Vec2{36, 38}));
    CHECK(!r.touching.floor);
    CHECK(collision::move(level.grid, {36, 38, 8, 8}, {0, 100}).position == (Vec2{36, 56}));
}

NNH_TEST(collision_one_way_lets_a_box_pass_sideways) {
    const AsciiGrid level(kPlank, 5);
    MoveResult r = collision::move(level.grid, {8, 36, 8, 8}, {40, 0});
    CHECK(r.position == (Vec2{48, 36}));
    CHECK(no_sides(r.touching));
    r = collision::move(level.grid, {80, 36, 8, 8}, {-60, 0});
    CHECK(r.position == (Vec2{20, 36}));
    CHECK(no_sides(r.touching));
}

NNH_TEST(collision_drop_through_falls_through_one_way_cells) {
    const AsciiGrid level(kPlank, 5);
    const MoveResult r = collision::move(level.grid, {36, 24, 8, 8}, {0, 10}, kDrop);
    CHECK(r.position == (Vec2{36, 34}));
    CHECK(!r.touching.floor);
}

NNH_TEST(collision_drop_through_still_stops_on_solid_cells) {
    const AsciiGrid level(kPlank, 5);
    const MoveResult r = collision::move(level.grid, {36, 24, 8, 8}, {0, 100}, kDrop);
    CHECK(r.position == (Vec2{36, 56}));
    CHECK(r.touching.floor);
}

NNH_TEST(collision_one_way_outside_is_a_floor_under_the_grid_only) {
    const char* const rows[] = {"....", "...."};
    AsciiGrid level(rows, 2);
    level.grid.outside = Cell::OneWay;
    MoveResult r = collision::move(level.grid, {4, 16, 8, 8}, {0, 100});
    CHECK(r.position == (Vec2{4, 24}));
    CHECK(r.touching.floor);
    r = collision::move(level.grid, {4, 16, 8, 8}, {-100, -100});
    CHECK(r.position == (Vec2{-96, -84}));
    CHECK(no_sides(r.touching));
}
