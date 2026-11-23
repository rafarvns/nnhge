// Rays against a grid (spec 019 FR-8, FR-9): solid, one-way, inside starts, outside kinds, points, normals, cost.

#include <cstdint>

#include "ascii_grid.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::Cell;
using collision::RayHit;
using nnh::Vec2;
using nnh::test::AsciiGrid;

namespace {

// 16 x 16 cells.
const char* const kRows[] = {
    "........",
    "..#.....",
    "....=...",
    ".#......",
    "......#.",
};

} // namespace

NNH_TEST(a_ray_stops_at_the_first_solid_cell_flush_with_its_face) {
    const AsciiGrid g(kRows, 5);
    RayHit hit;
    CHECK(collision::raycast(g.grid, {0, 24}, {128, 24}, hit));
    CHECK(hit.box == -1 && hit.cell.x == 2 && hit.cell.y == 1 && hit.cell.kind == Cell::Solid);
    CHECK(hit.point == Vec2(32, 24) && hit.normal == Vec2(-1, 0) && hit.fraction == 0.25f && hit.distance == 32);
    CHECK(collision::raycast(g.grid, {120, 56}, {0, 56}, hit)); // from the right: (1, 3)'s right face
    CHECK(hit.cell.x == 1 && hit.point == Vec2(32, 56) && hit.normal == Vec2(1, 0));
    CHECK(collision::raycast(g.grid, {40, 0}, {40, 79}, hit)); // down onto (2, 1)'s top
    CHECK(hit.point == Vec2(40, 16) && hit.normal == Vec2(0, -1));
    CHECK(!collision::raycast(g.grid, {104, 79}, {104, 0}, hit)); // up: starts inside (6, 4), ignored; then clear
}

NNH_TEST(a_start_inside_a_solid_cell_is_ignored_unless_asked) {
    const AsciiGrid g(kRows, 5);
    RayHit hit;
    CHECK(!collision::raycast(g.grid, {40, 24}, {120, 24}, hit));
    CHECK(collision::raycast(g.grid, {40, 24}, {120, 24}, hit, {.hit_from_inside = true}));
    CHECK(hit.inside && hit.fraction == 0 && hit.point == Vec2(40, 24) && hit.normal == Vec2(0, 0));
    CHECK(hit.cell.x == 2 && hit.cell.y == 1);
}

NNH_TEST(a_ray_starting_on_a_solid_face_going_in_hits_at_zero) {
    const AsciiGrid g(kRows, 5);
    RayHit hit;
    CHECK(collision::raycast(g.grid, {32, 24}, {60, 24}, hit));
    CHECK(hit.fraction == 0 && hit.normal == Vec2(-1, 0) && !hit.inside && hit.point == Vec2(32, 24));
    CHECK(!collision::raycast(g.grid, {48, 24}, {60, 24}, hit)); // on its right face going out
}

NNH_TEST(one_way_cells_block_only_from_above_going_down) {
    const AsciiGrid g(kRows, 5);
    RayHit hit;
    CHECK(collision::raycast(g.grid, {72, 0}, {72, 60}, hit)); // falls onto (4, 2)
    CHECK(hit.cell.kind == Cell::OneWay && hit.point == Vec2(72, 32) && hit.normal == Vec2(0, -1));
    CHECK(!collision::raycast(g.grid, {72, 47}, {72, 1}, hit)); // from below, going up
    CHECK(!collision::raycast(g.grid, {50, 40}, {90, 40}, hit)); // from the side
    CHECK(collision::raycast(g.grid, {60, 20}, {84, 44}, hit)); // slanted, through its top
    CHECK(hit.cell.kind == Cell::OneWay && hit.normal == Vec2(0, -1) && hit.point == Vec2(72, 32));
    CHECK(!collision::raycast(g.grid, {72, 0}, {72, 60}, hit, {.ignore_one_way = true}));
}

NNH_TEST(a_one_way_cell_entered_through_its_corner_going_down_blocks) {
    const AsciiGrid g(kRows, 5);
    RayHit hit;
    CHECK(collision::raycast(g.grid, {56, 24}, {72, 40}, hit)); // through (64, 32), its top-left corner
    CHECK(hit.cell.x == 4 && hit.cell.y == 2 && hit.normal == Vec2(0, -1) && hit.point == Vec2(64, 32));
    CHECK(!collision::raycast(g.grid, {88, 40}, {72, 24}, hit)); // its top-right corner going up: no
}

NNH_TEST(a_start_inside_a_one_way_cell_never_blocks) {
    const AsciiGrid g(kRows, 5);
    RayHit hit;
    CHECK(!collision::raycast(g.grid, {72, 36}, {72, 46}, hit, {.hit_from_inside = true}));
}

NNH_TEST(a_ray_never_slips_between_two_diagonal_solid_cells) {
    const char* const rows[] = {
        "#.",
        ".#",
    };
    const AsciiGrid g(rows, 2);
    RayHit hit;
    // From (1, 0) to (0, 1) through the shared corner (16, 16): the cells beside it are both solid.
    CHECK(collision::raycast(g.grid, {24, 8}, {8, 24}, hit));
    CHECK(hit.point == Vec2(16, 16) && hit.fraction == 0.5f);
    CHECK(hit.normal == Vec2(1, 0) && hit.cell.x == 0 && hit.cell.y == 0); // the x-neighbour, its x face
    CHECK(!collision::line_of_sight(g.grid, {24, 8}, {8, 24}));
}

NNH_TEST(a_solid_diagonal_cell_reports_its_x_face_and_the_corner) {
    const char* const rows[] = {
        "..",
        ".#",
    };
    const AsciiGrid g(rows, 2);
    RayHit hit;
    CHECK(collision::raycast(g.grid, {4, 4}, {28, 28}, hit));
    CHECK(hit.cell.x == 1 && hit.cell.y == 1 && hit.normal == Vec2(-1, 0) && hit.point == Vec2(16, 16));
}

NNH_TEST(the_outside_blocks_when_solid_and_is_skipped_when_empty) {
    AsciiGrid g(kRows, 5);
    RayHit hit;
    CHECK(!collision::raycast(g.grid, {8, 8}, {-200, 8}, hit));
    g.grid.outside = Cell::Solid;
    CHECK(collision::raycast(g.grid, {8, 8}, {-200, 8}, hit));
    CHECK(hit.cell.x == -1 && hit.cell.kind == Cell::Solid && hit.point == Vec2(0, 8) && hit.normal == Vec2(1, 0));
    g.grid.outside = Cell::OneWay;
    CHECK(!collision::raycast(g.grid, {8, 8}, {-200, 8}, hit));
    CHECK(collision::raycast(g.grid, {8, 70}, {8, 200}, hit)); // falls out of the bottom onto the outside
    CHECK(hit.cell.y == 5 && hit.point == Vec2(8, 80) && hit.normal == Vec2(0, -1));
}

NNH_TEST(a_ray_from_far_outside_reaches_the_grid) {
    const AsciiGrid g(kRows, 5);
    RayHit hit;
    CHECK(collision::raycast(g.grid, {-10000, 24}, {10000, 24}, hit));
    CHECK(hit.cell.x == 2 && hit.point == Vec2(32, 24));
}

namespace {

struct Counter {
    const AsciiGrid* g;
    int calls = 0;
};

Cell counting_cell(const void* context, int x, int y) {
    auto* c = static_cast<Counter*>(const_cast<void*>(context));
    ++c->calls;
    return c->g->cells[y * c->g->grid.width + x];
}

} // namespace

NNH_TEST(one_lookup_per_visited_cell_and_none_outside) {
    const AsciiGrid g(kRows, 5);
    Counter counter{&g};
    collision::Grid grid = g.grid;
    grid.cells = nullptr;
    grid.cell = counting_cell;
    grid.context = &counter;
    RayHit hit;
    CHECK(collision::raycast(grid, {-1000, 24}, {124, 24}, hit) && hit.cell.x == 2);
    CHECK(counter.calls == 3); // cells (0..2, 1); nothing outside
    counter.calls = 0;
    collision::CellCrossing cells[16];
    const int n = collision::cells_crossed(grid, {1, 1}, {127, 79}, cells, 16);
    CHECK(counter.calls == n);
}

NNH_TEST(the_first_hit_is_the_first_blocking_cell_crossed_on_many_rays) {
    const char* const rows[] = {
        "..........",
        "..#...=...",
        "....#.....",
        ".=....##..",
        "......=...",
        "#.........",
        "...=...#..",
    };
    AsciiGrid g(rows, 7, 12, 20, {-5.5f, 3.25f});
    g.grid.outside = Cell::Solid;
    std::uint32_t seed = 7;
    const auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 16) / 65536.0f;
    };
    static collision::CellCrossing cells[512];
    int mismatches = 0, hits = 0;
    for (int ray = 0; ray < 5000; ++ray) {
        const Vec2 from{next() * 160 - 20, next() * 180 - 20};
        const Vec2 to{next() * 160 - 20, next() * 180 - 20};
        const int n = collision::cells_crossed(g.grid, from, to, cells, 512);
        int first = -1;
        for (int i = 0; i < n && first < 0; ++i) {
            const bool solid = cells[i].kind == Cell::Solid && !(i == 0 && cells[i].fraction == 0 &&
                                                                  collision::contains({-5.5f + cells[i].x * 12.0f,
                                                                                       3.25f + cells[i].y * 20.0f, 12, 20},
                                                                                      from));
            if (solid) first = i;
        }
        RayHit hit;
        const bool got = collision::raycast(g.grid, from, to, hit, {.ignore_one_way = true});
        hits += got ? 1 : 0;
        const bool same = got == (first >= 0) &&
                          (!got || (hit.cell.x == cells[first].x && hit.cell.y == cells[first].y &&
                                    hit.fraction == cells[first].fraction));
        mismatches += same ? 0 : 1;
    }
    CHECK(mismatches == 0 && hits > 1000);
}
