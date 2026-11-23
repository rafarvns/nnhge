// The grid walk's integer path (plan "Console bench refinement (round 2)"): on the 1/256 px lattice within
// ±2^14 px the walk steps in integers; it must give the double walk's cells bit for bit, and be used only
// where its integers are exact.

#include "collision/grid_walk.hpp"
#include "nnh/collision.hpp"
#include "test.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace collision = nnh::collision;
using collision::detail::GridWalk;
using collision::detail::make_segment;
using collision::detail::WalkCell;
using collision::detail::WalkPath;
using nnh::Vec2;

namespace {

collision::Grid grid_of(int w, int h, float cw, float ch, Vec2 origin) {
    collision::Grid g;
    g.origin = origin;
    g.cell_w = cw;
    g.cell_h = ch;
    g.width = w;
    g.height = h;
    g.cell = [](const void*, int, int) { return collision::Cell::Empty; };
    return g;
}

std::vector<WalkCell> walk(const collision::Grid& g, Vec2 from, Vec2 to, bool clip, WalkPath path) {
    std::vector<WalkCell> out;
    GridWalk w(g, make_segment(from, to), clip, path);
    WalkCell c;
    while (w.next(c) && out.size() < 20000)
        out.push_back(c);
    return out;
}

bool same_bits(double a, double b) {
    return std::memcmp(&a, &b, sizeof a) == 0;
}
bool same_bits(float a, float b) {
    return std::memcmp(&a, &b, sizeof a) == 0;
}

bool same_cells(const std::vector<WalkCell>& a, const std::vector<WalkCell>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const WalkCell& p = a[i];
        const WalkCell& q = b[i];
        if (p.x != q.x || p.y != q.y || !same_bits(p.t.n, q.t.n) || !same_bits(p.t.a, q.t.a) ||
            p.face_x != q.face_x || p.face_y != q.face_y || p.inside != q.inside || p.corner != q.corner ||
            !same_bits(p.corner_point.x, q.corner_point.x) ||
            !same_bits(p.corner_point.y, q.corner_point.y)) {
            return false;
        }
    }
    return true;
}

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

/// A lattice coordinate near (or on a line of) an axis of `cells` cells of `size` from `origin`, in px.
float coordinate(Hash& h, float origin, float size, int cells) {
    const int r = h.range(6);
    if (r == 0) return origin + size * static_cast<float>(h.range(cells + 3) - 1); // on a line
    const int span = static_cast<int>(size * 256) * (cells + 8);
    return origin - 4 * size + static_cast<float>(h.range(span)) / 256.0f;
}

} // namespace

NNH_TEST(the_lattice_walk_equals_the_double_walk_cell_for_cell) {
    struct Layout {
        float cw, ch, ox, oy;
    };
    const Layout layouts[5] = {{16, 16, 0, 0},
                               {8, 8, -24.5f, 7.25f},
                               {12, 20, 3.75f, -40},
                               {0.5f, 0.75f, -0.5f, 0.25f},
                               {24, 10, -1000.125f, 812.5f}};
    Hash h{0x2545F491u};
    int mismatches = 0, lattice = 0, cells = 0;
    for (int ray = 0; ray < 6000; ++ray) {
        const Layout& l = layouts[ray % 5];
        const int w = 1 + h.range(24), ht = 1 + h.range(16);
        const collision::Grid g = grid_of(w, ht, l.cw, l.ch, {l.ox, l.oy});
        Vec2 from{coordinate(h, l.ox, l.cw, w), coordinate(h, l.oy, l.ch, ht)};
        Vec2 to{coordinate(h, l.ox, l.cw, w), coordinate(h, l.oy, l.ch, ht)};
        const int shape = h.range(6);
        if (shape == 0) to.x = from.x; // vertical
        if (shape == 1) to.y = from.y; // horizontal
        if (shape == 2) {              // through corners: a multiple of the cell diagonal from a corner
            from = {l.ox + l.cw * static_cast<float>(h.range(w + 2) - 1),
                    l.oy + l.ch * static_cast<float>(h.range(ht + 2) - 1)};
            const float k = static_cast<float>(h.range(7) - 3) + 0.5f;
            to = {from.x + k * l.cw, from.y + (h.range(2) ? k : -k) * l.ch};
        }
        const bool clip = h.range(2) == 0;
        GridWalk probe(g, make_segment(from, to), clip);
        lattice += probe.uses_lattice() ? 1 : 0;
        const std::vector<WalkCell> got = walk(g, from, to, clip, WalkPath::Any);
        const std::vector<WalkCell> want = walk(g, from, to, clip, WalkPath::Double);
        cells += static_cast<int>(want.size());
        if (!same_cells(got, want)) ++mismatches;
    }
    CHECK(mismatches == 0);
    CHECK(lattice > 3000 && cells > 30000); // not vacuous: most rays take the integer path (3949, 40891)
}

NNH_TEST(the_lattice_walk_is_used_only_where_its_integers_are_exact) {
    const collision::Grid g = grid_of(25, 15, 16, 16, {});
    const auto lattice = [&](Vec2 from, Vec2 to, bool clip = false, const collision::Grid* grid = nullptr) {
        return GridWalk(grid != nullptr ? *grid : g, make_segment(from, to), clip).uses_lattice();
    };
    CHECK(lattice({8, 8}, {100.5f, 50.25f}));
    CHECK(lattice({-16383.99609375f, 8}, {16383.99609375f, 9})); // just inside ±2^14
    CHECK(!lattice({-16384, 8}, {100, 9}));                      // ±2^14 itself is out
    CHECK(!lattice({8.1f, 8}, {100, 9}));                        // not a multiple of 1/256
    CHECK(!lattice({8, 8}, {100, 1e-40f}));                      // a subnormal
    CHECK(!lattice({8, 8}, {8, 8}));                             // nothing to walk
    CHECK(lattice({8, 8}, {500, 9}, true));                      // clipped, starting inside the grid
    CHECK(!lattice({-8, 8}, {500, 9}, true));                    // clipped, starting outside: the double path
    CHECK(!lattice({0, 8}, {500, 9}, true));                     // clipped, starting on the grid's edge
    const collision::Grid odd = grid_of(4, 4, 0.1f, 16, {});
    CHECK(!lattice({8, 8}, {100, 9}, false, &odd)); // a cell size off the lattice
    const collision::Grid far = grid_of(4, 4, 16, 16, {20000, 0});
    CHECK(!lattice({8, 8}, {100, 9}, false, &far)); // an origin beyond ±2^14
    const collision::Grid huge = grid_of(2000, 4, 16, 16, {});
    CHECK(
        lattice({8, 8}, {100, 9}, true, &huge)); // a grid reaching past ±2^14: only the segment's lines count
}
