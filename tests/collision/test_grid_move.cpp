// collision::move: x-then-y sweep through a grid, sliding, touching sides, no tunnelling (spec 012 FR-8).

#include "ascii_grid.hpp"
#include "crash/crash_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/collision.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

using nnh::Rect;
using nnh::Vec2;
using nnh::collision::Cell;
using nnh::collision::Grid;
using nnh::collision::MoveResult;
using nnh::test::AsciiGrid;
using nnh::test::no_sides;
namespace collision = nnh::collision;
namespace fake = nnh::platform_fake;

namespace {

const char* const kOpen[] = {"....", "....", "....", "...."};
const char* const kWallRight[] = {"....#", "....#", "....#", "....#", "....#", "....#"};
const char* const kWallLeft[] = {"#....", "#....", "#...."};
const char* const kFloor[] = {"....", "....", "####"};
const char* const kCeiling[] = {"####", "....", "...."};

Cell wall_at_700(const void*, int x, int) { return x == 700 ? Cell::Solid : Cell::Empty; }

} // namespace

NNH_TEST(collision_move_in_open_space_goes_the_whole_way) {
    const AsciiGrid level(kOpen, 4);
    const MoveResult r = collision::move(level.grid, {16, 16, 8, 8}, {5, -3});
    CHECK(r.position == (Vec2{21, 13}));
    CHECK(r.moved == (Vec2{5, -3}));
    CHECK(no_sides(r.touching));
}

NNH_TEST(collision_move_stops_flush_at_a_wall_on_the_right) {
    const AsciiGrid level(kWallRight, 6);
    const MoveResult r = collision::move(level.grid, {20, 4, 8, 8}, {50, 0});
    CHECK(r.position == (Vec2{56, 4}));
    CHECK(r.moved == (Vec2{36, 0}));
    CHECK(r.touching.right);
    CHECK(!r.touching.left && !r.touching.floor && !r.touching.ceiling);
}

NNH_TEST(collision_move_stops_flush_at_a_wall_on_the_left) {
    const AsciiGrid level(kWallLeft, 3);
    const MoveResult r = collision::move(level.grid, {40, 4, 8, 8}, {-50, 0});
    CHECK(r.position == (Vec2{16, 4}));
    CHECK(r.moved == (Vec2{-24, 0}));
    CHECK(r.touching.left);
    CHECK(!r.touching.right);
}

NNH_TEST(collision_move_lands_on_a_floor) {
    const AsciiGrid level(kFloor, 3);
    const MoveResult r = collision::move(level.grid, {4, 4, 8, 8}, {0, 40});
    CHECK(r.position == (Vec2{4, 24}));
    CHECK(r.moved == (Vec2{0, 20}));
    CHECK(r.touching.floor);
    CHECK(!r.touching.ceiling);
}

NNH_TEST(collision_move_bumps_a_ceiling) {
    const AsciiGrid level(kCeiling, 3);
    const MoveResult r = collision::move(level.grid, {4, 40, 8, 4}, {0, -40});
    CHECK(r.position == (Vec2{4, 16}));
    CHECK(r.moved == (Vec2{0, -24}));
    CHECK(r.touching.ceiling);
    CHECK(!r.touching.floor);
}

NNH_TEST(collision_move_slides_along_a_wall) {
    const AsciiGrid level(kWallRight, 6);
    const MoveResult r = collision::move(level.grid, {20, 4, 8, 8}, {50, 20});
    CHECK(r.position == (Vec2{56, 24}));
    CHECK(r.moved == (Vec2{36, 20}));
    CHECK(r.touching.right);
    CHECK(!r.touching.floor);
}

NNH_TEST(collision_move_slides_along_a_floor) {
    const AsciiGrid level(kFloor, 3);
    const MoveResult r = collision::move(level.grid, {4, 24, 8, 8}, {10, 5});
    CHECK(r.position == (Vec2{14, 24}));
    CHECK(r.moved == (Vec2{10, 0}));
    CHECK(r.touching.floor);
}

NNH_TEST(collision_move_diagonal_into_a_corner_cell_lands_on_it) {
    const char* const rows[] = {"....", "....", "..#.", "...."};
    const AsciiGrid level(rows, 4);
    // x goes first and is free (the corner cell is not in the box's rows), then y lands on the cell.
    const MoveResult r = collision::move(level.grid, {16, 16, 16, 16}, {16, 16});
    CHECK(r.position == (Vec2{32, 16}));
    CHECK(r.touching.floor);
    CHECK(!r.touching.right);
}

NNH_TEST(collision_move_into_an_inside_corner_touches_both_sides) {
    const char* const rows[] = {"....#", "....#", "#####"};
    const AsciiGrid level(rows, 3);
    const MoveResult r = collision::move(level.grid, {40, 8, 8, 8}, {30, 30});
    CHECK(r.position == (Vec2{56, 24}));
    CHECK(r.touching.right && r.touching.floor);
}

NNH_TEST(collision_move_never_tunnels_through_a_thin_wall) {
    const char* const rows[] = {"..#.....", "..#....."};
    const AsciiGrid level(rows, 2);
    const MoveResult r = collision::move(level.grid, {0, 0, 8, 8}, {1000, 0});
    CHECK(r.position == (Vec2{24, 0}));
    CHECK(r.touching.right);
}

NNH_TEST(collision_move_scans_every_cell_of_a_huge_delta) {
    Grid g;
    g.width = 1000;
    g.height = 4;
    g.cell = wall_at_700;
    const MoveResult r = collision::move(g, {0, 0, 8, 8}, {20000, 0});
    CHECK(r.position == (Vec2{700 * 16 - 8, 0}));
    CHECK(r.touching.right);
}

NNH_TEST(collision_move_huge_delta_stops_at_solid_outside) {
    AsciiGrid level(kOpen, 4);
    level.grid.outside = Cell::Solid;
    const MoveResult r = collision::move(level.grid, {4, 4, 8, 8}, {10000, -10000});
    CHECK(r.position == (Vec2{56, 0}));
    CHECK(r.touching.right && r.touching.ceiling);
}

NNH_TEST(collision_move_huge_delta_leaves_through_empty_outside) {
    const AsciiGrid level(kOpen, 4);
    const MoveResult r = collision::move(level.grid, {4, 4, 8, 8}, {-1e7f, 1e7f});
    CHECK(r.moved == (Vec2{-1e7f, 1e7f}));
    CHECK(no_sides(r.touching));
}

NNH_TEST(collision_move_box_larger_than_a_cell_hits_a_cell_under_its_middle) {
    const char* const rows[] = {"......", "......", "......", "..#..."};
    const AsciiGrid level(rows, 4);
    const MoveResult r = collision::move(level.grid, {8, 0, 40, 40}, {0, 20});
    CHECK(r.position == (Vec2{8, 8}));
    CHECK(r.touching.floor);
}

NNH_TEST(collision_move_box_starting_inside_solid_can_move_out) {
    const char* const rows[] = {"....", ".#..", "...."};
    const AsciiGrid level(rows, 3);
    const Rect inside{20, 20, 8, 8};
    CHECK(collision::move(level.grid, inside, {0, 20}).position == (Vec2{20, 40}));
    CHECK(collision::move(level.grid, inside, {-20, 0}).position == (Vec2{0, 20}));
    CHECK(collision::move(level.grid, inside, {20, -20}).position == (Vec2{40, 0}));
    CHECK(no_sides(collision::move(level.grid, inside, {20, -20}).touching));
}

NNH_TEST(collision_move_ignores_only_cells_the_box_already_overlaps) {
    const char* const rows[] = {"###.", "###.", "###."};
    const AsciiGrid level(rows, 3);
    // Inside column 1 of a thick wall: column 2 is new to the box, so it stops there.
    const MoveResult r = collision::move(level.grid, {20, 20, 8, 8}, {30, 0});
    CHECK(r.position == (Vec2{24, 20}));
    CHECK(r.touching.right);
}

NNH_TEST(collision_move_flush_against_a_wall_cannot_go_further) {
    const char* const rows[] = {"..#.", "..#."};
    const AsciiGrid level(rows, 2);
    const MoveResult r = collision::move(level.grid, {16, 0, 16, 16}, {1, 0});
    CHECK(r.position == (Vec2{16, 0}));
    CHECK(r.moved == (Vec2{0, 0}));
    CHECK(r.touching.right);
}

NNH_TEST(collision_move_away_from_a_touching_wall_is_free) {
    const char* const rows[] = {"..#.", "..#."};
    const AsciiGrid level(rows, 2);
    const MoveResult r = collision::move(level.grid, {16, 0, 16, 16}, {-5, 0});
    CHECK(r.position == (Vec2{11, 0}));
    CHECK(no_sides(r.touching));
}

NNH_TEST(collision_move_respects_exact_float_edges) {
    const char* const rows[] = {"..#.", "..#."};
    const AsciiGrid level(rows, 2);
    // Right edge exactly 32: flush, blocked at once.
    CHECK(collision::move(level.grid, {31.5f, 0, 0.5f, 8}, {3, 0}).moved == (Vec2{0, 0}));
    // Right edge just short of 32: moves the rest of the way.
    const MoveResult r = collision::move(level.grid, {31.25f, 0, 0.5f, 8}, {3, 0});
    CHECK(r.position == (Vec2{31.5f, 0}));
    CHECK(r.touching.right);
}

NNH_TEST(collision_move_with_zero_delta_touches_nothing) {
    const AsciiGrid level(kFloor, 3);
    const MoveResult r = collision::move(level.grid, {4, 24, 8, 8}, {});
    CHECK(r.position == (Vec2{4, 24}));
    CHECK(no_sides(r.touching));
}

NNH_TEST(collision_move_honours_a_negative_origin) {
    AsciiGrid level(kFloor, 3, 16, 16, {-100, -60});
    level.grid.outside = Cell::Solid;
    const MoveResult r = collision::move(level.grid, {-90, -56, 8, 8}, {-50, 100});
    CHECK(r.position == (Vec2{-100, -36}));
    CHECK(r.touching.left && r.touching.floor);
}

NNH_TEST(collision_move_honours_non_square_cells) {
    const char* const rows[] = {"....#", "....#", "#####"};
    const AsciiGrid level(rows, 3, 8, 32);
    const MoveResult r = collision::move(level.grid, {4, 4, 8, 8}, {100, 100});
    CHECK(r.position == (Vec2{24, 56}));
    CHECK(r.touching.right && r.touching.floor);
}

NNH_TEST(collision_move_solid_outside_walls_the_level_in) {
    AsciiGrid level(kOpen, 4);
    level.grid.outside = Cell::Solid;
    const MoveResult r = collision::move(level.grid, {4, 4, 8, 8}, {-20, -20});
    CHECK(r.position == (Vec2{0, 0}));
    CHECK(r.touching.left && r.touching.ceiling);
}

NNH_TEST(collision_move_on_a_zero_size_grid_sees_only_the_outside) {
    Grid g;
    CHECK(collision::move(g, {0, 0, 8, 8}, {1000, 1000}).moved == (Vec2{1000, 1000}));
    g.outside = Cell::Solid;
    // The box's own cell is ignored; the next column is solid.
    CHECK(collision::move(g, {0, 0, 8, 8}, {5, 0}).moved == (Vec2{5, 0}));
    const MoveResult r = collision::move(g, {0, 0, 8, 8}, {20, 0});
    CHECK(r.position == (Vec2{8, 0}));
    CHECK(r.touching.right);
}

NNH_TEST(collision_move_with_fractional_values_never_sinks_into_the_floor) {
    // Origin and sizes that are not exact in binary: landing must leave the box outside the floor row,
    // so the next frames keep it standing instead of letting it fall through.
    const char* const rows[] = {"......", "......", "......", "######"};
    const AsciiGrid level(rows, 4, 16.1f, 16.1f, {0.1f, 0.3f});
    Rect box{3.3f, 0.7f, 13.7f, 13.7f};
    for (int frame = 0; frame < 120; ++frame) {
        const MoveResult r = collision::move(level.grid, box, {0.2f, 3.3f});
        box.x = r.position.x;
        box.y = r.position.y;
    }
    const float floor_top = 0.3f + 3 * 16.1f;
    CHECK(box.y + box.h <= floor_top);
    CHECK(box.y + box.h > floor_top - 0.01f);
    const MoveResult r = collision::move(level.grid, box, {0, 3.3f});
    CHECK(r.touching.floor);
    CHECK(r.position.y == box.y);
}

namespace {
void move_by_nan() { collision::move({}, {0, 0, 8, 8}, {0.0f / 0.0f, 0}); }
void move_by_infinity() { collision::move({}, {0, 0, 8, 8}, {0, 1.0f / 0.0f}); }
} // namespace

NNH_TEST(collision_move_asserts_on_a_non_finite_delta) {
    fake::reset();
    nnh::detail::crash_install();
    CHECK(fake::run_expecting_crash(move_by_nan) == nnh::build::kAssertsEnabled);
    CHECK(fake::run_expecting_crash(move_by_infinity) == nnh::build::kAssertsEnabled);
}

// Float inputs (found by search) where `wall - w + w` rounds past the wall, or `(edge - origin) / size`
// rounds below the cell index: the stop position must still leave the wall line outside the box.

NNH_TEST(collision_move_stop_on_the_right_survives_rounding) {
    const char* const rows[] = {"..#.", "..#."};
    const AsciiGrid level(rows, 2, 38.8f, 16, {47.7f, 0});
    const MoveResult r = collision::move(level.grid, {50, 0, 13.5f, 8}, {200, 0});
    CHECK(r.touching.right);
    const MoveResult again = collision::move(level.grid, {r.position.x, 0, 13.5f, 8}, {1, 0});
    CHECK(again.touching.right);
    CHECK(again.moved == (Vec2{0, 0}));
}

NNH_TEST(collision_move_stop_on_the_left_survives_rounding) {
    const char* const rows[] = {"...................................#...."};
    const AsciiGrid level(rows, 1, 5.1f, 16, {40.1f, 0});
    const MoveResult r = collision::move(level.grid, {40.1f + 38 * 5.1f, 0, 2, 8}, {-50, 0});
    CHECK(r.touching.left);
    const MoveResult again = collision::move(level.grid, {r.position.x, 0, 2, 8}, {-1, 0});
    CHECK(again.touching.left);
    CHECK(again.moved == (Vec2{0, 0}));
}

NNH_TEST(collision_move_never_goes_backwards_when_rounding_disagrees) {
    const char* const rows[] = {"........................#.."};
    const AsciiGrid level(rows, 1, 10.2f, 16, {-8.6f, 0});
    const MoveResult r = collision::move(level.grid, {219.3f, 0, 16.9f, 8}, {5, 0});
    CHECK(r.touching.right);
    CHECK(r.position.x == 219.3f);
    CHECK(r.moved.x == 0);
}
