// Syncing a path map from a grid (spec 025 FR-3, FR-23; T1.2): array and callback grids, a tilemap's filtered grids,
// a region offset reading `outside` past the grid's edges, Empty/Solid/OneWay and the one-way setting, the weight
// callback (once per walkable cell, grid coordinates, row-major, only in sync), a partial sync.

#include "../tilemap/map_fixture.hpp"
#include "path_fixture.hpp"

namespace collision = nnh::collision;
using collision::Cell;
using nnh::PathMap;
using nnh::PathMapDesc;

namespace {

constexpr Cell E = Cell::Empty;
constexpr Cell S = Cell::Solid;
constexpr Cell O = Cell::OneWay;

// 4 x 3
const Cell kCells[12] = {E, S, E, O,  //
                         E, E, S, E,  //
                         O, E, E, E};

PathMap make(int w, int h, int gx = 0, int gy = 0, bool one_way_walkable = false, int agent = 1) {
    PathMapDesc d;
    d.width = w;
    d.height = h;
    d.grid_x = gx;
    d.grid_y = gy;
    d.one_way_walkable = one_way_walkable;
    d.agent_size = agent;
    return path_test::take(PathMap::create(d));
}

struct Calls {
    int count = 0;
    int xs[64] = {};
    int ys[64] = {};
};

int counting_weight(const void* context, int gx, int gy) {
    auto* calls = static_cast<Calls*>(const_cast<void*>(context));
    if (calls->count < 64) {
        calls->xs[calls->count] = gx;
        calls->ys[calls->count] = gy;
    }
    ++calls->count;
    return 1 + (gx + gy) % 9;
}

Cell callback_cell(const void*, int x, int y) { return kCells[y * 4 + x]; }

} // namespace

NNH_TEST(path_sync_maps_empty_solid_and_one_way_from_an_array_grid) {
    PathMap map = make(4, 3);
    map.sync(collision::grid_view(kCells, 4, 3, 16, 16));
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            const Cell c = kCells[y * 4 + x];
            CHECK(map.blocked(x, y) == (c != E));
            if (c == E) CHECK(map.weight(x, y) == 1);
        }
    }
}

NNH_TEST(path_sync_makes_one_way_cells_walkable_when_the_map_says_so) {
    PathMap map = make(4, 3, 0, 0, true);
    map.sync(collision::grid_view(kCells, 4, 3, 16, 16));
    CHECK(!map.blocked(3, 0) && !map.blocked(0, 2));
    CHECK(map.blocked(1, 0) && map.blocked(2, 1));
}

NNH_TEST(path_sync_reads_a_callback_grid_like_an_array) {
    PathMap a = make(4, 3);
    PathMap b = make(4, 3);
    a.sync(collision::grid_view(kCells, 4, 3, 16, 16));
    collision::Grid g;
    g.width = 4;
    g.height = 3;
    g.cell = callback_cell;
    b.sync(g);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) CHECK(a.weight(x, y) == b.weight(x, y));
    }
}

NNH_TEST(path_sync_with_a_region_offset_reads_outside_past_the_grid) {
    collision::Grid g = collision::grid_view(kCells, 4, 3, 16, 16);
    g.outside = S;
    PathMap map = make(3, 3, 2, 1); // grid cells (2..4, 1..3)
    map.sync(g);
    CHECK(map.blocked(0, 0));  // grid (2, 1): Solid
    CHECK(!map.blocked(1, 0)); // grid (3, 1): Empty
    CHECK(map.blocked(2, 0));  // grid (4, 1): outside, Solid
    CHECK(!map.blocked(0, 1)); // grid (2, 2)
    CHECK(map.blocked(0, 2));  // grid (2, 3): outside
    g.outside = E;
    map.sync(g);
    CHECK(!map.blocked(2, 0) && !map.blocked(0, 2) && map.blocked(0, 0));
    PathMap before = make(2, 2, -1, -1); // grid cells (-1..0, -1..0)
    before.sync(g);
    CHECK(!before.blocked(0, 0) && !before.blocked(1, 0) && !before.blocked(0, 1) && !before.blocked(1, 1));
}

NNH_TEST(path_sync_calls_the_weight_callback_once_per_walkable_cell_row_major_with_grid_coordinates) {
    collision::Grid g = collision::grid_view(kCells, 4, 3, 16, 16);
    PathMap map = make(3, 2, 1, 1); // grid (1..3, 1..2): E S E / E E E
    Calls calls;
    map.sync(g, {counting_weight, &calls});
    CHECK(calls.count == 5);
    const int xs[] = {1, 3, 1, 2, 3};
    const int ys[] = {1, 1, 2, 2, 2};
    for (int i = 0; i < 5; ++i) CHECK(calls.xs[i] == xs[i] && calls.ys[i] == ys[i]);
    CHECK(map.weight(0, 0) == 1 + (1 + 1) % 9 && map.weight(2, 1) == 1 + (3 + 2) % 9);
    CHECK(map.blocked(1, 0));
}

namespace {

int blocking_weight(const void*, int gx, int) { return gx == 0 ? nnh::kPathBlocked : 200; }

} // namespace

NNH_TEST(path_sync_weight_callback_may_block_a_cell) {
    PathMap map = make(4, 3);
    map.sync(collision::grid_view(kCells, 4, 3, 16, 16), {blocking_weight, nullptr});
    CHECK(map.blocked(0, 0) && map.blocked(0, 1));
    CHECK(map.weight(2, 0) == 200);
}

NNH_TEST(path_sync_of_a_region_changes_only_its_rectangle) {
    PathMap map = make(4, 3);
    map.fill({0, 0, 4, 3}, 5);
    const std::uint32_t v = map.version();
    map.sync(collision::grid_view(kCells, 4, 3, 16, 16), {1, 0, 2, 2});
    CHECK(map.version() == v + 1);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            const bool inside = x >= 1 && x <= 2 && y <= 1;
            if (!inside) {
                CHECK(map.weight(x, y) == 5);
            } else {
                CHECK(map.blocked(x, y) == (kCells[y * 4 + x] != E));
            }
        }
    }
    map.sync(collision::grid_view(kCells, 4, 3, 16, 16), {3, 2, 10, 10}); // clipped to (3, 2)
    CHECK(map.weight(3, 2) == 1 && map.weight(2, 2) == 5);
}

NNH_TEST(path_sync_reads_a_tilemaps_filtered_grids_one_map_per_mover) {
    map_test::Engine engine;
    nnh::Tilemap level = map_test::make({.width = 5, .height = 1});
    const int first = map_test::tileset(level, {.texture = &engine.sheet});
    const int layer = map_test::layer(level);
    constexpr collision::Layers kWall = collision::layer(0);
    constexpr collision::Layers kWater = collision::layer(1);
    level.set_tile_collision(first, Cell::Solid);     // a wall: solid for everyone
    level.set_tile_collision(first + 1, Cell::Solid); // water
    level.set_tile_collision_layers(first + 1, kWater);
    level.set_tile(layer, 1, 0, map_test::tile(first));
    level.set_tile(layer, 3, 0, map_test::tile(first + 1));
    PathMap walkers = make(5, 1);
    PathMap boats = make(5, 1);
    walkers.sync(level.collision_grid(kWall | kWater));
    boats.sync(level.collision_grid(kWall));
    CHECK(walkers.blocked(1, 0) && walkers.blocked(3, 0) && !walkers.blocked(2, 0));
    CHECK(boats.blocked(1, 0) && !boats.blocked(3, 0));
    PathMap all = make(5, 1);
    all.sync(level.collision_grid());
    CHECK(all.blocked(1, 0) && all.blocked(3, 0) && !all.blocked(0, 0) && !all.blocked(4, 0));
}

namespace {

PathMap* g_map = nullptr;
int bad_weight(const void*, int, int) { return 0; }
void sync_with_a_bad_callback() { g_map->sync(collision::grid_view(kCells, 4, 3, 16, 16), {bad_weight, nullptr}); }
void sync_a_negative_region() { g_map->sync(collision::grid_view(kCells, 4, 3, 16, 16), {0, 0, -1, 2}); }
void sync_a_region_outside() { g_map->sync(collision::grid_view(kCells, 4, 3, 16, 16), {9, 0, 2, 2}); }
void sync_an_empty_map() {
    PathMap empty;
    empty.sync(collision::grid_view(kCells, 4, 3, 16, 16));
}

} // namespace

NNH_TEST(path_sync_misuse_asserts_and_release_clamps_or_ignores) {
    PathMap map = make(4, 3);
    g_map = &map;
    CHECK(path_test::asserts_or_is_ignored(sync_with_a_bad_callback));
    if (!nnh::build::kAssertsEnabled) CHECK(map.weight(0, 0) == 1); // clamped to 1
    const std::uint32_t v = map.version();
    CHECK(path_test::asserts_or_is_ignored(sync_a_negative_region));
    CHECK(path_test::asserts_or_is_ignored(sync_a_region_outside));
    CHECK(map.version() == v);
    CHECK(path_test::asserts_or_is_ignored(sync_an_empty_map));
}
