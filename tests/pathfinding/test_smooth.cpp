// Smoothing (spec 025 FR-15; T4.2): open room -> 2 waypoints, an L corridor -> 3, a corner it must not cut in each
// mode, a heavier cell it must not cross, a blocked start, first and last kept, in place, the buffer contract, and every
// segment of 500 seeded smoothings checked exactly.

#include <initializer_list>
#include <vector>

#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::Diagonals;
using nnh::PathMap;
using nnh::PathSearcher;

namespace {

std::vector<CellPos> found_path(PathSearcher& s, const PathMap& map, CellPos a, CellPos b) {
    s.find(map, a, b);
    std::vector<CellPos> v(static_cast<std::size_t>(s.path(nullptr, 0)));
    s.path(v.data(), static_cast<int>(v.size()));
    return v;
}

std::vector<CellPos> smoothed(const PathMap& map, const std::vector<CellPos>& path) {
    std::vector<CellPos> out(path.size());
    const int n = nnh::smooth_path(map, path.data(), static_cast<int>(path.size()), out.data(),
                                   static_cast<int>(out.size()));
    out.resize(static_cast<std::size_t>(n));
    return out;
}

} // namespace

NNH_TEST(path_smoothing_an_open_room_keeps_two_points) {
    const char* rows[] = {"..........", "..........", "..........", "..........", ".........."};
    const PathMap map = path_test::ascii_map(rows, 5);
    PathSearcher s = path_test::take(PathSearcher::create(16, 16));
    const auto path = found_path(s, map, {0, 0}, {9, 3});
    CHECK(path.size() > 2);
    const auto w = smoothed(map, path);
    CHECK(w.size() == 2 && w[0] == (CellPos{0, 0}) && w[1] == (CellPos{9, 3}));
}

NNH_TEST(path_smoothing_an_l_corridor_keeps_three_points) {
    const char* rows[] = {
        "S......",
        "######.",
        "######.",
        "######G",
    };
    const PathMap map = path_test::ascii_map(rows, 4);
    PathSearcher s = path_test::take(PathSearcher::create(16, 16));
    const auto w = smoothed(map, found_path(s, map, {0, 0}, {6, 3}));
    CHECK(w.size() == 3 && w[0] == (CellPos{0, 0}) && w[1] == (CellPos{6, 0}) && w[2] == (CellPos{6, 3}));
}

NNH_TEST(path_smoothing_never_cuts_a_wall_corner_unless_the_mode_allows_it) {
    // The straight segment (0,0) -> (2,2) passes the corner shared by (1,0) and (0,1)... and (1,1), (2,1) and (1,2).
    // Here (1,0) is a wall: a corner whose side cells are (1,0) (blocked) and (0,1) (open).
    const char* rows[] = {".#.", "...", "..."};
    const std::vector<CellPos> path = {{0, 0}, {0, 1}, {1, 2}, {2, 2}};
    const PathMap strict = path_test::ascii_map(rows, 3, Diagonals::OnlyIfNoObstacles);
    const PathMap one = path_test::ascii_map(rows, 3, Diagonals::AtLeastOneWalkable);
    const PathMap always = path_test::ascii_map(rows, 3, Diagonals::Always);
    const PathMap four = path_test::ascii_map(rows, 3, Diagonals::Never);
    CHECK(smoothed(strict, path).size() == 3);
    CHECK(smoothed(four, path).size() == 3);
    CHECK(smoothed(one, path).size() == 2);
    CHECK(smoothed(always, path).size() == 2);
    // Both side cells walls: only Always passes the corner at (1, 1)-(0, 0).
    const char* gap[] = {".#..", "#...", "...."};
    const std::vector<CellPos> long_way = {{0, 0}, {1, 1}, {2, 2}, {3, 2}};
    CHECK(smoothed(path_test::ascii_map(gap, 3, Diagonals::Always), long_way).size() == 3); // (0,0)->(3,2) crosses (1,0)
    CHECK(smoothed(path_test::ascii_map(gap, 3, Diagonals::AtLeastOneWalkable), {{0, 0}, {1, 1}, {2, 2}}).size() == 3);
    CHECK(smoothed(path_test::ascii_map(gap, 3, Diagonals::Always), {{0, 0}, {1, 1}, {2, 2}}).size() == 2);
}

NNH_TEST(path_smoothing_never_crosses_a_heavier_cell) {
    const char* rows[] = {
        "S.....",
        "..9...",
        ".....G",
    };
    const PathMap map = path_test::ascii_map(rows, 3);
    const std::vector<CellPos> around = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 1}, {5, 2}};
    const auto w = smoothed(map, around);
    for (std::size_t i = 1; i < w.size(); ++i) CHECK(path_ref::segment_ok(path_ref::ref_of(map), w[i - 1], w[i], 1));
    CHECK(w.size() >= 3);
    // The same path through the heavy cell may cross it (it replaces a part as heavy).
    const std::vector<CellPos> through = {{0, 0}, {1, 1}, {2, 1}, {3, 1}, {4, 2}, {5, 2}};
    CHECK(smoothed(map, through).size() == 2);
}

NNH_TEST(path_smoothing_keeps_first_and_last_works_in_place_and_follows_the_buffer_contract) {
    const char* rows[] = {"S.#....", "..#.##.", "....#.G"};
    const PathMap map = path_test::ascii_map(rows, 3);
    PathSearcher s = path_test::take(PathSearcher::create(16, 16));
    std::vector<CellPos> path = found_path(s, map, {0, 0}, {6, 2});
    const std::vector<CellPos> w = smoothed(map, path);
    CHECK(w.front() == path.front() && w.back() == path.back());
    std::vector<CellPos> in_place = path;
    const int n = nnh::smooth_path(map, in_place.data(), static_cast<int>(in_place.size()), in_place.data(),
                                   static_cast<int>(in_place.size()));
    CHECK(n == static_cast<int>(w.size()));
    for (int i = 0; i < n; ++i) CHECK(in_place[static_cast<std::size_t>(i)] == w[static_cast<std::size_t>(i)]);
    CHECK(nnh::smooth_path(map, path.data(), static_cast<int>(path.size()), nullptr, 0) == n);
    CellPos two[3] = {{9, 9}, {9, 9}, {9, 9}};
    CHECK(nnh::smooth_path(map, path.data(), static_cast<int>(path.size()), two, 2) == n);
    CHECK(two[0] == w[0] && two[1] == w[1] && two[2] == (CellPos{9, 9}));
    CHECK(nnh::smooth_path(map, path.data(), 0, two, 2) == 0);
    CHECK(nnh::smooth_path(map, path.data(), 1, two, 2) == 1 && two[0] == path[0]);
}

NNH_TEST(path_smoothing_from_a_blocked_start_never_checks_the_start) {
    const char* rows[] = {"#.....", "......"};
    const PathMap map = path_test::ascii_map(rows, 2);
    PathSearcher s = path_test::take(PathSearcher::create(16, 16));
    const auto w = smoothed(map, found_path(s, map, {0, 0}, {5, 1}));
    CHECK(w.size() == 2);
}

NNH_TEST(path_every_smoothed_segment_of_500_seeded_paths_passes_the_exact_check) {
    PathSearcher s = path_test::take(PathSearcher::create(48, 48));
    int checked = 0;
    for (unsigned seed = 0; seed < 500; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 41000, 48);
        const CellPos a = path_ref::random_cell(m.ref, seed, 1);
        const CellPos b = path_ref::random_cell(m.ref, seed, 2);
        const auto path = found_path(s, m.map, a, b);
        if (path.size() < 2) continue;
        const auto w = smoothed(m.map, path);
        CHECK(w.front() == path.front() && w.back() == path.back());
        // Each waypoint is a path cell, in order; each segment passes with the heaviest cell it replaces.
        std::size_t at = 0;
        for (std::size_t i = 1; i < w.size(); ++i) {
            const std::size_t from = at;
            while (at < path.size() && !(path[at] == w[i])) ++at;
            CHECK(at < path.size());
            int heaviest = 1;
            for (std::size_t k = from + 1; k <= at && k < path.size(); ++k) {
                heaviest = std::max(heaviest, m.ref.at(path[k].x, path[k].y));
            }
            CHECK(path_ref::segment_ok(m.ref, w[i - 1], w[i], heaviest));
            ++checked;
        }
    }
    CHECK(checked > 300);
}
