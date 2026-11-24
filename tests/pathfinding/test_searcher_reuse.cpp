// Searcher reuse (spec 025 FR-7; T3.3): 10 000 searches on one searcher equal fresh searchers'; maps smaller than the
// searcher; across the stamp's wrap; two searchers sliced over one map, interleaved; record and entry sizes.

#include <vector>

#include "path_fixture.hpp"
#include "path_reference.hpp"
#include "pathfinding/search_internal.hpp"

using nnh::CellPos;
using nnh::PathMap;
using nnh::PathResult;
using nnh::PathSearcher;

namespace {

bool same(const PathResult& a, const PathResult& b) {
    return a.status == b.status && a.cost == b.cost && a.cells == b.cells && a.expanded == b.expanded;
}

std::vector<CellPos> cells_of(const PathSearcher& s) {
    std::vector<CellPos> v(static_cast<std::size_t>(s.path(nullptr, 0)));
    s.path(v.data(), static_cast<int>(v.size()));
    return v;
}

} // namespace

NNH_TEST(path_one_searcher_reused_10000_times_equals_fresh_searchers) {
    PathSearcher reused = path_test::take(PathSearcher::create(24, 24));
    int differences = 0;
    for (unsigned i = 0; i < 10000; ++i) {
        const path_ref::RandomMap m = path_ref::random_map(i / 20 + 20000, 24); // 20 searches per map
        const CellPos from = path_ref::random_cell(m.ref, i, 1);
        const CellPos to = path_ref::random_cell(m.ref, i, 2);
        const nnh::PathOptions o{.partial = i % 3 == 0};
        const PathResult a = reused.find(m.map, from, to, o);
        PathSearcher fresh = path_test::take(PathSearcher::create(m.ref.w, m.ref.h));
        const PathResult b = fresh.find(m.map, from, to, o);
        if (!same(a, b) || cells_of(reused) != cells_of(fresh)) ++differences;
    }
    CHECK(differences == 0);
}

NNH_TEST(path_searches_across_the_stamp_wrap_equal_fresh_ones) {
    PathSearcher s = path_test::take(PathSearcher::create(32, 32));
    nnh::detail::path_searcher_set_stamp(s, nnh::detail::kMaxStamp - 3);
    for (unsigned i = 0; i < 12; ++i) {
        const path_ref::RandomMap m = path_ref::random_map(i % 2 + 31000, 32);
        const CellPos from = path_ref::random_cell(m.ref, i, 1);
        const CellPos to = path_ref::random_cell(m.ref, i, 2);
        const PathResult a = s.find(m.map, from, to, {.partial = true});
        PathSearcher fresh = path_test::take(PathSearcher::create(32, 32));
        CHECK(same(a, fresh.find(m.map, from, to, {.partial = true})));
        CHECK(cells_of(s) == cells_of(fresh));
    }
    CHECK(nnh::detail::PathAccess::searcher(s)->stamp < 20);
}

NNH_TEST(path_two_searchers_sliced_over_one_map_interleaved) {
    const path_ref::RandomMap m = path_ref::random_map(4242, 48, 1);
    PathSearcher a = path_test::take(PathSearcher::create(48, 48));
    PathSearcher b = path_test::take(PathSearcher::create(48, 48));
    PathSearcher ref = path_test::take(PathSearcher::create(48, 48));
    const CellPos a0 = path_ref::random_cell(m.ref, 1, 1), a1 = path_ref::random_cell(m.ref, 1, 2);
    const CellPos b0 = path_ref::random_cell(m.ref, 2, 1), b1 = path_ref::random_cell(m.ref, 2, 2);
    a.begin(m.map, a0, a1, {.partial = true});
    b.begin(m.map, b0, b1, {.partial = true});
    bool da = false, db = false;
    while (!da || !db) {
        if (!da) da = a.step(3);
        if (!db) db = b.step(5);
    }
    CHECK(same(a.result(), ref.find(m.map, a0, a1, {.partial = true})) && cells_of(a) == cells_of(ref));
    CHECK(same(b.result(), ref.find(m.map, b0, b1, {.partial = true})) && cells_of(b) == cells_of(ref));
}

NNH_TEST(path_search_records_are_8_bytes_and_entries_16) {
    CHECK(sizeof(nnh::detail::SearchRecord) == 8);
    CHECK(sizeof(nnh::detail::OpenEntry) == 16);
}
