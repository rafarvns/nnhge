// Bounds and partial paths (spec 025 FR-11..FR-13; T3.1): the limit checked before each pop, every status, the
// closest reached cell (lowest h, then g, then row-major index), an island's shore, a blocked goal, the open set never
// overflowing on a comb map, and the same searches without the option.

#include <initializer_list>
#include <string>
#include <vector>

#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::PathMap;
using nnh::PathOptions;
using nnh::PathResult;
using nnh::PathSearcher;
using nnh::PathStatus;

namespace {

PathSearcher searcher(int side = 64) { return path_test::take(PathSearcher::create(side, side)); }

const char* kCorridor[] = {"S.......G"};

const char* kWalled[] = {
    "S....",
    ".###.",
    ".#G#.",
    ".###.",
    ".....",
};

} // namespace

NNH_TEST(path_limit_lets_the_goal_be_the_last_pop) {
    const PathMap map = path_test::ascii_map(kCorridor, 1);
    PathSearcher s = searcher();
    const PathResult nine = s.find(map, {0, 0}, {8, 0}, {.max_expanded = 9});
    CHECK(nine.status == PathStatus::Found && nine.expanded == 9);
    const PathResult eight = s.find(map, {0, 0}, {8, 0}, {.max_expanded = 8});
    CHECK(eight.status == PathStatus::LimitReached && eight.expanded == 8 && eight.cells == 0 && eight.cost == 0);
    CHECK(s.path(nullptr, 0) == 0);
}

NNH_TEST(path_every_status_is_reachable) {
    PathSearcher s = searcher();
    CHECK(s.result().status == PathStatus::None);
    const PathMap map = path_test::ascii_map(kWalled, 5);
    s.begin(map, {0, 0}, {4, 4});
    CHECK(s.result().status == PathStatus::Running && s.running());
    s.step(1000);
    CHECK(s.result().status == PathStatus::Found);
    CHECK(s.find(map, {0, 0}, {2, 2}).status == PathStatus::NoPath);
    CHECK(s.find(map, {0, 0}, {4, 4}, {.max_expanded = 3}).status == PathStatus::LimitReached);
    CHECK(s.find(map, {0, 0}, {2, 2}, {.partial = true}).status == PathStatus::Partial);
}

NNH_TEST(path_partial_goes_to_the_closest_reached_cell_ties_by_g_then_index) {
    // The goal sits in a sealed box. Reached cells at h 20: (2, 0) and (0, 2) with g 20, (4, 2) and (2, 4) dearer;
    // the lower row-major index wins: (2, 0).
    const PathMap map = path_test::ascii_map(kWalled, 5);
    PathSearcher s = searcher();
    const PathResult r = s.find(map, {0, 0}, {2, 2}, {.partial = true});
    CHECK(r.status == PathStatus::Partial && r.cost == 20 && r.cells == 3);
    CellPos cells[8];
    s.path(cells, 8);
    CHECK(cells[0] == (CellPos{0, 0}) && cells[2] == (CellPos{2, 0}));
    const auto ref = path_ref::literal_astar(path_ref::ref_of(map), {0, 0}, {2, 2}, {.partial = true});
    CHECK(ref.cost == r.cost && ref.path.back() == cells[2] && ref.expanded == r.expanded);
}

NNH_TEST(path_partial_ties_on_h_go_to_the_lower_g) {
    // The goal (1, 3) is blocked. (1, 2), (0, 3) and (2, 3) all have h 10; (1, 2) is the cheapest (g 20).
    const char* rows[] = {"...", "...", "9.1", ".#."};
    const PathMap map = path_test::ascii_map(rows, 4);
    PathSearcher s = searcher();
    const PathResult r = s.find(map, {1, 0}, {1, 3}, {.partial = true});
    CellPos cells[8];
    s.path(cells, 8);
    CHECK(r.status == PathStatus::Partial && cells[r.cells - 1] == (CellPos{1, 2}));
    // With (1, 2) blocked, (0, 3) and (2, 3) tie at h 10: (2, 3) costs 34, (0, 3) goes through the 9 (more).
    const char* walls[] = {"...", "...", "9#1", ".#."};
    const PathMap m2 = path_test::ascii_map(walls, 4, nnh::Diagonals::Always);
    const PathResult r2 = s.find(m2, {1, 0}, {1, 3}, {.partial = true});
    const auto ref = path_ref::literal_astar(path_ref::ref_of(m2), {1, 0}, {1, 3}, {.partial = true});
    s.path(cells, 8);
    CHECK(r2.status == PathStatus::Partial && cells[r2.cells - 1] == ref.path.back() && r2.cost == ref.cost);
    CHECK(ref.path.back() == (CellPos{2, 3}) && r2.cost == 34);
}

NNH_TEST(path_partial_to_an_island_stops_on_the_shore) {
    const char* rows[] = {
        "S.......",
        "..####..",
        "..#..#..",
        "..#.G#..",
        "..####..",
    };
    const PathMap map = path_test::ascii_map(rows, 5);
    PathSearcher s = searcher();
    const PathResult r = s.find(map, {0, 0}, {4, 3}, {.partial = true});
    CellPos cells[16];
    s.path(cells, 16);
    const CellPos shore = cells[r.cells - 1];
    CHECK(r.status == PathStatus::Partial);
    CHECK(!(shore.x >= 2 && shore.x <= 5 && shore.y >= 1 && shore.y <= 4)); // outside the ring
    CHECK(path_test::find_mark(rows, 5, 'G') == (CellPos{4, 3}));
    const auto ref = path_ref::literal_astar(path_ref::ref_of(map), {0, 0}, {4, 3}, {.partial = true});
    CHECK(shore == ref.path.back() && r.cost == ref.cost);
}

NNH_TEST(path_partial_with_a_blocked_goal_or_a_limit) {
    const char* rows[] = {"S...#"};
    const PathMap map = path_test::ascii_map(rows, 1);
    PathSearcher s = searcher();
    const PathResult blocked = s.find(map, {0, 0}, {4, 0}, {.partial = true});
    CHECK(blocked.status == PathStatus::Partial && blocked.cells == 4 && blocked.cost == 30);
    const PathResult limited = s.find(map, {0, 0}, {3, 0}, {.max_expanded = 2, .partial = true});
    CHECK(limited.status == PathStatus::Partial && limited.cells == 3 && limited.expanded == 2);
    const char* shut[] = {".#", "#."};
    const PathResult alone = s.find(path_test::ascii_map(shut, 2), {0, 0}, {1, 1}, {.partial = true});
    CHECK(alone.status == PathStatus::Partial && alone.cells == 1 && alone.cost == 0);
}

NNH_TEST(path_open_set_never_overflows_on_a_comb_map) {
    // Teeth: every even row open, odd rows blocked but for the spine (column 0). Most cells sit on the frontier.
    const int side = 63;
    std::vector<std::string> text(side, std::string(side, '.'));
    for (int y = 1; y < side; y += 2) {
        for (int x = 1; x < side; ++x) text[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = '#';
    }
    for (int y = 0; y < side; ++y) text[static_cast<std::size_t>(y)][static_cast<std::size_t>(side - 1)] = static_cast<char>('1' + y % 9);
    std::vector<const char*> rows;
    for (const std::string& t : text) rows.push_back(t.c_str());
    for (nnh::Diagonals mode : {nnh::Diagonals::Never, nnh::Diagonals::Always}) {
        const PathMap map = path_test::ascii_map(rows.data(), side, mode);
        PathSearcher s = searcher(side); // exactly the map's size
        const path_ref::RefMap ref = path_ref::ref_of(map);
        for (int scale : {16, 64}) {
            const PathOptions o{.partial = true, .heuristic_scale = scale};
            const PathResult r = s.find(map, {0, 0}, {side - 2, side - 2}, o); // a blocked goal: everything expanded
            const auto want = path_ref::literal_astar(ref, {0, 0}, {side - 2, side - 2}, o);
            CHECK(r.status == want.status && r.expanded == want.expanded && r.cost == want.cost);
        }
    }
}

NNH_TEST(path_without_partial_unreachable_and_limit_give_no_cells) {
    const PathMap map = path_test::ascii_map(kWalled, 5);
    PathSearcher s = searcher();
    const PathResult none = s.find(map, {0, 0}, {2, 2});
    CHECK(none.status == PathStatus::NoPath && none.cells == 0);
    const PathResult lim = s.find(map, {0, 0}, {4, 4}, {.max_expanded = 2});
    CHECK(lim.status == PathStatus::LimitReached && lim.cells == 0 && lim.expanded == 2);
    CellPos c[4];
    CHECK(s.path(c, 4) == 0);
}
