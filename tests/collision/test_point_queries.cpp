// What is under a point, what overlaps a box (spec 019 FR-14, FR-15).

#include "ascii_grid.hpp"
#include "nnh/build.hpp"
#include "nnh/collision.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::Cell;
using collision::Layers;
using nnh::Rect;
using nnh::Vec2;
using nnh::test::AsciiGrid;

namespace {

// 0 [0,10)², 1 [5,15)², 2 a flat box, 3 far away, 4 [10,20)x[0,10) (touches 0 on its right edge).
const Rect kBoxes[5] = {{0, 0, 10, 10}, {5, 5, 10, 10}, {0, 0, 10, 0}, {100, 100, 4, 4}, {10, 0, 10, 10}};
const Layers kLayers[5] = {1u, 2u, 1u, 1u, 4u};

collision::BoxView view() {
    return collision::box_view(kBoxes, kLayers, 5);
}

} // namespace

NNH_TEST(boxes_at_lists_the_boxes_containing_the_point_in_ascending_order) {
    int out[4] = {-1, -1, -1, -1};
    CHECK(collision::boxes_at(view(), {7, 7}, out, 4) == 2);
    CHECK(out[0] == 0 && out[1] == 1);
    CHECK(collision::boxes_at(view(), {50, 50}, out, 4) == 0);
}

NNH_TEST(boxes_at_uses_half_open_edges) {
    int out[4];
    CHECK(collision::boxes_at(view(), {0, 0}, out, 4) == 1 && out[0] == 0);  // top-left corner is inside
    CHECK(collision::boxes_at(view(), {10, 3}, out, 4) == 1 && out[0] == 4); // 0's right edge is 4's left
    CHECK(collision::boxes_at(view(), {15, 15}, out, 4) == 0);               // 1's bottom-right corner is not
}

NNH_TEST(boxes_at_filters_by_mask_and_ignore) {
    int out[4];
    CHECK(collision::boxes_at(view(), {7, 7}, out, 4, {.mask = 2u}) == 1 && out[0] == 1);
    CHECK(collision::boxes_at(view(), {7, 7}, out, 4, {.ignore = 0}) == 1 && out[0] == 1);
    CHECK(collision::boxes_at(view(), {7, 7}, out, 4, {.mask = collision::kNoLayers}) == 0);
}

NNH_TEST(boxes_at_counts_past_the_capacity) {
    int out[1] = {-1};
    CHECK(collision::boxes_at(view(), {7, 7}, out, 1) == 2 && out[0] == 0);
    CHECK(collision::boxes_at(view(), {7, 7}, nullptr, 0) == 2);
}

NNH_TEST(boxes_overlapping_needs_shared_area) {
    int out[5] = {};
    CHECK(collision::boxes_overlapping(view(), {6, 6, 3, 3}, out, 5) == 2 && out[0] == 0 && out[1] == 1);
    CHECK(collision::boxes_overlapping(view(), {-4, 0, 4, 10}, out, 5) == 0); // touches box 0 only
    CHECK(collision::boxes_overlapping(view(), {0, 0, 0, 10}, out, 5) == 0);  // a box without area
    CHECK(collision::boxes_overlapping(view(), {9, 0, 2, 2}, out, 5) == 2 && out[0] == 0 && out[1] == 4);
}

NNH_TEST(boxes_overlapping_can_skip_the_querying_box_itself) {
    int out[5] = {};
    CHECK(collision::boxes_overlapping(view(), kBoxes[1], out, 5) == 3);
    CHECK(collision::boxes_overlapping(view(), kBoxes[1], out, 5, {.ignore = 1}) == 2 && out[0] == 0 &&
          out[1] == 4);
    CHECK(collision::boxes_overlapping(view(), kBoxes[1], out, 5, {.mask = 4u | 2u, .ignore = 1}) == 1 &&
          out[0] == 4);
}

NNH_TEST(an_empty_view_finds_nothing) {
    int out[1];
    CHECK(collision::boxes_at({}, {0, 0}, out, 1) == 0);
    CHECK(collision::boxes_overlapping({}, {0, 0, 1, 1}, out, 1) == 0);
}

NNH_TEST(cell_at_finds_the_cell_under_a_point) {
    const char* const rows[] = {"..#", ".=.", "..."};
    const AsciiGrid g(rows, 3, 16, 8, {-8, 4});
    const collision::Grid grid = g.grid;
    collision::CellHit c = collision::cell_at(grid, {24, 4}); // cell (2, 0): x 24 → (24 + 8) / 16 = 2
    CHECK(c.x == 2 && c.y == 0 && c.kind == Cell::Solid);
    c = collision::cell_at(grid, {8, 12}); // cell (1, 1): top-left corner of the one-way cell
    CHECK(c.x == 1 && c.y == 1 && c.kind == Cell::OneWay);
    c = collision::cell_at(grid, {7.99f, 11.99f}); // just before it: cell (0, 0)
    CHECK(c.x == 0 && c.y == 0 && c.kind == Cell::Empty);
}

NNH_TEST(cell_at_reports_outside_cells_beyond_the_edges) {
    const char* const rows[] = {".."};
    const AsciiGrid g(rows, 1);
    collision::Grid grid = g.grid;
    collision::CellHit c = collision::cell_at(grid, {-1, -1});
    CHECK(c.x == -1 && c.y == -1 && c.kind == Cell::Empty);
    grid.outside = Cell::Solid;
    c = collision::cell_at(grid, {40, 5});
    CHECK(c.x == 2 && c.y == 0 && c.kind == Cell::Solid);
    grid.outside = Cell::OneWay;
    CHECK(collision::cell_at(grid, {-100, 5}).kind == Cell::OneWay);
}

NNH_TEST(cell_at_saturates_huge_coordinates) {
    const char* const rows[] = {"."};
    const AsciiGrid g(rows, 1);
    const collision::Grid grid = g.grid;
    const collision::CellHit c = collision::cell_at(grid, {1e30f, -1e30f});
    CHECK(c.x == (1 << 30) && c.y == -(1 << 30));
}

namespace {

int g_out[2];
int g_total = 7;
collision::CellHit g_cell{3, 3, Cell::Solid};
constexpr float kNan = __builtin_nanf("");
void at_nan() {
    g_total = collision::boxes_at(collision::box_view(kBoxes, 5), {kNan, 0}, g_out, 2);
}
void at_bad_buffer() {
    g_total = collision::boxes_at(collision::box_view(kBoxes, 5), {0, 0}, nullptr, 2);
}
void at_bad_view() {
    g_total = collision::boxes_at({nullptr, 3}, {0, 0}, g_out, 2);
}
void overlapping_nan() {
    g_total = collision::boxes_overlapping(collision::box_view(kBoxes, 5), {0, 0, kNan, 1}, g_out, 2);
}
void overlapping_bad_capacity() {
    g_total = collision::boxes_overlapping(collision::box_view(kBoxes, 5), {0, 0, 1, 1}, g_out, -1);
}
const collision::Cell kSolid[1] = {Cell::Solid};
void cell_nan() {
    g_cell = collision::cell_at(collision::grid_view(kSolid, 1, 1, 16, 16), {kNan, 0});
}
void cell_bad_grid() {
    collision::Grid grid = collision::grid_view(kSolid, 1, 1, 16, 16);
    grid.cell_w = -1;
    g_cell = collision::cell_at(grid, {1, 1});
}

} // namespace

NNH_TEST(point_query_misuse_asserts_and_finds_nothing_in_release) {
    for (auto fn : {at_nan, at_bad_buffer, at_bad_view, overlapping_nan, overlapping_bad_capacity}) {
        g_total = 7;
        CHECK(nnh::platform_fake::run_expecting_crash(fn) == nnh::build::kAssertsEnabled);
        if (!nnh::build::kAssertsEnabled) CHECK(g_total == 0);
    }
    for (auto fn : {cell_nan, cell_bad_grid}) {
        g_cell = {3, 3, Cell::Solid};
        CHECK(nnh::platform_fake::run_expecting_crash(fn) == nnh::build::kAssertsEnabled);
        if (!nnh::build::kAssertsEnabled) CHECK(g_cell.x == 0 && g_cell.y == 0 && g_cell.kind == Cell::Empty);
    }
}
