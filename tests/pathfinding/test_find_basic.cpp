// One-shot searches (spec 025 FR-7, FR-8, FR-11; T2.4): statuses, cost, cells and expansions; from == to; a blocked
// start leaves; a blocked goal answers at once; path buffers; cells outside the map.

#include <initializer_list>

#include "nnh/memory.hpp"
#include "path_fixture.hpp"
#include "path_reference.hpp"
#include "pathfinding/path_block.hpp"

using nnh::CellPos;
using nnh::PathMap;
using nnh::PathResult;
using nnh::PathSearcher;
using nnh::PathStatus;

namespace {

PathSearcher searcher(int w = 16, int h = 16) { return path_test::take(PathSearcher::create(w, h)); }

const char* kRoom[] = {
    "S...#....",
    ".##.#.##.",
    ".#..#..#.",
    ".#.###.#.",
    ".#.....#G",
};

} // namespace

NNH_TEST(path_searcher_create_refuses_bad_sizes_and_takes_one_block) {
    CHECK(PathSearcher::create(0, 4).error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(PathSearcher::create(4, 1025).error().code == nnh::ErrorCode::InvalidArgument);
    static NNH_MEM_TAG(searchers);
    {
        PathSearcher s = path_test::take(PathSearcher::create(10, 6, &NNH_MEM_TAG_REF(searchers)));
        CHECK(s.valid() && s.width() == 10 && s.height() == 6);
        const nnh::MemStats st = nnh::mem::tag_stats(NNH_MEM_TAG_REF(searchers));
        CHECK(st.count == 1);
        // 8 bytes per padded cell + 16 per cell (the open set) + the header.
        CHECK(st.bytes >= 8u * 12 * 8 + 16u * 60 && st.bytes <= 8u * 12 * 8 + 16u * 60 + 512);
    }
    CHECK(nnh::mem::tag_stats(NNH_MEM_TAG_REF(searchers)).count == 0);
}

NNH_TEST(path_find_reports_found_with_cost_cells_and_expansions) {
    const PathMap map = path_test::ascii_map(kRoom, 5);
    PathSearcher s = searcher();
    const CellPos from = path_test::find_mark(kRoom, 5, 'S');
    const CellPos to = path_test::find_mark(kRoom, 5, 'G');
    const PathResult r = s.find(map, from, to);
    const path_ref::RefResult ref = path_ref::literal_astar(path_ref::ref_of(map), from, to);
    CHECK(r.status == PathStatus::Found && !r.stale);
    CHECK(r.cost == ref.cost && r.cells == static_cast<int>(ref.path.size()) && r.expanded == ref.expanded);
    CellPos cells[64];
    CHECK(s.path(cells, 64) == r.cells);
    CHECK(cells[0] == from && cells[r.cells - 1] == to);
    for (int i = 0; i < r.cells; ++i) CHECK(cells[i] == ref.path[static_cast<std::size_t>(i)]);
    CHECK(s.result().status == PathStatus::Found && !s.running());
}

NNH_TEST(path_find_reports_no_path_when_every_reachable_cell_is_expanded) {
    const char* rows[] = {"S.#.", "..#G"};
    const PathMap map = path_test::ascii_map(rows, 2);
    PathSearcher s = searcher();
    const PathResult r = s.find(map, {0, 0}, {3, 1});
    CHECK(r.status == PathStatus::NoPath && r.cells == 0 && r.cost == 0 && r.expanded == 4);
    CHECK(s.path(nullptr, 0) == 0);
}

NNH_TEST(path_find_from_equal_to_is_found_with_one_cell_walkable_or_not) {
    const char* rows[] = {".#."};
    const PathMap map = path_test::ascii_map(rows, 1);
    PathSearcher s = searcher();
    for (const CellPos c : {CellPos{0, 0}, CellPos{1, 0}}) {
        const PathResult r = s.find(map, c, c);
        CHECK(r.status == PathStatus::Found && r.cells == 1 && r.cost == 0 && r.expanded == 0);
        CellPos out[2] = {};
        CHECK(s.path(out, 2) == 1 && out[0] == c);
    }
}

NNH_TEST(path_find_leaves_a_blocked_start_and_counts_only_entered_cells) {
    const char* rows[] = {"#.9."};
    const PathMap map = path_test::ascii_map(rows, 1);
    PathSearcher s = searcher();
    const PathResult r = s.find(map, {0, 0}, {3, 0});
    CHECK(r.status == PathStatus::Found && r.cost == 10 + 90 + 10 && r.cells == 4);
}

NNH_TEST(path_find_answers_a_blocked_goal_at_once) {
    const char* rows[] = {"....", "...#"};
    const PathMap map = path_test::ascii_map(rows, 2);
    PathSearcher s = searcher();
    const PathResult r = s.find(map, {0, 0}, {3, 1});
    CHECK(r.status == PathStatus::NoPath && r.expanded == 0 && r.cells == 0);
}

NNH_TEST(path_buffer_exact_fit_too_small_and_counting) {
    const char* rows[] = {"S.....G"};
    const PathMap map = path_test::ascii_map(rows, 1);
    PathSearcher s = searcher();
    CHECK(s.find(map, {0, 0}, {6, 0}).cells == 7);
    CellPos exact[7];
    CHECK(s.path(exact, 7) == 7 && exact[6] == (CellPos{6, 0}));
    CellPos small[4] = {{9, 9}, {9, 9}, {9, 9}, {9, 9}};
    CHECK(s.path(small, 3) == 7);
    CHECK(small[0] == (CellPos{0, 0}) && small[2] == (CellPos{2, 0}) && small[3] == (CellPos{9, 9}));
    CHECK(s.path(nullptr, 0) == 7);
    nnh::Vec2 points[7];
    CHECK(s.path_points(map, points, 7) == 7);
    CHECK(points[0] == map.cell_center({0, 0}) && points[6] == map.cell_center({6, 0}));
    // Readable until the next search.
    CHECK(s.path(exact, 7) == 7);
    s.find(map, {2, 0}, {3, 0});
    CHECK(s.path(exact, 7) == 2 && exact[0] == (CellPos{2, 0}));
}

NNH_TEST(path_find_over_a_map_smaller_than_the_searcher) {
    const char* rows[] = {"S..", "##.", "G.."};
    const PathMap map = path_test::ascii_map(rows, 3);
    PathSearcher s = searcher(40, 30);
    const PathResult r = s.find(map, {0, 0}, {0, 2});
    CHECK(r.status == PathStatus::Found && r.cost == path_ref::literal_astar(path_ref::ref_of(map), {0, 0}, {0, 2}).cost);
}

namespace {

PathSearcher* g_s = nullptr;
const PathMap* g_map = nullptr;
// Made by the test, not inside the asserting calls: an assert longjmps past a local's destructor (a leak).
PathSearcher* g_small = nullptr; // 4 × 4
const PathMap* g_big = nullptr;  // 19 × 1
PathResult g_r;
void from_outside() { g_r = g_s->find(*g_map, {-1, 0}, {1, 1}); }
void to_outside() { g_r = g_s->find(*g_map, {0, 0}, {1, 9}); }
void map_too_big() { g_r = g_small->find(*g_big, {0, 0}, {1, 0}); }
void empty_map() {
    const PathMap empty;
    g_r = g_s->find(empty, {0, 0}, {1, 0});
}
void negative_limit() { g_r = g_s->find(*g_map, {0, 0}, {1, 1}, {.max_expanded = -1}); }
void scale_too_low() { g_r = g_s->find(*g_map, {0, 0}, {1, 1}, {.heuristic_scale = 15}); }
void scale_too_high() { g_r = g_s->find(*g_map, {0, 0}, {1, 1}, {.heuristic_scale = 65}); }

} // namespace

NNH_TEST(path_find_misuse_asserts_and_is_invalid_in_release) {
    const char* rows[] = {"..", ".."};
    const PathMap map = path_test::ascii_map(rows, 2);
    PathSearcher s = searcher();
    const char* long_row[] = {"..................."};
    const PathMap big = path_test::ascii_map(long_row, 1);
    PathSearcher small = path_test::take(PathSearcher::create(4, 4));
    g_s = &s;
    g_map = &map;
    g_small = &small;
    g_big = &big;
    for (auto fn : {from_outside, to_outside, map_too_big, empty_map, negative_limit, scale_too_low, scale_too_high}) {
        g_r = {};
        CHECK(path_test::asserts_or_is_ignored(fn));
        if (!nnh::build::kAssertsEnabled) CHECK(g_r.status == PathStatus::Invalid && g_r.cells == 0);
    }
}
