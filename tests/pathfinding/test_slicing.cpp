// Time slicing (spec 025 FR-14, FR-23; T3.2): slices of any size equal find(); cancel; begin while running restarts;
// an edit mid-search flags the result stale and the search goes on over the current cells; a map moved mid-search is
// fine; a map destroyed mid-search asserts and the searcher ends Invalid and frees its block.

#include <new>
#include <vector>

#include "nnh/memory.hpp"
#include "path_fixture.hpp"
#include "path_reference.hpp"

using nnh::CellPos;
using nnh::PathMap;
using nnh::PathOptions;
using nnh::PathResult;
using nnh::PathSearcher;
using nnh::PathStatus;

namespace {

bool same(const PathResult& a, const PathResult& b) {
    return a.status == b.status && a.cost == b.cost && a.cells == b.cells && a.expanded == b.expanded &&
           a.stale == b.stale;
}

std::vector<CellPos> cells_of(const PathSearcher& s) {
    std::vector<CellPos> v(static_cast<std::size_t>(s.path(nullptr, 0)));
    s.path(v.data(), static_cast<int>(v.size()));
    return v;
}

} // namespace

NNH_TEST(path_slices_of_1_7_64_and_the_whole_equal_find_on_300_searches) {
    PathSearcher whole = path_test::take(PathSearcher::create(40, 40));
    PathSearcher sliced = path_test::take(PathSearcher::create(40, 40));
    const int sizes[4] = {1, 7, 64, 1 << 30};
    for (unsigned seed = 0; seed < 300; ++seed) {
        const path_ref::RandomMap m = path_ref::random_map(seed + 9000, 40);
        const CellPos from = path_ref::random_cell(m.ref, seed, 1);
        const CellPos to = path_ref::random_cell(m.ref, seed, 2);
        const PathOptions o{.max_expanded = seed % 3 == 0 ? 50 : 0, .partial = seed % 2 == 0};
        const PathResult want = whole.find(m.map, from, to, o);
        const std::vector<CellPos> want_cells = cells_of(whole);
        const int size = sizes[seed % 4];
        sliced.begin(m.map, from, to, o);
        int calls = 0;
        while (!sliced.step(size)) ++calls;
        CHECK(same(sliced.result(), want));
        CHECK(cells_of(sliced) == want_cells);
        if (size == 1 && want.expanded > 2) CHECK(calls >= want.expanded - 1);
    }
}

NNH_TEST(path_cancel_ends_a_search_and_begin_restarts_one) {
    const char* rows[] = {"S.........", "..........", ".........G"};
    const PathMap map = path_test::ascii_map(rows, 3);
    PathSearcher s = path_test::take(PathSearcher::create(16, 16));
    s.begin(map, {0, 0}, {9, 2});
    CHECK(!s.step(2) && s.running());
    s.cancel();
    CHECK(!s.running() && s.result().status == PathStatus::None);
    s.begin(map, {0, 0}, {9, 2});
    s.step(3);
    s.begin(map, {9, 2}, {0, 0}); // restarts
    while (!s.step(5)) {
    }
    PathSearcher fresh = path_test::take(PathSearcher::create(16, 16));
    CHECK(same(s.result(), fresh.find(map, {9, 2}, {0, 0})));
    CHECK(s.step(4)); // finished: true, unchanged
    CHECK(s.result().status == PathStatus::Found);
}

NNH_TEST(path_edit_mid_search_flags_stale_and_uses_the_current_cells) {
    const char* rows[] = {
        "S........",
        ".#######.",
        "........G",
    };
    PathMap map = path_test::ascii_map(rows, 3);
    PathSearcher s = path_test::take(PathSearcher::create(16, 16));
    s.begin(map, {0, 0}, {8, 2});
    s.step(3);
    map.set_cell(6, 0, nnh::kPathBlocked); // the top route closes ahead of the search
    while (!s.step(4)) {
    }
    const PathResult r = s.result();
    CHECK(r.status == PathStatus::Found && r.stale);
    for (const CellPos c : cells_of(s)) CHECK(!(c == CellPos{6, 0}));
    const PathResult fresh = s.find(map, {0, 0}, {8, 2});
    CHECK(!fresh.stale);
}

NNH_TEST(path_map_moved_mid_search_gives_the_same_result) {
    const path_ref::RandomMap m = path_ref::random_map(77, 30, 1);
    PathSearcher a = path_test::take(PathSearcher::create(30, 30));
    PathSearcher b = path_test::take(PathSearcher::create(30, 30));
    const PathResult want = a.find(m.map, {0, 0}, {m.ref.w - 1, m.ref.h - 1}, {.partial = true});
    PathMap moving;
    {
        path_ref::RandomMap copy = path_ref::random_map(77, 30, 1);
        moving = static_cast<PathMap&&>(copy.map);
    }
    b.begin(moving, {0, 0}, {m.ref.w - 1, m.ref.h - 1}, {.partial = true});
    b.step(5);
    PathMap moved = static_cast<PathMap&&>(moving);
    while (!b.step(5)) {
    }
    CHECK(same(b.result(), want));
}

namespace {

alignas(PathMap) unsigned char g_storage[sizeof(PathMap)];
PathMap* g_doomed = nullptr;
void destroy_the_map() { g_doomed->~PathMap(); }

} // namespace

NNH_TEST(path_map_destroyed_mid_search_asserts_and_the_searcher_ends_invalid) {
    const int live = nnh::mem::live_allocations();
    {
        PathSearcher s = path_test::take(PathSearcher::create(16, 16));
        const char* rows[] = {"S.........", "..........", ".........G"};
        g_doomed = ::new (g_storage) PathMap(path_test::ascii_map(rows, 3));
        s.begin(*g_doomed, {0, 0}, {9, 2});
        s.step(2);
        CHECK(path_test::asserts_or_is_ignored(destroy_the_map));
        CHECK(nnh::mem::live_allocations() == live + 2); // the doomed block lives on
        CHECK(s.step(10));
        CHECK(s.result().status == PathStatus::Invalid && s.result().cells == 0);
        CHECK(nnh::mem::live_allocations() == live + 1); // freed by its last user
    }
    CHECK(nnh::mem::live_allocations() == live);
}

NNH_TEST(path_map_destroyed_mid_search_is_freed_by_a_cancel_or_the_searcher_going) {
    const int live = nnh::mem::live_allocations();
    const char* rows[] = {"S.........", ".........G"};
    {
        PathSearcher s = path_test::take(PathSearcher::create(16, 16));
        g_doomed = ::new (g_storage) PathMap(path_test::ascii_map(rows, 2));
        s.begin(*g_doomed, {0, 0}, {9, 1});
        CHECK(path_test::asserts_or_is_ignored(destroy_the_map));
        s.cancel();
        CHECK(nnh::mem::live_allocations() == live + 1);
        g_doomed = ::new (g_storage) PathMap(path_test::ascii_map(rows, 2));
        s.begin(*g_doomed, {0, 0}, {9, 1});
        CHECK(path_test::asserts_or_is_ignored(destroy_the_map));
    }
    CHECK(nnh::mem::live_allocations() == live);
}
