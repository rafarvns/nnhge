// A grid and boxes together, and line of sight (spec 019 FR-12).

#include "ascii_grid.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::Layers;
using collision::RayHit;
using nnh::Rect;
using nnh::Vec2;
using nnh::test::AsciiGrid;

namespace {

const char* const kRows[] = {
    "........",
    "....#...",
    "........",
};

// 0: an enemy flush behind the wall (x 80..90), 1: an enemy before it (x 40..50), 2: the viewer (x 0..10).
const Rect kBoxes[3] = {{80, 16, 10, 16}, {40, 18, 10, 10}, {0, 18, 10, 10}};
const Layers kLayers[3] = {2u, 2u, 1u};

collision::BoxView boxes() { return collision::box_view(kBoxes, kLayers, 3); }

} // namespace

NNH_TEST(the_nearest_of_cells_and_boxes_wins) {
    const AsciiGrid g(kRows, 3);
    RayHit hit;
    CHECK(collision::raycast(g.grid, boxes(), {5, 24}, {120, 24}, hit));
    CHECK(hit.box == 1 && hit.point == Vec2(40, 24)); // the viewer's own box contains the start: ignored
    CHECK(collision::raycast(g.grid, boxes(), {60, 24}, {120, 24}, hit));
    CHECK(hit.box == -1 && hit.cell.x == 4 && hit.point == Vec2(64, 24));
    CHECK(collision::raycast(g.grid, boxes(), {72, 4}, {72, 12}, hit) == false);
}

NNH_TEST(a_cell_wins_a_tie_with_a_box_at_the_same_fraction) {
    const AsciiGrid g(kRows, 3);
    const Rect flush[1] = {{64, 16, 16, 16}}; // exactly over cell (4, 1)
    RayHit hit;
    CHECK(collision::raycast(g.grid, collision::box_view(flush, 1), {0, 24}, {120, 24}, hit));
    CHECK(hit.box == -1 && hit.cell.x == 4);
}

NNH_TEST(an_absent_grid_or_view_leaves_the_other) {
    const AsciiGrid g(kRows, 3);
    RayHit hit;
    CHECK(collision::raycast(collision::Grid{}, boxes(), {60, 24}, {120, 24}, hit) && hit.box == 0);
    CHECK(collision::raycast(g.grid, collision::BoxView{}, {5, 24}, {120, 24}, hit) && hit.cell.x == 4);
    CHECK(!collision::raycast(collision::Grid{}, collision::BoxView{}, {5, 24}, {120, 24}, hit));
}

NNH_TEST(the_mask_filters_boxes_only) {
    const AsciiGrid g(kRows, 3);
    RayHit hit;
    CHECK(collision::raycast(g.grid, boxes(), {5, 24}, {120, 24}, hit, {.mask = 1u}) && hit.cell.x == 4);
}

NNH_TEST(line_of_sight_is_clear_until_a_cell_or_a_box_is_in_the_way) {
    const AsciiGrid g(kRows, 3);
    CHECK(collision::line_of_sight(g.grid, boxes(), {5, 8}, {120, 8}));          // above everything
    CHECK(!collision::line_of_sight(g.grid, boxes(), {60, 24}, {120, 24}));      // the wall
    CHECK(!collision::line_of_sight(g.grid, boxes(), {5, 24}, {60, 24}));        // enemy 1
    CHECK(collision::line_of_sight(g.grid, boxes(), {5, 24}, {60, 24}, {.mask = 1u})); // the viewer only
    CHECK(collision::line_of_sight(g.grid, {5, 24}, {60, 24}));                  // grid only
    CHECK(!collision::line_of_sight(g.grid, {5, 24}, {100, 24}));
}

NNH_TEST(line_of_sight_never_counts_what_contains_the_viewer) {
    const AsciiGrid g(kRows, 3);
    CHECK(collision::line_of_sight(g.grid, boxes(), {5, 24}, {35, 24}, {.hit_from_inside = true}));
    CHECK(collision::line_of_sight(g.grid, boxes(), {5, 24}, {35, 24}, {.ignore = 2}));
    CHECK(collision::line_of_sight(g.grid, {70, 24}, {70, 40}, {.hit_from_inside = true})); // from inside the wall
}

NNH_TEST(line_of_sight_can_look_over_one_way_cells) {
    const char* const rows[] = {"..", "==", ".."};
    const AsciiGrid g(rows, 3);
    CHECK(!collision::line_of_sight(g.grid, {8, 4}, {8, 44}));
    CHECK(collision::line_of_sight(g.grid, {8, 4}, {8, 44}, {.ignore_one_way = true}));
    CHECK(collision::line_of_sight(g.grid, {8, 44}, {8, 4})); // looking up through them
}
