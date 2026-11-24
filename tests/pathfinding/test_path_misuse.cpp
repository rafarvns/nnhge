// Searcher misuse (spec 025 FR-23; T3.5): the rows of the plan's misuse table not covered beside their feature — calls
// on empty or moved-from searchers, step() without begin(), negative step sizes, bad path buffers. Each asserts in
// Debug/Development and is harmless in Release.

#include <initializer_list>

#include "path_fixture.hpp"

using nnh::CellPos;
using nnh::PathMap;
using nnh::PathResult;
using nnh::PathSearcher;
using nnh::PathStatus;

namespace {

PathSearcher* g_s = nullptr;
PathSearcher* g_fresh = nullptr; // never begun; made by the test: an assert longjmps past a local's destructor (a leak)
const PathMap* g_map = nullptr;
int g_n = 0;
bool g_done = false;

void step_without_begin() { g_done = g_fresh->step(3); }
void negative_step() { g_done = g_s->step(-1); }
void path_null_with_capacity() { g_n = g_s->path(nullptr, 3); }
void path_negative_capacity() {
    CellPos c[2];
    g_n = g_s->path(c, -2);
}
void points_null_with_capacity() { g_n = g_s->path_points(*g_map, nullptr, 2); }
void points_on_an_empty_map() {
    nnh::Vec2 p[4];
    g_n = g_s->path_points(PathMap{}, p, 4);
}
void begin_on_an_empty_searcher() {
    PathSearcher empty;
    empty.begin(*g_map, {0, 0}, {1, 0});
    g_done = empty.step(5);
}

} // namespace

NNH_TEST(path_searcher_misuse_asserts_and_is_harmless_in_release) {
    const char* rows[] = {"S..", "..G"};
    const PathMap map = path_test::ascii_map(rows, 2);
    PathSearcher s = path_test::take(PathSearcher::create(4, 4));
    PathSearcher fresh = path_test::take(PathSearcher::create(4, 4));
    g_s = &s;
    g_fresh = &fresh;
    g_map = &map;
    const PathResult found = s.find(map, {0, 0}, {2, 1});
    CHECK(found.status == PathStatus::Found);
    for (auto fn : {step_without_begin, negative_step, path_null_with_capacity, path_negative_capacity,
                    points_null_with_capacity, points_on_an_empty_map, begin_on_an_empty_searcher}) {
        g_n = 7;
        g_done = true; // the step() cases overwrite it: true in Release (finished or nothing runs)
        CHECK(path_test::asserts_or_is_ignored(fn));
        if (!nnh::build::kAssertsEnabled) {
            CHECK(g_n == 0 || g_n == 7);
            CHECK(g_done);
        }
    }
    // Nothing changed for the finished search.
    CHECK(s.result().status == PathStatus::Found && s.path(nullptr, 0) == found.cells);
}

NNH_TEST(path_moved_from_searcher_answers_nothing) {
    PathSearcher a = path_test::take(PathSearcher::create(4, 4));
    PathSearcher b = static_cast<PathSearcher&&>(a);
    CHECK(!a.valid() && b.valid() && a.width() == 0);
    CHECK(a.result().status == PathStatus::None && !a.running() && a.path(nullptr, 0) == 0);
    CHECK(!a.reached({0, 0}) && a.cost_to({0, 0}) == nnh::kUnreachable);
    PathSearcher c;
    c = static_cast<PathSearcher&&>(b);
    CHECK(c.valid() && c.width() == 4);
}
