// AC-2 (spec 025 T6.1): seeded random maps (1 x 1 to 64 x 64, 0-50 % blocked, weights 1-9, every mode, agent sizes
// 1-2) against plain references: 4 searches each (partial on and off, a limit sometimes, one sliced) against the
// Dijkstra (reachability, minimum cost, a valid path summing to the cost) and the literal reference A* (same status,
// cost, expansions and cells); one field (1-4 goals) against the multi-source reference and next() against its
// definition; every smoothed segment against the exact checker.

#include <algorithm>
#include <vector>

#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::kUnreachable;
using nnh::PathOptions;
using nnh::PathResult;
using nnh::PathSearcher;
using nnh::PathStatus;

namespace {

// Debug runs a quarter of the maps (the literal A* scans its open set: quadratic); the other flavors run them all.
constexpr unsigned kMaps = nnh::build::flavor() == nnh::build::Flavor::Debug ? 512 : 2048;

struct Tally {
    int searches = 0, found = 0, partial = 0, sliced = 0, fields = 0, segments = 0, failures = 0;
};

std::vector<CellPos> cells_of(const PathSearcher& s) {
    std::vector<CellPos> v(static_cast<std::size_t>(s.path(nullptr, 0)));
    s.path(v.data(), static_cast<int>(v.size()));
    return v;
}

void check_search(const path_ref::RandomMap& m, PathSearcher& s, unsigned seed, unsigned k, Tally& t) {
    const CellPos from = path_ref::random_cell(m.ref, seed, 100 + k);
    const CellPos to = path_ref::random_cell(m.ref, seed, 200 + k);
    const unsigned h = path_test::hash4(seed, k, 0, 300);
    const PathOptions o{.max_expanded = h % 4 == 0 ? 1 + static_cast<int>((h >> 4) % 60) : 0,
                        .partial = (h >> 8) % 2 == 0,
                        .heuristic_scale = (h >> 9) % 5 == 0 ? 40 : 16};
    PathResult r;
    if (k == 3) { // sliced
        s.begin(m.map, from, to, o);
        while (!s.step(1 + static_cast<int>((h >> 12) % 9))) {
        }
        r = s.result();
        ++t.sliced;
    } else {
        r = s.find(m.map, from, to, o);
    }
    ++t.searches;
    const std::vector<CellPos> cells = cells_of(s);
    const path_ref::RefResult lit = path_ref::literal_astar(m.ref, from, to, o);
    bool ok = r.status == lit.status && r.cost == lit.cost && r.expanded == lit.expanded && cells == lit.path;
    if (r.status == PathStatus::Found || r.status == PathStatus::Partial) {
        std::uint32_t summed = 0;
        ok = ok && path_ref::valid_path(m.ref, cells.data(), static_cast<int>(cells.size()), summed) && summed == r.cost;
        t.found += r.status == PathStatus::Found ? 1 : 0;
        t.partial += r.status == PathStatus::Partial ? 1 : 0;
    }
    if (o.max_expanded == 0 && from != to) { // unbounded: reachability and (exact heuristic) the minimum
        const auto dist = path_ref::dijkstra(m.ref, from);
        const std::uint32_t best = m.ref.at(to.x, to.y) != 0 ? dist[static_cast<std::size_t>(m.ref.index(to))] : kUnreachable;
        ok = ok && ((r.status == PathStatus::Found) == (best != kUnreachable));
        if (r.status == PathStatus::Found && o.heuristic_scale == 16) ok = ok && r.cost == best;
    }
    if (cells.size() >= 2) { // smoothing: every segment exact, with the heaviest replaced weight
        std::vector<CellPos> w(cells.size());
        const int n = nnh::smooth_path(m.map, cells.data(), static_cast<int>(cells.size()), w.data(),
                                       static_cast<int>(w.size()));
        std::size_t at = 0;
        for (int i = 1; i < n; ++i) {
            const std::size_t start = at;
            while (at < cells.size() && !(cells[at] == w[static_cast<std::size_t>(i)])) ++at;
            int heaviest = 1;
            for (std::size_t c = start + 1; c <= at && c < cells.size(); ++c) {
                heaviest = std::max(heaviest, m.ref.at(cells[c].x, cells[c].y));
            }
            ok = ok && at < cells.size() && path_ref::segment_ok(m.ref, w[static_cast<std::size_t>(i - 1)],
                                                                 w[static_cast<std::size_t>(i)], heaviest);
            ++t.segments;
        }
    }
    if (!ok) {
        if (t.failures < 5) std::printf("  map seed %u search %u differs\n", m.seed, k);
        ++t.failures;
    }
}

void check_field(const path_ref::RandomMap& m, nnh::DistanceField& f, unsigned seed, Tally& t) {
    std::vector<CellPos> goals;
    for (unsigned k = 0; k < 1 + seed % 4; ++k) goals.push_back(path_ref::random_cell(m.ref, seed, 400 + k));
    const std::uint32_t max_cost = seed % 7 == 0 ? 200u : kUnreachable;
    f.build(m.map, goals.data(), static_cast<int>(goals.size()), max_cost);
    const auto want = path_ref::field(m.ref, goals, max_cost);
    bool ok = true;
    for (int y = 0; y < m.ref.h; ++y) {
        for (int x = 0; x < m.ref.w; ++x) {
            const std::uint32_t d = f.distance({x, y});
            ok = ok && d == want[static_cast<std::size_t>(y * m.ref.w + x)];
            CellPos n{};
            if (f.next(m.map, {x, y}, n)) { // next lowers the cost-to-go by exactly the step it takes
                int dir = 0;
                while (path_ref::kDx[dir] != n.x - x || path_ref::kDy[dir] != n.y - y) ++dir;
                ok = ok && path_ref::step_allowed(m.ref, x, y, dir) &&
                     path_ref::step_cost(m.ref, dir, n.x, n.y) + f.distance(n) == d;
            } else {
                ok = ok && (d == 0 || d == kUnreachable ||
                            max_cost != kUnreachable); // a capped field may strand a cell next to its cap
            }
        }
    }
    ++t.fields;
    if (!ok) {
        if (t.failures < 5) std::printf("  map seed %u field differs\n", m.seed);
        ++t.failures;
    }
}

} // namespace

NNH_TEST(path_random_maps_match_the_references) {
    PathSearcher s = path_test::take(PathSearcher::create(64, 64));
    nnh::DistanceField f = path_test::take(nnh::DistanceField::create({.width = 64, .height = 64}));
    Tally t;
    for (unsigned seed = 0; seed < kMaps; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 100000, 64, 2);
        for (unsigned k = 0; k < 4; ++k) check_search(m, s, seed, k, t);
        check_field(m, f, seed, t);
    }
    std::printf("  %d maps: %d searches (%d found, %d partial, %d sliced), %d fields, %d smoothed segments\n",
                static_cast<int>(kMaps), t.searches, t.found, t.partial, t.sliced, t.fields, t.segments);
    CHECK(t.failures == 0);
    CHECK(t.found > static_cast<int>(kMaps) && t.partial > static_cast<int>(kMaps) / 8 && t.segments > static_cast<int>(kMaps));
}
