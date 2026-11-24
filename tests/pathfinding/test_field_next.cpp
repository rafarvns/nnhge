// Field queries (spec 025 FR-18; T5.3): next() is the brute-force argmin of step x weight + distance with ties in
// neighbour order, none at a goal or an unreachable cell, under the diagonal rule; next_point(); and a field equals the
// search cost from every walkable cell on 200 seeded maps.

#include <vector>

#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::DistanceField;
using nnh::kUnreachable;
using nnh::PathMap;

namespace {

DistanceField field(int w, int h) { return path_test::take(DistanceField::create({.width = w, .height = h})); }

/// Brute force: the neighbour minimising step x weight + distance (first in order on ties), or {-1, -1}.
CellPos brute_next(const path_ref::RefMap& m, const std::vector<std::uint32_t>& d, CellPos at) {
    const std::uint32_t here = d[static_cast<std::size_t>(m.index(at))];
    if (here == 0 || here == kUnreachable) return {-1, -1};
    CellPos best{-1, -1};
    std::uint64_t best_cost = ~0ull;
    for (int k = 0; k < path_ref::directions(m.mode); ++k) {
        if (!path_ref::step_allowed(m, at.x, at.y, k)) continue;
        const int nx = at.x + path_ref::kDx[k];
        const int ny = at.y + path_ref::kDy[k];
        const std::uint32_t dn = d[static_cast<std::size_t>(ny * m.w + nx)];
        if (dn == kUnreachable) continue;
        const std::uint64_t c = std::uint64_t{path_ref::step_cost(m, k, nx, ny)} + dn;
        if (c < best_cost) {
            best_cost = c;
            best = {nx, ny};
        }
    }
    return best;
}

} // namespace

NNH_TEST(path_field_next_is_the_brute_force_argmin_with_neighbour_order_ties) {
    DistanceField f = field(40, 40);
    int asked = 0;
    for (unsigned seed = 0; seed < 100; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 71000, 40);
        const CellPos goal = path_ref::random_cell(m.ref, seed, 1);
        f.build(m.map, &goal, 1);
        const auto d = path_ref::field(m.ref, {goal}, kUnreachable);
        for (int y = 0; y < m.ref.h; ++y) {
            for (int x = 0; x < m.ref.w; ++x) {
                CellPos got{-1, -1};
                const bool has = f.next(m.map, {x, y}, got);
                const CellPos want = brute_next(m.ref, d, {x, y});
                CHECK(has == (want.x >= 0));
                if (has) CHECK(got == want);
                ++asked;
            }
        }
    }
    CHECK(asked > 10000);
}

NNH_TEST(path_field_next_ties_go_to_the_first_neighbour_in_order) {
    // Goals (0, 0) and (1, 1). From (0, 1), N and E both cost 10 + 0: N comes first.
    const char* rows[] = {"...", "...", "..."};
    const PathMap map = path_test::ascii_map(rows, 3);
    DistanceField f = field(3, 3);
    const CellPos goals[] = {{0, 0}, {1, 1}};
    f.build(map, goals, 2);
    CellPos n{};
    CHECK(f.next(map, {0, 1}, n) && n == (CellPos{0, 0}));
    CHECK(f.next(map, {2, 2}, n) && n == (CellPos{1, 1}));
    CHECK(!f.next(map, {0, 0}, n)); // at a goal
}

NNH_TEST(path_field_next_is_none_at_unreachable_cells_and_obeys_the_corner_rule) {
    const char* rows[] = {".#", "#."};
    const PathMap strict = path_test::ascii_map(rows, 2);
    const PathMap always = path_test::ascii_map(rows, 2, nnh::Diagonals::Always);
    DistanceField f = field(2, 2);
    const CellPos goal{0, 0};
    CellPos n{};
    f.build(strict, &goal, 1);
    CHECK(!f.next(strict, {1, 1}, n));
    f.build(always, &goal, 1);
    CHECK(f.next(always, {1, 1}, n) && n == goal);
}

NNH_TEST(path_field_next_point_goes_through_cell_at_and_cell_center) {
    const char* rows[] = {"....", "...."};
    const PathMap map = path_test::ascii_map(rows, 2);
    DistanceField f = field(4, 4);
    const CellPos goal{3, 0};
    f.build(map, &goal, 1);
    nnh::Vec2 to{};
    // From (0, 1) (distance 34): E costs 10 + 24 and NE 14 + 20, a tie: E comes first.
    CHECK(f.next_point(map, {5, 20}, to) && to == map.cell_center({1, 1}));
    CHECK(!f.next_point(map, {-30, 5}, to));          // outside the map: no answer, no assert
    CHECK(!f.next_point(map, map.cell_center(goal), to)); // at the goal
}

NNH_TEST(path_field_equals_the_search_cost_from_every_walkable_cell_on_200_maps) {
    DistanceField f = field(16, 16);
    nnh::PathSearcher s = path_test::take(nnh::PathSearcher::create(16, 16));
    int bad = 0;
    for (unsigned seed = 0; seed < 200; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 81000, 16);
        const CellPos goals[2] = {path_ref::random_cell(m.ref, seed, 1), path_ref::random_cell(m.ref, seed, 2)};
        const int count = 1 + static_cast<int>(seed % 2);
        f.build(m.map, goals, count);
        for (int y = 0; y < m.ref.h; ++y) {
            for (int x = 0; x < m.ref.w; ++x) {
                if (m.ref.at(x, y) == 0) continue;
                std::uint32_t best = kUnreachable;
                for (int g = 0; g < count; ++g) {
                    if (m.ref.at(goals[g].x, goals[g].y) == 0) continue;
                    const nnh::PathResult r = s.find(m.map, {x, y}, goals[g]);
                    if (r.status == nnh::PathStatus::Found && r.cost < best) best = r.cost;
                }
                if (f.distance({x, y}) != best) ++bad;
            }
        }
    }
    CHECK(bad == 0);
}
