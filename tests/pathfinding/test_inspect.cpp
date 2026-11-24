// Inspecting a search (spec 025 FR-20; T3.4): reached, expanded and cost_to during a sliced search and after.

#include <initializer_list>

#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::PathMap;
using nnh::PathSearcher;

namespace {

int count_expanded(const PathSearcher& s, int w, int h) {
    int n = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) n += s.expanded({x, y}) ? 1 : 0;
    }
    return n;
}

} // namespace

NNH_TEST(path_inspection_during_a_sliced_search_and_after) {
    const path_ref::RandomMap m = path_ref::random_map(606, 32, 1);
    const int w = m.ref.w;
    const int h = m.ref.h;
    PathSearcher s = path_test::take(PathSearcher::create(32, 32));
    const CellPos from{0, 0};
    const CellPos to{w - 1, h - 1};
    s.begin(m.map, from, to, {.partial = true});
    CHECK(s.reached(from) && s.cost_to(from) == 0 && !s.expanded(from));
    if (!s.step(5)) CHECK(count_expanded(s, w, h) == 5);
    while (!s.step(7)) {
    }
    const nnh::PathResult r = s.result();
    CHECK(count_expanded(s, w, h) == r.expanded);
    const auto dist = path_ref::dijkstra(m.ref, from);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (s.expanded({x, y})) {
                CHECK(s.reached({x, y}));
                CHECK(s.cost_to({x, y}) == dist[static_cast<std::size_t>(y * w + x)]); // closed g is optimal
            }
            if (!s.reached({x, y})) CHECK(s.cost_to({x, y}) == nnh::kUnreachable);
        }
    }
    CellPos cells[32 * 32];
    const int n = s.path(cells, 32 * 32);
    if (n > 0) CHECK(s.cost_to(cells[n - 1]) == r.cost);
}

NNH_TEST(path_inspection_of_a_new_search_forgets_the_last_one) {
    const char* rows[] = {"S...", "...G"};
    const PathMap map = path_test::ascii_map(rows, 2);
    PathSearcher s = path_test::take(PathSearcher::create(8, 8));
    CHECK(!s.reached({0, 0}) && s.cost_to({0, 0}) == nnh::kUnreachable);
    s.find(map, {0, 0}, {3, 1});
    CHECK(s.reached({1, 0}));
    s.find(map, {3, 1}, {3, 1}); // from == to: only the start
    CHECK(!s.reached({1, 0}) && s.reached({3, 1}));
}

namespace {

const PathSearcher* g_s = nullptr;
bool g_answer = true;
void reached_outside() { g_answer = g_s->reached({5, 0}); }
void expanded_outside() { g_answer = g_s->expanded({0, -1}); }
void cost_outside() { g_answer = g_s->cost_to({4, 2}) != nnh::kUnreachable; }

} // namespace

NNH_TEST(path_inspection_outside_the_map_asserts_and_answers_nothing) {
    const char* rows[] = {"S...", "...G"};
    const PathMap map = path_test::ascii_map(rows, 2);
    PathSearcher s = path_test::take(PathSearcher::create(8, 8));
    s.find(map, {0, 0}, {3, 1});
    g_s = &s;
    for (auto fn : {reached_outside, expanded_outside, cost_outside}) {
        g_answer = true;
        CHECK(path_test::asserts_or_is_ignored(fn));
        if (!nnh::build::kAssertsEnabled) CHECK(!g_answer);
    }
}
