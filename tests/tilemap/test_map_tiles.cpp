// Reading and changing tiles, world <-> cell coordinates (spec 012 FR-2, FR-4).

#include <cmath>
#include <limits>

#include "map_fixture.hpp"
#include "nnh/build.hpp"
#include "tilemap/tilemap_internal.hpp"

using map_test::make;
using map_test::tile;
using nnh::Rect;
using nnh::Tilemap;
using nnh::Vec2;
namespace fake = nnh::platform_fake;

namespace {

/// A 10 x 6 map of 16x8 tiles, one 8-tile tileset (ids 1..8) and two layers.
struct Level {
    map_test::Engine engine;
    Tilemap map = make({.width = 10, .height = 6, .tile_width = 16, .tile_height = 8});
    int first = map_test::tileset(map, {.texture = &engine.sheet, .tile_count = 8});
    int back = map_test::layer(map, {.name = "back"});
    int front = map_test::layer(map, {.name = "front"});
};

bool same_rect(Rect a, Rect b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; }

Tilemap* g_map = nullptr;
void set_on_bad_layer() { g_map->set_tile(7, 0, 0, tile(1)); }
void set_unknown_id() { g_map->set_tile(0, 0, 0, tile(9)); }
void set_diagonal_on_non_square() { g_map->set_tile(0, 0, 0, tile(1, false, false, true)); }

} // namespace

NNH_TEST(a_tile_set_in_a_cell_is_read_back_with_its_flips) {
    Level l;
    l.map.set_tile(l.back, 3, 2, tile(5, true, false, false));
    l.map.set_tile(l.front, 9, 5, tile(8, false, true, false));
    CHECK(l.map.tile(l.back, 3, 2) == tile(5, true, false, false));
    CHECK(l.map.tile(l.front, 9, 5) == tile(8, false, true, false));
    CHECK(l.map.tile(l.front, 3, 2).empty()); // layers are separate
    CHECK(l.map.tile(l.back, 4, 2).empty() && l.map.tile(l.back, 3, 3).empty());
}

NNH_TEST(every_flip_combination_survives_the_cell_bits) {
    map_test::Engine engine;
    Tilemap map = make({.width = 8, .height = 1});
    map_test::tileset(map, {.texture = &engine.sheet});
    const int l = map_test::layer(map);
    for (int i = 0; i < 8; ++i) map.set_tile(l, i, 0, tile(8, i & 1, i & 2, i & 4));
    for (int i = 0; i < 8; ++i) CHECK(map.tile(l, i, 0) == tile(8, i & 1, i & 2, i & 4));
}

NNH_TEST(an_empty_tile_clears_the_cell_and_its_flips) {
    Level l;
    l.map.set_tile(l.back, 1, 1, tile(3, true, true));
    l.map.set_tile(l.back, 1, 1, tile(0, true, true));
    CHECK(l.map.tile(l.back, 1, 1) == nnh::Tile{});
}

NNH_TEST(tiles_outside_the_map_or_layers_read_as_empty) {
    Level l;
    l.map.fill(l.back, 0, 0, 10, 6, tile(2));
    CHECK(l.map.tile(l.back, -1, 0).empty() && l.map.tile(l.back, 0, -1).empty());
    CHECK(l.map.tile(l.back, 10, 0).empty() && l.map.tile(l.back, 0, 6).empty());
    CHECK(l.map.tile(2, 0, 0).empty() && l.map.tile(-1, 0, 0).empty());
    CHECK(l.map.tile(l.back, 9, 5) == tile(2));
}

NNH_TEST(set_tile_outside_the_map_changes_nothing) {
    Level l;
    l.map.set_tile(l.back, -1, 0, tile(1));
    l.map.set_tile(l.back, 10, 0, tile(1));
    l.map.set_tile(l.back, 0, 6, tile(1));
    l.map.set_tile(l.back, 0, -1, tile(1));
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 10; ++x) CHECK(l.map.tile(l.back, x, y).empty());
    }
}

NNH_TEST(set_tile_misuse_asserts_and_is_ignored) {
    Level l;
    g_map = &l.map;
    CHECK(fake::run_expecting_crash(set_on_bad_layer) == nnh::build::kAssertsEnabled);
    CHECK(fake::run_expecting_crash(set_unknown_id) == nnh::build::kAssertsEnabled);
    CHECK(fake::run_expecting_crash(set_diagonal_on_non_square) == nnh::build::kAssertsEnabled);
    CHECK(l.map.tile(0, 0, 0).empty()); // in Release: ignored
}

NNH_TEST(fill_sets_a_block_of_cells) {
    Level l;
    l.map.fill(l.front, 2, 1, 3, 2, tile(4, true));
    int set = 0;
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 10; ++x) {
            const bool inside = x >= 2 && x < 5 && y >= 1 && y < 3;
            CHECK(l.map.tile(l.front, x, y) == (inside ? tile(4, true) : nnh::Tile{}));
            set += inside ? 1 : 0;
        }
    }
    CHECK(set == 6);
}

NNH_TEST(fill_is_clipped_to_the_map) {
    Level l;
    l.map.fill(l.back, -2, -3, 4, 5, tile(1)); // covers cells 0..1 x 0..1
    CHECK(l.map.tile(l.back, 0, 0) == tile(1) && l.map.tile(l.back, 1, 1) == tile(1));
    CHECK(l.map.tile(l.back, 2, 0).empty() && l.map.tile(l.back, 0, 2).empty());
    l.map.fill(l.back, 8, 4, 100, 100, tile(2));
    CHECK(l.map.tile(l.back, 9, 5) == tile(2) && l.map.tile(l.back, 8, 4) == tile(2));
    CHECK(l.map.tile(l.back, 7, 4).empty());
}

NNH_TEST(fill_with_no_area_changes_nothing) {
    Level l;
    l.map.fill(l.back, 1, 1, 0, 3, tile(1));
    l.map.fill(l.back, 1, 1, 3, -1, tile(1));
    l.map.fill(l.back, 10, 0, 5, 5, tile(1));
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 10; ++x) CHECK(l.map.tile(l.back, x, y).empty());
    }
}

NNH_TEST(cell_rect_covers_a_cell_in_world_pixels) {
    Level l; // 16 x 8 tiles
    CHECK(same_rect(l.map.cell_rect(0, 0), {0, 0, 16, 8}));
    CHECK(same_rect(l.map.cell_rect(3, 2), {48, 16, 16, 8}));
    CHECK(same_rect(l.map.cell_rect(-1, -2), {-16, -16, 16, 8}));
    CHECK(same_rect(l.map.cell_rect(12, 0), {192, 0, 16, 8})); // outside the map too
}

NNH_TEST(world_to_cell_floors_and_says_whether_the_cell_is_inside) {
    Level l; // 10 x 6 cells of 16 x 8
    int x = 99, y = 99;
    CHECK(l.map.world_to_cell({0, 0}, x, y) && x == 0 && y == 0);
    CHECK(l.map.world_to_cell({15.99f, 7.99f}, x, y) && x == 0 && y == 0);
    CHECK(l.map.world_to_cell({16, 8}, x, y) && x == 1 && y == 1);
    CHECK(l.map.world_to_cell({159.9f, 47.9f}, x, y) && x == 9 && y == 5);
    CHECK(!l.map.world_to_cell({160, 0}, x, y) && x == 10 && y == 0);
    CHECK(!l.map.world_to_cell({0, 48}, x, y) && x == 0 && y == 6);
}

NNH_TEST(world_to_cell_floors_negative_positions_down) {
    Level l;
    int x = 0, y = 0;
    CHECK(!l.map.world_to_cell({-0.5f, -0.5f}, x, y) && x == -1 && y == -1);
    CHECK(!l.map.world_to_cell({-16, -8}, x, y) && x == -1 && y == -1);
    CHECK(!l.map.world_to_cell({-16.5f, -8.5f}, x, y) && x == -2 && y == -2);
}

NNH_TEST(world_to_cell_clamps_huge_values_and_rejects_nan) {
    Level l;
    int x = 0, y = 0;
    CHECK(!l.map.world_to_cell({1e30f, -1e30f}, x, y) && x == (1 << 30) && y == -(1 << 30));
    const float inf = std::numeric_limits<float>::infinity();
    CHECK(!l.map.world_to_cell({inf, -inf}, x, y) && x == (1 << 30) && y == -(1 << 30));
    x = y = 5;
    CHECK(!l.map.world_to_cell({std::nanf(""), 0}, x, y) && x == 0 && y == 0);
}

NNH_TEST(tile_at_reads_the_tile_under_a_world_position) {
    Level l;
    l.map.set_tile(l.front, 2, 3, tile(6, false, true));
    CHECK(l.map.tile_at(l.front, {32, 24}) == tile(6, false, true));
    CHECK(l.map.tile_at(l.front, {47.5f, 31.5f}) == tile(6, false, true));
    CHECK(l.map.tile_at(l.front, {48, 24}).empty());
    l.map.set_tile(l.front, 0, 0, tile(1));
    CHECK(l.map.tile_at(l.front, {-0.1f, 0}).empty()); // cell -1: outside, not cell 0
}

NNH_TEST(set_tile_records_which_tilesets_a_layer_uses) {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4});
    map_test::tileset(map, {.texture = &engine.sheet, .tile_count = 2}); // ids 1, 2
    const int second = map_test::tileset(map, {.texture = &engine.sheet}); // ids 3..10
    const int l = map_test::layer(map);
    const auto used = [&] { return nnh::detail::map_view(map).tile_layers()[l].tilesets_used; };
    CHECK(used() == 0);
    map.set_tile(l, 0, 0, tile(2));
    CHECK(used() == 1);
    map.fill(l, 1, 1, 2, 2, tile(second));
    CHECK(used() == 3);
    map.set_tile(l, 0, 0, tile(0)); // never cleared
    CHECK(used() == 3);
}

NNH_TEST(an_empty_map_has_no_tiles_and_ignores_edits) {
    Tilemap map;
    map.set_tile(0, 0, 0, tile(1));
    map.fill(0, 0, 0, 4, 4, tile(1));
    CHECK(map.tile(0, 0, 0).empty() && map.tile_at(0, {1, 1}).empty());
    int x = 7, y = 7;
    CHECK(!map.world_to_cell({1, 1}, x, y) && x == 0 && y == 0);
    CHECK(same_rect(map.cell_rect(1, 1), {}));
}
