// The exact grid walk (spec 019 FR-9, T3.1): directions, edges, faces, corners, clipping, long rays, and the
// walk equal to an exact integer reference cell for cell on 5000 seeded rays.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

#include "collision/grid_walk.hpp"
#include "nnh/collision.hpp"
#include "ray_reference.hpp"
#include "test.hpp"

namespace collision = nnh::collision;
using collision::CellCrossing;
using collision::detail::GridWalk;
using collision::detail::make_segment;
using collision::detail::WalkCell;
using nnh::Vec2;

namespace {

/// A grid of `w` x `h` empty cells (16 x 16 at the origin unless told otherwise); `outside` Empty clips.
collision::Grid empty_grid(int w, int h, float cw = 16, float ch = 16, Vec2 origin = {},
                           collision::Cell outside = collision::Cell::Empty) {
    collision::Grid g;
    g.origin = origin;
    g.cell_w = cw;
    g.cell_h = ch;
    g.width = w;
    g.height = h;
    g.outside = outside;
    g.cell = [](const void*, int, int) { return collision::Cell::Empty; };
    return g;
}

struct Visit {
    int x, y;
};

std::vector<WalkCell> walk(const collision::Grid& g, Vec2 from, Vec2 to, bool clip = true) {
    std::vector<WalkCell> out;
    GridWalk w(g, make_segment(from, to), clip);
    WalkCell c;
    while (w.next(c) && out.size() < 100000) out.push_back(c);
    return out;
}

bool cells_are(const std::vector<WalkCell>& got, std::initializer_list<Visit> want) {
    if (got.size() != want.size()) return false;
    int i = 0;
    for (const Visit& v : want) {
        if (got[i].x != v.x || got[i].y != v.y) return false;
        ++i;
    }
    return true;
}

} // namespace

NNH_TEST(the_walk_goes_along_each_axis_both_ways) {
    const collision::Grid g = empty_grid(4, 4);
    CHECK(cells_are(walk(g, {8, 8}, {56, 8}), {{0, 0}, {1, 0}, {2, 0}, {3, 0}}));
    CHECK(cells_are(walk(g, {56, 24}, {8, 24}), {{3, 1}, {2, 1}, {1, 1}, {0, 1}}));
    CHECK(cells_are(walk(g, {40, 8}, {40, 56}), {{2, 0}, {2, 1}, {2, 2}, {2, 3}}));
    CHECK(cells_are(walk(g, {40, 56}, {40, 8}), {{2, 3}, {2, 2}, {2, 1}, {2, 0}}));
}

NNH_TEST(the_walk_covers_the_eight_octants) {
    const collision::Grid g = empty_grid(8, 8);
    const Vec2 c{64, 64};
    // Shallow and steep in each quadrant, from (66, 66): the last cell and how many cells.
    struct Case {
        Vec2 to;
        Visit last;
        size_t count;
    } cases[] = {
        {{100, 74}, {6, 4}, 3}, {{74, 100}, {4, 6}, 3}, {{28, 74}, {1, 4}, 4}, {{54, 100}, {3, 6}, 4},
        {{28, 54}, {1, 3}, 5},  {{54, 28}, {3, 1}, 5},  {{100, 54}, {6, 3}, 4}, {{74, 28}, {4, 1}, 4},
    };
    for (const Case& k : cases) {
        const auto cells = walk(g, {c.x + 2, c.y + 2}, k.to); // start inside cell (4, 4)
        CHECK(!cells.empty() && cells.front().x == 4 && cells.front().y == 4 && cells.front().inside);
        CHECK(cells.size() == k.count && cells.back().x == k.last.x && cells.back().y == k.last.y);
        for (size_t i = 1; i < cells.size(); ++i) {
            const int step = std::abs(cells[i].x - cells[i - 1].x) + std::abs(cells[i].y - cells[i - 1].y);
            CHECK(step == 1); // no corners here: neighbours only
            CHECK(collision::detail::compare(cells[i - 1].t, cells[i].t) < 0);
        }
    }
}

NNH_TEST(a_segment_on_a_grid_line_visits_nothing) {
    const collision::Grid g = empty_grid(4, 4);
    CHECK(walk(g, {16, 0}, {16, 64}).empty());
    CHECK(walk(g, {0, 32}, {64, 32}).empty());
    CHECK(walk(g, {64, 32}, {0, 32}, false).empty());
}

NNH_TEST(a_start_on_an_edge_enters_the_cell_ahead_through_that_face) {
    const collision::Grid g = empty_grid(4, 4);
    auto cells = walk(g, {16, 8}, {40, 8}); // on the line between columns 0 and 1, going right
    CHECK(cells_are(cells, {{1, 0}, {2, 0}}));
    CHECK(cells[0].face_x && !cells[0].inside && cells[0].t.n == 0);
    cells = walk(g, {16, 8}, {4, 8}); // the same point going left
    CHECK(cells_are(cells, {{0, 0}}) && cells[0].face_x);
    cells = walk(g, {16, 16}, {40, 40}); // from a corner, diagonally: the diagonal cell, no cell beside it
    CHECK(cells_are(cells, {{1, 1}, {2, 1}, {1, 2}, {2, 2}}) && cells[0].face_x && cells[0].face_y);
}

NNH_TEST(a_segment_ending_on_a_face_does_not_enter_the_next_cell) {
    const collision::Grid g = empty_grid(4, 4);
    CHECK(cells_are(walk(g, {8, 8}, {32, 8}), {{0, 0}, {1, 0}}));
    CHECK(cells_are(walk(g, {8, 8}, {32.01f, 8}), {{0, 0}, {1, 0}, {2, 0}}));
}

NNH_TEST(a_corner_visits_both_neighbours_then_the_diagonal) {
    const collision::Grid g = empty_grid(4, 4);
    const auto cells = walk(g, {8, 8}, {40, 40}); // through (16, 16) and (32, 32)
    CHECK(cells_are(cells, {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {2, 1}, {1, 2}, {2, 2}}));
    CHECK(cells[1].face_x && !cells[1].face_y && cells[2].face_y && !cells[2].face_x);
    CHECK(cells[3].face_x && cells[3].face_y && cells[3].corner);
    CHECK(cells[1].corner_point == Vec2(16, 16) && cells[2].corner_point == Vec2(16, 16));
    for (int i = 1; i <= 3; ++i) CHECK(collision::detail::compare(cells[i].t, {1, 4}) == 0);
    const auto back = walk(g, {40, 40}, {8, 8}); // the other way: x-neighbour first again
    CHECK(cells_are(back, {{2, 2}, {1, 2}, {2, 1}, {1, 1}, {0, 1}, {1, 0}, {0, 0}}));
}

NNH_TEST(a_far_segment_is_clipped_to_the_grid) {
    const collision::Grid g = empty_grid(3, 2);
    auto cells = walk(g, {-1000, 8}, {1000, 8});
    CHECK(cells_are(cells, {{0, 0}, {1, 0}, {2, 0}}));
    CHECK(cells[0].face_x && collision::detail::compare(cells[0].t, {1000, 2000}) == 0);
    CHECK(walk(g, {-1000, 100}, {1000, 100}).empty()); // misses the grid
    CHECK(walk(g, {-10, -10}, {-10, 100}).empty());
    cells = walk(g, {-1000, 8}, {1000, 8}, false); // without clipping: every outside cell too
    CHECK(cells.size() == 126 && cells[63].x == 0 && cells.front().x == -63 && cells.back().x == 62);
}

NNH_TEST(a_clip_through_a_grid_corner_keeps_the_cell_beside_it) {
    const collision::Grid g = empty_grid(4, 4);
    // Enters the grid exactly at (0, 16), between rows 0 and 1, going right and down.
    const auto cells = walk(g, {-16, 0}, {24, 40});
    CHECK(cells.size() >= 2 && cells[0].x == 0 && cells[0].y == 0 && cells[0].face_x && !cells[0].face_y);
    CHECK(cells[1].x == 0 && cells[1].y == 1 && cells[1].face_x && cells[1].face_y);
}

NNH_TEST(long_rays_visit_every_cell) {
    const collision::Grid g = empty_grid(1000, 1000, 16, 16, {}, collision::Cell::Solid);
    const auto cells = walk(g, {1, 3}, {10001, 3}, false);
    CHECK(cells.size() == 626 && cells.back().x == 625);
    const auto diag = walk(g, {0.5f, 0.25f}, {7000.5f, 7001.25f}, false);
    int x = 0, y = 0;
    for (const WalkCell& c : diag) {
        CHECK(std::abs(c.x - x) + std::abs(c.y - y) <= 2);
        x = c.x;
        y = c.y;
    }
    CHECK(x == 437 && y == 437);
}

NNH_TEST(non_square_cells_and_a_negative_fractional_origin) {
    const collision::Grid g = empty_grid(5, 5, 12, 20, {-7.5f, -3.25f});
    const auto cells = walk(g, {-7, -3}, {40, -3});
    CHECK(cells_are(cells, {{0, 0}, {1, 0}, {2, 0}, {3, 0}}));
    const auto down = walk(g, {0, -3.25f}, {0, 60}); // starts on the top edge, inside column 0
    CHECK(cells_are(down, {{0, 0}, {0, 1}, {0, 2}, {0, 3}}) && down[0].face_y);
}

namespace {

struct Hash {
    std::uint32_t s;
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    int range(int n) { return static_cast<int>(next() % static_cast<std::uint32_t>(n)); }
};

struct Layout {
    float cw, ch, ox, oy;
};

/// A lattice coordinate (in 1/256 px) near the grid; often exactly on a line, sometimes far outside.
ray_ref::I64 coordinate(Hash& h, ray_ref::I64 origin, ray_ref::I64 size, int cells) {
    const ray_ref::I64 span = size * (cells + 4);
    ray_ref::I64 v = origin - 2 * size + h.range(static_cast<int>(span));
    const int r = h.range(8);
    if (r == 0) v = origin + size * (h.range(cells + 3) - 1); // on a line
    if (r == 1) v = origin - 40 * size + h.range(static_cast<int>(size * (cells + 80)));
    return v;
}

/// Rays whose reference has corner cells, and cells compared: the comparison must not be vacuous.
struct Coverage {
    int corner_rays = 0;
    int cells = 0;
};

int compare_with_reference(collision::Cell outside, std::uint32_t seed, Coverage& coverage) {
    const Layout layouts[4] = {{8, 8, 0, 0}, {16, 16, -24.5f, 7.25f}, {12, 20, 3.75f, -40}, {24, 10, -0.5f, -0.25f}};
    Hash h{seed};
    int mismatches = 0;
    static collision::CellCrossing got[4096];
    for (int ray = 0; ray < 5000; ++ray) {
        const Layout& l = layouts[ray % 4];
        const int w = 3 + h.range(10), ht = 3 + h.range(10);
        const collision::Grid g = empty_grid(w, ht, l.cw, l.ch, {l.ox, l.oy}, outside);
        ray_ref::Lattice lat{static_cast<ray_ref::I64>(l.ox * 256), static_cast<ray_ref::I64>(l.oy * 256),
                             static_cast<ray_ref::I64>(l.cw * 256), static_cast<ray_ref::I64>(l.ch * 256), w, ht,
                             outside == collision::Cell::Empty};
        ray_ref::I64 px = coordinate(h, lat.ox, lat.cw, w), py = coordinate(h, lat.oy, lat.ch, ht);
        ray_ref::I64 qx = coordinate(h, lat.ox, lat.cw, w), qy = coordinate(h, lat.oy, lat.ch, ht);
        const int shape = h.range(6);
        if (shape == 0) qx = px;                       // vertical
        if (shape == 1) qy = py;                       // horizontal
        if (shape == 2) {                              // exactly through corners: a multiple of the cell diagonal
            px = lat.ox + lat.cw * (h.range(w + 2) - 1);
            py = lat.oy + lat.ch * (h.range(ht + 2) - 1);
            const int k = h.range(9) - 4, m = h.range(2) + 1;
            qx = px + (k == 0 ? 1 : k) * lat.cw / m;
            qy = py + (h.range(2) ? 1 : -1) * (k == 0 ? 1 : (k < 0 ? -k : k)) * lat.ch / m;
        }
        const std::vector<ray_ref::RefCell> want = ray_ref::cells(lat, px, py, qx, qy);
        const Vec2 from{static_cast<float>(px) / 256, static_cast<float>(py) / 256};
        const Vec2 to{static_cast<float>(qx) / 256, static_cast<float>(qy) / 256};
        const int n = collision::cells_crossed(g, from, to, got, 4096);
        coverage.cells += n;
        for (const ray_ref::RefCell& c : want) {
            if (c.rank < 2) {
                ++coverage.corner_rays;
                break;
            }
        }
        bool same = n == static_cast<int>(want.size());
        for (int i = 0; same && i < n; ++i) {
            same = got[i].x == want[i].x && got[i].y == want[i].y && got[i].fraction == ray_ref::fraction(want[i].t);
        }
        if (!same && ++mismatches <= 3) {
            std::printf("  ray %d: (%g, %g) -> (%g, %g) grid %dx%d %gx%g at (%g, %g): %d cells, reference %d\n", ray,
                        from.x, from.y, to.x, to.y, w, ht, l.cw, l.ch, l.ox, l.oy, n, static_cast<int>(want.size()));
        }
    }
    return mismatches;
}

} // namespace

NNH_TEST(the_walk_equals_the_exact_reference_on_5000_rays_clipped) {
    Coverage c;
    CHECK(compare_with_reference(collision::Cell::Empty, 0x9E3779B9u, c) == 0);
    CHECK(c.corner_rays > 300 && c.cells > 15000);
}

NNH_TEST(the_walk_equals_the_exact_reference_on_5000_rays_with_outside_cells) {
    Coverage c;
    CHECK(compare_with_reference(collision::Cell::Solid, 0x85EBCA6Bu, c) == 0);
    CHECK(c.corner_rays > 300 && c.cells > 30000);
}

NNH_TEST(every_sampled_point_of_a_float_ray_lies_in_a_visited_cell_in_order) {
    Hash h{12345u};
    const collision::Grid g = empty_grid(20, 20, 13.5f, 9.25f, {-3.3f, 1.7f}, collision::Cell::Solid);
    static collision::CellCrossing got[2048];
    int misses = 0;
    for (int ray = 0; ray < 500; ++ray) {
        const Vec2 from{static_cast<float>(h.range(30000)) / 97.0f - 20, static_cast<float>(h.range(20000)) / 89.0f - 20};
        const Vec2 to{static_cast<float>(h.range(30000)) / 97.0f - 20, static_cast<float>(h.range(20000)) / 89.0f - 20};
        const int n = collision::cells_crossed(g, from, to, got, 2048);
        int at = 0;
        for (int k = 1; k < 64; ++k) {
            const double t = k / 64.0;
            const double x = from.x + t * (static_cast<double>(to.x) - from.x);
            const double y = from.y + t * (static_cast<double>(to.y) - from.y);
            const double fx = (x - g.origin.x) / g.cell_w, fy = (y - g.origin.y) / g.cell_h;
            const double cx = static_cast<double>(static_cast<long long>(fx < 0 ? fx - 1 : fx));
            const double cy = static_cast<double>(static_cast<long long>(fy < 0 ? fy - 1 : fy));
            if (fx - cx < 1e-4 || cx + 1 - fx < 1e-4 || fy - cy < 1e-4 || cy + 1 - fy < 1e-4) continue; // on an edge
            while (at < n && !(got[at].x == static_cast<int>(cx) && got[at].y == static_cast<int>(cy))) ++at;
            if (at == n) ++misses;
            if (at == n) break;
        }
    }
    CHECK(misses == 0);
}
