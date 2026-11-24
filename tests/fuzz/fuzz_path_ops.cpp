// Fuzz target body for pathfinding (spec 025 T6.3), see fuzz_path_ops.hpp.

#include "fuzz_path_ops.hpp"

#include <cstdlib>

#include "nnh/build.hpp"
#include "nnh/pathfinding.hpp"

namespace nnh::fuzz {
namespace {

constexpr int kSide = 32;
constexpr int kDx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
constexpr int kDy[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

struct Reader {
    const std::uint8_t* data;
    std::size_t size;
    std::size_t at = 0;
    std::uint32_t next() { return at < size ? data[at++] : 0u; }
    bool more() const { return at < size; }
};

void require(bool ok) {
    if (!ok) std::abort();
}

/// A found path: starts at `from`, adjacent steps the mode allows into walkable cells, costs summing to `cost`.
bool valid_path(const PathMap& m, const CellPos* c, int n, CellPos from, std::uint32_t cost) {
    if (n < 1 || !(c[0] == from)) return false;
    std::uint32_t sum = 0;
    const bool eight = m.diagonals() != Diagonals::Never;
    for (int i = 1; i < n; ++i) {
        const int dx = c[i].x - c[i - 1].x;
        const int dy = c[i].y - c[i - 1].y;
        int d = -1;
        for (int k = 0; k < (eight ? 8 : 4); ++k) d = kDx[k] == dx && kDy[k] == dy ? k : d;
        if (d < 0 || !m.contains(c[i]) || m.blocked(c[i].x, c[i].y)) return false;
        if (d >= 4) {
            const bool a = !m.blocked(c[i].x, c[i - 1].y);
            const bool b = !m.blocked(c[i - 1].x, c[i].y);
            if (m.diagonals() == Diagonals::AtLeastOneWalkable && !(a || b)) return false;
            if (m.diagonals() == Diagonals::OnlyIfNoObstacles && !(a && b)) return false;
        }
        sum += static_cast<std::uint32_t>((d < 4 ? m.straight_cost() : m.diagonal_cost()) * m.weight(c[i].x, c[i].y));
    }
    return sum == cost;
}

struct Session {
    PathMap map;
    PathSearcher a;
    PathSearcher b;
    DistanceField field;
    CellPos path[kSide * kSide];
    CellPos other[kSide * kSide];
    Vec2 points[kSide * kSide];
    int count = 0;
};

CellPos cell(Reader& r, int w, int h) {
    const int x = static_cast<int>(r.next());
    const int y = static_cast<int>(r.next());
    if (!build::kAssertsEnabled && x == 255) return {-1, y % h}; // misuse, Release builds only
    return {x % w, y % h};
}

void search(Session& s, Reader& r, int w, int h) {
    const CellPos from = cell(r, w, h);
    const CellPos to = cell(r, w, h);
    const std::uint32_t o = r.next();
    const PathOptions opt{.max_expanded = static_cast<int>(o % 4) * 25, .partial = (o & 4u) != 0,
                          .heuristic_scale = 16 + static_cast<int>((o >> 3) % 4) * 16};
    const PathResult one = s.a.find(s.map, from, to, opt);
    s.count = s.a.path(s.path, kSide * kSide);
    s.b.begin(s.map, from, to, opt);
    const int slice = 1 + static_cast<int>(r.next() % 40);
    while (!s.b.step(slice)) {
    }
    const PathResult two = s.b.result();
    require(one.status == two.status && one.cost == two.cost && one.cells == two.cells && one.expanded == two.expanded);
    require(s.b.path(s.other, kSide * kSide) == s.count);
    for (int i = 0; i < s.count; ++i) require(s.path[i] == s.other[i]);
    if (one.status == PathStatus::Found || one.status == PathStatus::Partial) {
        require(valid_path(s.map, s.path, s.count, from, one.cost));
        if (one.status == PathStatus::Found) require(s.path[s.count - 1] == to);
    } else {
        require(s.count == 0);
    }
}

void stale_search(Session& s, Reader& r, int w, int h) {
    const CellPos from = cell(r, w, h);
    const CellPos to = cell(r, w, h);
    s.b.begin(s.map, from, to, {.partial = true});
    s.b.step(static_cast<int>(r.next() % 16));
    const CellPos c = cell(r, w, h);
    if (s.map.contains(c)) s.map.set_cell(c.x, c.y, r.next() % 2 == 0 ? kPathBlocked : 1 + static_cast<int>(r.next() % 9));
    while (!s.b.step(7)) {
    }
    const int n = s.b.path(s.other, kSide * kSide);
    for (int i = 0; i < n; ++i) require(s.map.contains(s.other[i]));
}

void smooth(Session& s) {
    const int n = smooth_path(s.map, s.path, s.count, s.other, kSide * kSide);
    require(n <= s.count && (s.count == 0 || (s.other[0] == s.path[0] && s.other[n - 1] == s.path[s.count - 1])));
}

void field(Session& s, Reader& r, int w, int h) {
    CellPos goals[4];
    const int count = 1 + static_cast<int>(r.next() % 4);
    for (int g = 0; g < count; ++g) goals[g] = cell(r, w, h);
    const std::uint32_t cap = r.next() % 2 == 0 ? kUnreachable : r.next() * 4;
    s.field.begin(s.map, goals, count, cap);
    const int slice = static_cast<int>(r.next() % 64);
    for (int guard = 0; !s.field.step(slice + 1) && guard < 100000; ++guard) {
    }
    for (int q = 0; q < 8; ++q) {
        const CellPos at{static_cast<int>(r.next()) % w, static_cast<int>(r.next()) % h};
        CellPos n{};
        if (s.field.next(s.map, at, n)) require(s.field.distance(n) < s.field.distance(at));
    }
}

void follow(Session& s, Reader& r) {
    const int n = s.a.path_points(s.map, s.points, kSide * kSide);
    PathFollower f(s.points, n, static_cast<float>(r.next() % 20));
    Vec2 p = n > 0 ? s.points[0] : Vec2{};
    for (int k = 0; k < 64 && !f.arrived(); ++k) {
        const Vec2 t = f.target(p);
        p = p + (t - p) * 0.5f;
    }
}

} // namespace

int path_ops_target(const std::uint8_t* data, std::size_t size) {
    Reader r{data, size};
    const int w = 1 + static_cast<int>(r.next() % kSide);
    const int h = 1 + static_cast<int>(r.next() % kSide);
    const std::uint32_t settings = r.next();
    PathMapDesc d{.width = w, .height = h};
    d.diagonals = static_cast<Diagonals>(settings % 4);
    d.agent_size = 1 + static_cast<int>((settings >> 2) % 3);
    d.straight_cost = 1 + static_cast<int>((settings >> 4) % 4);
    d.diagonal_cost = d.straight_cost + static_cast<int>((settings >> 6) % 2) * d.straight_cost / 2;
    auto map = PathMap::create(d);
    auto a = PathSearcher::create(kSide, kSide);
    auto b = PathSearcher::create(kSide, kSide);
    auto f = DistanceField::create({.width = kSide, .height = kSide, .double_buffered = (settings & 0x80u) != 0});
    if (!map || !a || !b || !f) return 0;
    static Session s; // large: kept off the stack
    s.map = static_cast<PathMap&&>(map.value());
    s.a = static_cast<PathSearcher&&>(a.value());
    s.b = static_cast<PathSearcher&&>(b.value());
    s.field = static_cast<DistanceField&&>(f.value());
    s.count = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint32_t v = r.next() % 12;
            if (v != 0) s.map.set_cell(x, y, v > 9 ? kPathBlocked : static_cast<int>(v));
        }
    }
    for (int ops = 0; r.more() && ops < 200; ++ops) {
        switch (r.next() % 8) {
        case 0: search(s, r, w, h); break;
        case 1: stale_search(s, r, w, h); break;
        case 2: smooth(s); break;
        case 3: field(s, r, w, h); break;
        case 4: follow(s, r); break;
        case 5: s.map.set_diagonals(static_cast<Diagonals>(r.next() % 4)); break;
        case 6: {
            const CellPos c = cell(r, w, h);
            s.map.fill({c.x, c.y, static_cast<int>(r.next() % 4), static_cast<int>(r.next() % 4)},
                       r.next() % 3 == 0 ? kPathBlocked : 1 + static_cast<int>(r.next() % 9));
            s.count = 0; // the old path may cross the new cells
            break;
        }
        default: {
            const CellPos c = cell(r, w, h);
            if (s.map.contains(c)) s.map.set_cell(c.x, c.y, r.next() % 2 == 0 ? kPathBlocked : 1);
            s.count = 0;
            break;
        }
        }
    }
    s.a = PathSearcher{};
    s.b = PathSearcher{};
    s.field = DistanceField{};
    s.map = PathMap{};
    return 0;
}

} // namespace nnh::fuzz
