// Costs (spec 025 FR-21, FR-22; T3.5): after creation nothing allocates over 10 000 operations, nothing logs in the
// normal path, no event is published, and the weight callback is the only game code ever called (only by sync).

#include "alloc_counter.hpp"
#include "nnh/app.hpp"
#include "nnh/events.hpp"
#include "nnh/memory.hpp"
#include "path_fixture.hpp"

using nnh::CellPos;
using nnh::PathMap;
using nnh::PathSearcher;

namespace {

int g_weight_calls = 0;
int counting_weight(const void*, int gx, int gy) {
    ++g_weight_calls;
    return 1 + (gx * 7 + gy * 3) % 5;
}

nnh::collision::Cell cell_of(const void*, int x, int y) {
    return (x * 13 + y * 7) % 6 == 0 ? nnh::collision::Cell::Solid : nnh::collision::Cell::Empty;
}

} // namespace

NNH_TEST(path_operations_allocate_nothing_log_nothing_and_publish_nothing_after_creation) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    PathMap map = path_test::take(PathMap::create({.width = 40, .height = 30}));
    PathSearcher s = path_test::take(PathSearcher::create(40, 30));
    nnh::collision::Grid grid;
    grid.width = 40;
    grid.height = 30;
    grid.cell = cell_of;
    CellPos cells[256];
    nnh::Vec2 points[256];
    const auto news = alloc_counter::news();
    const nnh::MemStats heap = nnh::mem::region_stats(nnh::MemRegion::Heap);
    const int lines = nnh::platform_fake::state().console_lines;
    const nnh::events::Stats events = nnh::events::stats();
    g_weight_calls = 0;
    map.sync(grid, {counting_weight, nullptr});
    const int sync_calls = g_weight_calls;
    long long sum = 0;
    for (unsigned i = 0; i < 10000; ++i) {
        const unsigned h = path_test::hash4(i, 1, 0, 31);
        const CellPos a{static_cast<int>(h % 40), static_cast<int>((h >> 8) % 30)};
        const CellPos b{static_cast<int>((h >> 16) % 40), static_cast<int>((h >> 24) % 30)};
        switch (i % 5) {
        case 0: sum += s.find(map, a, b, {.max_expanded = 200, .partial = true}).cost; break;
        case 1:
            s.begin(map, a, b);
            while (!s.step(16)) {
            }
            sum += s.path(cells, 256);
            break;
        case 2: map.set_cell(a.x, a.y, 1 + static_cast<int>(h % 9)); break;
        case 3: map.sync(grid, {a.x, a.y, 3, 3}); break;
        default: sum += s.path_points(map, points, 256) + (map.blocked(a.x, a.y) ? 1 : 0); break;
        }
    }
    CHECK(g_weight_calls == sync_calls); // searches, edits and region syncs without weights call no game code
    CHECK(alloc_counter::news() == news);
    const nnh::MemStats after = nnh::mem::region_stats(nnh::MemRegion::Heap);
    CHECK(after.count == heap.count && after.bytes == heap.bytes);
    CHECK(nnh::platform_fake::state().console_lines == lines);
    CHECK(nnh::events::stats().dropped_events == events.dropped_events);
    CHECK(nnh::events::stats().subscribers == events.subscribers);
    CHECK(sum > 0);
    nnh::shutdown();
}

NNH_TEST(path_weight_callback_runs_once_per_walkable_cell_and_only_in_sync) {
    PathMap map = path_test::take(PathMap::create({.width = 40, .height = 30}));
    nnh::collision::Grid grid;
    grid.width = 40;
    grid.height = 30;
    grid.cell = cell_of;
    int walkable = 0;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 40; ++x) walkable += cell_of(nullptr, x, y) == nnh::collision::Cell::Empty ? 1 : 0;
    }
    g_weight_calls = 0;
    map.sync(grid, {counting_weight, nullptr});
    CHECK(g_weight_calls == walkable);
    PathSearcher s = path_test::take(PathSearcher::create(40, 30));
    s.find(map, {0, 1}, {39, 29});
    CHECK(g_weight_calls == walkable);
}
