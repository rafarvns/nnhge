// Search costs (spec 025 FR-6, FR-9; T2.4): step x weight of the entered cells, the start's weight never counts, every
// neighbour mode; a Found cost equals the reference Dijkstra's on 500 seeded maps; scaled heuristics stay within
// their bound.

#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::Diagonals;
using nnh::PathMap;
using nnh::PathSearcher;
using nnh::PathStatus;

NNH_TEST(path_cost_is_step_times_the_entered_cells_weight) {
    const char* rows[] = {"95G"};
    const PathMap map = path_test::ascii_map(rows, 1);
    PathSearcher s = path_test::take(PathSearcher::create(8, 8));
    CHECK(s.find(map, {0, 0}, {2, 0}).cost == 50 + 10); // the start's 9 never counts
    CHECK(s.find(map, {2, 0}, {0, 0}).cost == 50 + 90);
}

NNH_TEST(path_cost_of_a_diagonal_follows_each_mode) {
    const char* rows[] = {"...", "...", "..."};
    PathSearcher s = path_test::take(PathSearcher::create(8, 8));
    for (Diagonals mode : {Diagonals::Always, Diagonals::AtLeastOneWalkable, Diagonals::OnlyIfNoObstacles}) {
        const PathMap map = path_test::ascii_map(rows, 3, mode);
        CHECK(s.find(map, {0, 0}, {2, 2}).cost == 28);
    }
    const PathMap four = path_test::ascii_map(rows, 3, Diagonals::Never);
    CHECK(s.find(four, {0, 0}, {2, 2}).cost == 40);
    const char* gap[] = {".#", "#."};
    CHECK(s.find(path_test::ascii_map(gap, 2, Diagonals::Always), {0, 0}, {1, 1}).cost == 14);
    CHECK(s.find(path_test::ascii_map(gap, 2, Diagonals::AtLeastOneWalkable), {0, 0}, {1, 1}).status ==
          PathStatus::NoPath);
    const char* wall[] = {"..", "#."};
    CHECK(s.find(path_test::ascii_map(wall, 2, Diagonals::AtLeastOneWalkable), {0, 0}, {1, 1}).cost == 14);
    CHECK(s.find(path_test::ascii_map(wall, 2), {0, 0}, {1, 1}).cost == 20);
}

NNH_TEST(path_found_cost_equals_the_reference_dijkstra_on_500_seeded_maps) {
    PathSearcher s = path_test::take(PathSearcher::create(32, 32));
    int found = 0;
    for (unsigned seed = 0; seed < 500; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed, 32);
        const CellPos from = path_ref::random_cell(m.ref, seed, 1);
        const CellPos to = path_ref::random_cell(m.ref, seed, 2);
        const nnh::PathResult r = s.find(m.map, from, to);
        const auto dist = path_ref::dijkstra(m.ref, from);
        const std::uint32_t best = dist[static_cast<std::size_t>(m.ref.index(to))];
        if (from == to) {
            CHECK(r.status == PathStatus::Found && r.cost == 0);
        } else if (m.ref.at(to.x, to.y) == 0 || best == nnh::kUnreachable) {
            CHECK(r.status == PathStatus::NoPath);
        } else {
            CHECK(r.status == PathStatus::Found && r.cost == best);
            std::vector<CellPos> cells(static_cast<std::size_t>(r.cells));
            s.path(cells.data(), r.cells);
            std::uint32_t summed = 0;
            CHECK(path_ref::valid_path(m.ref, cells.data(), r.cells, summed) && summed == r.cost);
            ++found;
        }
    }
    CHECK(found > 150);
}

NNH_TEST(path_scaled_heuristics_stay_within_their_bound_and_expand_less) {
    PathSearcher s = path_test::take(PathSearcher::create(48, 48));
    long long expanded[3] = {};
    const int scales[3] = {16, 24, 64};
    for (unsigned seed = 0; seed < 300; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 1000, 48, 1);
        const CellPos from = path_ref::random_cell(m.ref, seed, 3);
        const CellPos to = path_ref::random_cell(m.ref, seed, 4);
        std::uint32_t minimum = 0;
        for (int k = 0; k < 3; ++k) {
            const nnh::PathResult r = s.find(m.map, from, to, {.heuristic_scale = scales[k]});
            expanded[k] += r.expanded;
            if (k == 0) minimum = r.cost;
            if (r.status == PathStatus::Found) {
                CHECK(static_cast<unsigned long long>(r.cost) * 16 <= static_cast<unsigned long long>(minimum) * scales[k]);
                CHECK(r.cost >= minimum);
            }
        }
    }
    CHECK(expanded[1] <= expanded[0] && expanded[2] <= expanded[0]);
}
