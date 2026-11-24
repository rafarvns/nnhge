// The smoothing walk (spec 025 FR-15; T4.1): the integer centre-to-centre walk visits exactly the cells
// collision::cells_crossed visits on a 16-px grid, in the same order, with the same corners (a corner lists the
// x-neighbour, the y-neighbour, then the diagonal cell, all at one fraction).

#include <vector>

#include "nnh/collision.hpp"
#include "path_fixture.hpp"
#include "pathfinding/smooth_walk.hpp"

using nnh::CellPos;
using nnh::detail::SmoothWalk;
using nnh::detail::WalkStep;

namespace {

/// The walk as cells_crossed would list it: the start, then per step its side cells (corners) and the cell.
struct Listed {
    int x, y;
    bool side;
};

std::vector<Listed> walk_list(CellPos a, CellPos b) {
    std::vector<Listed> v{{a.x, a.y, false}};
    SmoothWalk walk(a, b);
    WalkStep s;
    while (walk.next(s)) {
        if (s.corner) {
            v.push_back({s.x, s.y - s.sy, true}); // the x-neighbour of the cell before
            v.push_back({s.x - s.sx, s.y, true}); // its y-neighbour
        }
        v.push_back({s.x, s.y, false});
    }
    return v;
}

bool equals_cells_crossed(CellPos a, CellPos b) {
    nnh::collision::Grid g; // 16-px cells, no source: every cell is `outside` (Empty)
    g.width = 64;
    g.height = 64;
    const nnh::Vec2 from{static_cast<float>(16 * a.x + 8), static_cast<float>(16 * a.y + 8)};
    const nnh::Vec2 to{static_cast<float>(16 * b.x + 8), static_cast<float>(16 * b.y + 8)};
    nnh::collision::CellCrossing crossed[300];
    const int n = nnh::collision::cells_crossed(g, from, to, crossed, 300);
    const std::vector<Listed> mine = walk_list(a, b);
    if (a == b) return mine.size() == 1; // a zero-length segment: cells_crossed lists nothing
    if (n != static_cast<int>(mine.size())) return false;
    for (int i = 0; i < n; ++i) {
        const Listed& m = mine[static_cast<std::size_t>(i)];
        if (crossed[i].x != m.x || crossed[i].y != m.y) return false;
        // A side cell is entered at the same fraction as the next cell (a corner).
        if (m.side && i + 1 < n && crossed[i].fraction != crossed[i + 1].fraction) return false;
        if (!m.side && i > 0 && !mine[static_cast<std::size_t>(i - 1)].side && crossed[i].fraction == crossed[i - 1].fraction) {
            return false;
        }
    }
    return true;
}

} // namespace

NNH_TEST(path_smooth_walk_equals_cells_crossed_for_20000_seeded_centre_pairs) {
    int bad = 0;
    for (unsigned i = 0; i < 20000; ++i) {
        const unsigned h = path_test::hash4(i, 5, 0, 41);
        const unsigned side = i % 4 == 0 ? 64 : 9; // many short segments (corners are common), some long
        const CellPos a{static_cast<int>(h % side), static_cast<int>((h >> 8) % side)};
        const CellPos b{static_cast<int>((h >> 16) % side), static_cast<int>((h >> 24) % side)};
        if (!equals_cells_crossed(a, b)) ++bad;
    }
    CHECK(bad == 0);
}

NNH_TEST(path_smooth_walk_horizontal_vertical_and_45_degrees) {
    CHECK(equals_cells_crossed({2, 3}, {9, 3}));
    CHECK(equals_cells_crossed({9, 3}, {2, 3}));
    CHECK(equals_cells_crossed({4, 1}, {4, 20}));
    CHECK(equals_cells_crossed({4, 20}, {4, 1}));
    CHECK(equals_cells_crossed({0, 0}, {10, 10}));
    CHECK(equals_cells_crossed({10, 0}, {0, 10}));
    CHECK(equals_cells_crossed({3, 3}, {9, 6})); // through corners every other step
    const std::vector<Listed> diag = walk_list({0, 0}, {2, 2});
    CHECK(diag.size() == 7 && diag[1].side && diag[2].side && !diag[3].side && diag[3].x == 1 && diag[3].y == 1);
}
