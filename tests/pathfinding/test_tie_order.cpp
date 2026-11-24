// The total order (spec 025 FR-10; T2.5): hand-worked ties with their expected cells, and the literal reference A*
// giving the same status, cost, expansions and cells on 300 seeded maps of every mode.

#include <initializer_list>
#include <vector>

#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::Diagonals;
using nnh::PathMap;
using nnh::PathSearcher;
using nnh::PathStatus;

namespace {

bool path_is(PathSearcher& s, std::initializer_list<CellPos> want) {
    CellPos got[32];
    const int n = s.path(got, 32);
    if (n != static_cast<int>(want.size())) return false;
    int i = 0;
    for (CellPos c : want) {
        if (!(got[i++] == c)) return false;
    }
    return true;
}

} // namespace

NNH_TEST(path_tie_straight_line_in_an_open_room_is_straight) {
    const char* rows[] = {".....", ".....", "....."};
    PathSearcher s = path_test::take(PathSearcher::create(8, 8));
    s.find(path_test::ascii_map(rows, 3), {0, 1}, {4, 1});
    CHECK(path_is(s, {{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}}));
}

NNH_TEST(path_tie_between_diagonal_first_and_straight_first_goes_by_lower_h) {
    // (0, 0) -> (2, 1), cost 24 both ways: E then SE, or SE then E. After the start, E has h 14 and SE h 10 (both
    // f 24): SE is expanded first and reaches the goal.
    const char* rows[] = {"...", "..."};
    PathSearcher s = path_test::take(PathSearcher::create(8, 8));
    const nnh::PathResult r = s.find(path_test::ascii_map(rows, 2), {0, 0}, {2, 1});
    CHECK(r.cost == 24 && r.expanded == 3);
    CHECK(path_is(s, {{0, 0}, {1, 1}, {2, 1}}));
}

NNH_TEST(path_tie_around_a_symmetric_pillar_goes_by_the_most_recent_push) {
    // Around the pillar both ways cost 40. The south cell (pushed after the north one) is expanded first, but the
    // north side wins the later ties at f 40 (the most recent push), so the path goes over the top.
    const char* rows[] = {"...", ".#.", "..."};
    PathSearcher s = path_test::take(PathSearcher::create(8, 8));
    const nnh::PathResult r = s.find(path_test::ascii_map(rows, 3), {0, 1}, {2, 1});
    CHECK(r.cost == 40 && r.expanded == 7);
    CHECK(path_is(s, {{0, 1}, {0, 0}, {1, 0}, {2, 0}, {2, 1}}));
}

NNH_TEST(path_tie_with_equal_weights_both_ways_matches_the_reference) {
    const char* rows[] = {"S3...", ".#3#.", "..3.G"};
    for (Diagonals mode : {Diagonals::Never, Diagonals::Always, Diagonals::AtLeastOneWalkable,
                           Diagonals::OnlyIfNoObstacles}) {
        const PathMap map = path_test::ascii_map(rows, 3, mode);
        PathSearcher s = path_test::take(PathSearcher::create(8, 8));
        const nnh::PathResult r = s.find(map, {0, 0}, {4, 2});
        const path_ref::RefResult ref = path_ref::literal_astar(path_ref::ref_of(map), {0, 0}, {4, 2});
        CHECK(r.cost == ref.cost && r.expanded == ref.expanded);
        CellPos got[32];
        CHECK(s.path(got, 32) == static_cast<int>(ref.path.size()));
        for (std::size_t i = 0; i < ref.path.size(); ++i) CHECK(got[i] == ref.path[i]);
    }
}

NNH_TEST(path_find_equals_the_literal_reference_astar_on_300_seeded_maps) {
    PathSearcher s = path_test::take(PathSearcher::create(40, 40));
    std::vector<CellPos> cells(40 * 40);
    for (unsigned seed = 0; seed < 300; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 5000, 40);
        for (unsigned k = 0; k < 3; ++k) {
            const CellPos from = path_ref::random_cell(m.ref, seed, 10 + k);
            const CellPos to = path_ref::random_cell(m.ref, seed, 20 + k);
            const nnh::PathOptions o{.heuristic_scale = k == 2 ? 24 : 16};
            const nnh::PathResult r = s.find(m.map, from, to, o);
            const path_ref::RefResult ref = path_ref::literal_astar(m.ref, from, to, o);
            CHECK(r.status == ref.status && r.cost == ref.cost && r.expanded == ref.expanded);
            CHECK(r.cells == static_cast<int>(ref.path.size()));
            if (r.cells != static_cast<int>(ref.path.size())) continue;
            s.path(cells.data(), r.cells);
            bool same = true;
            for (int i = 0; i < r.cells; ++i) same = same && cells[static_cast<std::size_t>(i)] == ref.path[static_cast<std::size_t>(i)];
            CHECK(same);
        }
    }
}
