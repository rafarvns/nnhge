// Reference code for the pathfinding tests (spec 025 T2.1), see path_reference.hpp. Deliberately naive.

#include "path_reference.hpp"

#include <algorithm>
#include <functional>
#include <queue>
#include <utility>

#include "path_fixture.hpp"

namespace path_ref {

using nnh::kUnreachable;
using nnh::PathStatus;

const int kDx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
const int kDy[8] = {-1, 0, 1, 0, -1, 1, 1, -1};

RefMap ref_of(const nnh::PathMap& map) {
    RefMap m;
    m.w = map.width();
    m.h = map.height();
    m.mode = map.diagonals();
    m.straight = map.straight_cost();
    m.diagonal = map.diagonal_cost();
    m.cells.resize(static_cast<std::size_t>(m.w * m.h));
    for (int y = 0; y < m.h; ++y) {
        for (int x = 0; x < m.w; ++x) {
            const int v = map.weight(x, y);
            m.cells[static_cast<std::size_t>(y * m.w + x)] = v == nnh::kPathBlocked ? 0 : v;
        }
    }
    return m;
}

int directions(Diagonals mode) { return mode == Diagonals::Never ? 4 : 8; }

bool step_allowed(const RefMap& m, int x, int y, int d) {
    const int nx = x + kDx[d];
    const int ny = y + kDy[d];
    if (m.at(nx, ny) == 0) return false;
    if (d < 4) return true;
    const bool a = m.at(nx, y) != 0; // the side cells: the x neighbour and the y neighbour
    const bool b = m.at(x, ny) != 0;
    switch (m.mode) {
    case Diagonals::Always: return true;
    case Diagonals::AtLeastOneWalkable: return a || b;
    default: return a && b;
    }
}

std::uint32_t step_cost(const RefMap& m, int d, int nx, int ny) {
    return static_cast<std::uint32_t>((d < 4 ? m.straight : m.diagonal) * m.at(nx, ny));
}

std::uint32_t heuristic(const RefMap& m, int x, int y, CellPos to) {
    const int dx = x > to.x ? x - to.x : to.x - x;
    const int dy = y > to.y ? y - to.y : to.y - y;
    if (m.mode == Diagonals::Never) return static_cast<std::uint32_t>(m.straight * (dx + dy));
    const int lo = dx < dy ? dx : dy;
    const int hi = dx < dy ? dy : dx;
    return static_cast<std::uint32_t>(m.straight * (hi - lo) + m.diagonal * lo);
}

namespace {

using Item = std::pair<std::uint32_t, int>;
using MinQueue = std::priority_queue<Item, std::vector<Item>, std::greater<Item>>;

} // namespace

std::vector<std::uint32_t> dijkstra(const RefMap& m, CellPos from) {
    std::vector<std::uint32_t> dist(static_cast<std::size_t>(m.w * m.h), kUnreachable);
    MinQueue q;
    dist[static_cast<std::size_t>(m.index(from))] = 0;
    q.push({0, m.index(from)});
    while (!q.empty()) {
        const auto [d, c] = q.top();
        q.pop();
        if (d != dist[static_cast<std::size_t>(c)]) continue;
        const int x = c % m.w;
        const int y = c / m.w;
        for (int k = 0; k < directions(m.mode); ++k) {
            if (!step_allowed(m, x, y, k)) continue;
            const int nx = x + kDx[k];
            const int ny = y + kDy[k];
            const std::uint32_t nd = d + step_cost(m, k, nx, ny);
            auto& slot = dist[static_cast<std::size_t>(ny * m.w + nx)];
            if (nd < slot) {
                slot = nd;
                q.push({nd, ny * m.w + nx});
            }
        }
    }
    return dist;
}

std::vector<std::uint32_t> field(const RefMap& m, const std::vector<CellPos>& goals, std::uint32_t max_cost) {
    std::vector<std::uint32_t> dist(static_cast<std::size_t>(m.w * m.h), kUnreachable);
    MinQueue q;
    for (CellPos g : goals) {
        if (m.at(g.x, g.y) == 0) continue;
        dist[static_cast<std::size_t>(m.index(g))] = 0;
        q.push({0, m.index(g)});
    }
    while (!q.empty()) {
        const auto [d, c] = q.top();
        q.pop();
        if (d != dist[static_cast<std::size_t>(c)]) continue;
        const int x = c % m.w;
        const int y = c / m.w;
        for (int k = 0; k < directions(m.mode); ++k) {
            const int px = x + kDx[k];
            const int py = y + kDy[k];
            // An agent at p steps into c: the reversed step (the rule is symmetric).
            if (!step_allowed(m, x, y, k)) continue;
            const std::uint32_t nd = d + step_cost(m, k, x, y);
            if (nd > max_cost) continue;
            auto& slot = dist[static_cast<std::size_t>(py * m.w + px)];
            if (nd < slot) {
                slot = nd;
                q.push({nd, py * m.w + px});
            }
        }
    }
    return dist;
}

namespace {

struct OpenItem {
    int cell = 0;
    std::uint32_t f = 0, h = 0, seq = 0;
};

std::vector<CellPos> walk_back(const RefMap& m, const std::vector<int>& parent, int end) {
    std::vector<CellPos> path;
    for (int c = end; c >= 0; c = parent[static_cast<std::size_t>(c)]) path.push_back({c % m.w, c / m.w});
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace

RefResult literal_astar(const RefMap& m, CellPos from, CellPos to, const nnh::PathOptions& o) {
    RefResult r;
    const auto inside = [&](CellPos c) { return c.x >= 0 && c.y >= 0 && c.x < m.w && c.y < m.h; };
    if (!inside(from) || !inside(to) || o.max_expanded < 0 || o.heuristic_scale < 16 || o.heuristic_scale > 64) {
        r.status = PathStatus::Invalid;
        return r;
    }
    if (from == to) {
        r.status = PathStatus::Found;
        r.path = {from};
        return r;
    }
    if (m.at(to.x, to.y) == 0 && !o.partial) {
        r.status = PathStatus::NoPath;
        return r;
    }
    const int n = m.w * m.h;
    const int limit = o.max_expanded > 0 ? o.max_expanded : n;
    const auto scaled = [&](int x, int y) {
        return static_cast<std::uint32_t>(heuristic(m, x, y, to) * static_cast<std::uint32_t>(o.heuristic_scale)) >> 4;
    };
    std::vector<std::uint32_t> g(static_cast<std::size_t>(n), kUnreachable);
    std::vector<int> parent(static_cast<std::size_t>(n), -1);
    std::vector<char> closed(static_cast<std::size_t>(n), 0);
    std::vector<OpenItem> open;
    std::uint32_t seq = 0;
    g[static_cast<std::size_t>(m.index(from))] = 0;
    const std::uint32_t h0 = scaled(from.x, from.y);
    open.push_back({m.index(from), h0, h0, seq++});
    while (true) {
        if (r.expanded >= limit) {
            r.status = PathStatus::LimitReached;
            break;
        }
        if (open.empty()) {
            r.status = PathStatus::NoPath;
            break;
        }
        std::size_t best = 0;
        for (std::size_t i = 1; i < open.size(); ++i) {
            const OpenItem& a = open[i];
            const OpenItem& b = open[best];
            if (a.f < b.f || (a.f == b.f && (a.h < b.h || (a.h == b.h && a.seq > b.seq)))) best = i;
        }
        const OpenItem e = open[best];
        open.erase(open.begin() + static_cast<std::ptrdiff_t>(best));
        closed[static_cast<std::size_t>(e.cell)] = 1;
        ++r.expanded;
        if (e.cell == m.index(to)) {
            r.status = PathStatus::Found;
            break;
        }
        const int x = e.cell % m.w;
        const int y = e.cell / m.w;
        for (int k = 0; k < directions(m.mode); ++k) {
            if (!step_allowed(m, x, y, k)) continue;
            const int nx = x + kDx[k];
            const int ny = y + kDy[k];
            const int c = ny * m.w + nx;
            if (closed[static_cast<std::size_t>(c)]) continue;
            const std::uint32_t ng = g[static_cast<std::size_t>(e.cell)] + step_cost(m, k, nx, ny);
            const std::uint32_t h = scaled(nx, ny);
            if (g[static_cast<std::size_t>(c)] == kUnreachable) {
                g[static_cast<std::size_t>(c)] = ng;
                parent[static_cast<std::size_t>(c)] = e.cell;
                open.push_back({c, ng + h, h, seq++});
            } else if (ng < g[static_cast<std::size_t>(c)]) {
                g[static_cast<std::size_t>(c)] = ng;
                parent[static_cast<std::size_t>(c)] = e.cell;
                for (OpenItem& it : open) {
                    if (it.cell == c) {
                        it.f = ng + h;
                        it.seq = seq++;
                    }
                }
            }
        }
    }
    if (r.status == PathStatus::Found) {
        r.cost = g[static_cast<std::size_t>(m.index(to))];
        r.path = walk_back(m, parent, m.index(to));
        return r;
    }
    if (!o.partial) return r;
    // The reached cell closest to the goal: lowest unscaled h, then lower g, then lower row-major index.
    int best = -1;
    for (int c = 0; c < n; ++c) {
        if (g[static_cast<std::size_t>(c)] == kUnreachable) continue;
        if (best < 0) {
            best = c;
            continue;
        }
        const std::uint32_t hc = heuristic(m, c % m.w, c / m.w, to);
        const std::uint32_t hb = heuristic(m, best % m.w, best / m.w, to);
        if (hc < hb || (hc == hb && g[static_cast<std::size_t>(c)] < g[static_cast<std::size_t>(best)])) best = c;
    }
    r.status = PathStatus::Partial;
    r.cost = g[static_cast<std::size_t>(best)];
    r.path = walk_back(m, parent, best);
    return r;
}

bool valid_path(const RefMap& m, const CellPos* cells, int count, std::uint32_t& cost) {
    cost = 0;
    if (count < 1) return false;
    for (int i = 1; i < count; ++i) {
        const int dx = cells[i].x - cells[i - 1].x;
        const int dy = cells[i].y - cells[i - 1].y;
        int dir = -1;
        for (int k = 0; k < directions(m.mode); ++k) {
            if (kDx[k] == dx && kDy[k] == dy) dir = k;
        }
        if (dir < 0 || !step_allowed(m, cells[i - 1].x, cells[i - 1].y, dir)) return false;
        cost += step_cost(m, dir, cells[i].x, cells[i].y);
    }
    return true;
}

bool segment_ok(const RefMap& m, CellPos a, CellPos b, int max_weight) {
    if (a == b) return true;
    // Doubled coordinates: centres odd, grid lines even.
    const long long ax = 2 * a.x + 1, ay = 2 * a.y + 1;
    const long long dx = 2 * (b.x - a.x), dy = 2 * (b.y - a.y);
    const auto cross = [&](long long px, long long py) { return (px - ax) * dy - (py - ay) * dx; };
    for (int j = std::min(a.y, b.y); j <= std::max(a.y, b.y); ++j) {
        for (int i = std::min(a.x, b.x); i <= std::max(a.x, b.x); ++i) {
            if (i == a.x && j == a.y) continue;
            bool pos = false, neg = false;
            for (int cy = 0; cy <= 2; cy += 2) {
                for (int cx = 0; cx <= 2; cx += 2) {
                    const long long c = cross(2 * i + cx, 2 * j + cy);
                    pos = pos || c > 0;
                    neg = neg || c < 0;
                }
            }
            if (pos && neg) { // crosses the cell's interior
                const int v = m.at(i, j);
                if (v == 0 || v > max_weight) return false;
            }
        }
    }
    if (dx == 0 || dy == 0 || m.mode == Diagonals::Always) return true;
    const int sx = dx > 0 ? 1 : -1;
    const int sy = dy > 0 ? 1 : -1;
    for (int X = std::min(a.x, b.x) + 1; X <= std::max(a.x, b.x); ++X) {
        const long long num = (2LL * X - ax) * dy; // 2Y - ay = num / dx
        if (num % dx != 0) continue;
        const long long twice_y = ay + num / dx;
        if (twice_y % 2 != 0) continue;
        const int Y = static_cast<int>(twice_y / 2);
        const int i = sx > 0 ? X - 1 : X;
        const int j = sy > 0 ? Y - 1 : Y;
        const bool s1 = m.at(i + sx, j) != 0;
        const bool s2 = m.at(i, j + sy) != 0;
        if (m.mode == Diagonals::AtLeastOneWalkable ? !(s1 || s2) : !(s1 && s2)) return false;
    }
    return true;
}

RandomMap random_map(unsigned seed, int max_side, int max_agent) {
    using path_test::hash4;
    static const int kCosts[5][2] = {{10, 14}, {1, 1}, {2, 3}, {5, 7}, {3, 6}};
    RandomMap r;
    r.seed = seed;
    nnh::PathMapDesc d;
    d.width = 1 + static_cast<int>(hash4(seed, 1, 0, 11) % static_cast<unsigned>(max_side));
    d.height = 1 + static_cast<int>(hash4(seed, 2, 0, 11) % static_cast<unsigned>(max_side));
    d.diagonals = static_cast<Diagonals>(hash4(seed, 3, 0, 11) % 4);
    d.agent_size = 1 + static_cast<int>(hash4(seed, 4, 0, 11) % static_cast<unsigned>(max_agent));
    const unsigned pair = hash4(seed, 5, 0, 11) % 5;
    d.straight_cost = kCosts[pair][0];
    d.diagonal_cost = kCosts[pair][1];
    const unsigned blocked = hash4(seed, 6, 0, 11) % 51;
    r.map = path_test::take(nnh::PathMap::create(d));
    for (int y = 0; y < d.height; ++y) {
        for (int x = 0; x < d.width; ++x) {
            const unsigned h = hash4(seed, static_cast<unsigned>(x), static_cast<unsigned>(y), 12);
            r.map.set_cell(x, y, h % 100 < blocked ? nnh::kPathBlocked : 1 + static_cast<int>((h >> 8) % 9));
        }
    }
    r.ref = ref_of(r.map);
    return r;
}

CellPos random_cell(const RefMap& m, unsigned seed, unsigned salt) {
    const unsigned h = path_test::hash4(seed, salt, 0, 13);
    return {static_cast<int>(h % static_cast<unsigned>(m.w)), static_cast<int>((h >> 16) % static_cast<unsigned>(m.h))};
}

} // namespace path_ref
