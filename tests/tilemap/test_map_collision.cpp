// Collision of a map (spec 012 FR-4, FR-7, FR-8): tile flags, collision layers, collision_at, and
// collision::move on the map's grid. Also: a whole level built in code and played against.

#include <utility>

#include "map_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/memory.hpp"
#include "tilemap/tilemap_internal.hpp"

using map_test::make;
using map_test::tile;
using nnh::Tilemap;
using nnh::collision::Cell;
namespace collision = nnh::collision;
namespace fake = nnh::platform_fake;

namespace {

/// 8 x 6 cells of 16 x 16; tileset ids 1..8; tile 2 solid, tile 3 one-way (set before placing tiles).
struct Level {
    map_test::Engine engine;
    Tilemap map = make({.width = 8, .height = 6});
    int first = map_test::tileset(map, {.texture = &engine.sheet});
    int back = map_test::layer(map, {.name = "back"});
    int ground = map_test::layer(map, {.name = "ground", .collision = true});
    Level() {
        map.set_tile_collision(2, Cell::Solid);
        map.set_tile_collision(3, Cell::OneWay);
    }
};

std::uint16_t mask(const Tilemap& map) { return nnh::detail::map_view(map).header().collision_layers; }

Tilemap* g_map = nullptr;
void mark_unknown_tile() { g_map->set_tile_collision(9, Cell::Solid); }

} // namespace

NNH_TEST(tiles_start_empty_for_collision_and_keep_their_marks) {
    Level l;
    CHECK(l.map.tile_collision(1) == Cell::Empty);
    CHECK(l.map.tile_collision(2) == Cell::Solid && l.map.tile_collision(3) == Cell::OneWay);
    l.map.set_tile_collision(2, Cell::Empty);
    CHECK(l.map.tile_collision(2) == Cell::Empty);
    CHECK(l.map.tile_collision(0) == Cell::Empty && l.map.tile_collision(9) == Cell::Empty);
    CHECK(l.map.tile_collision(-1) == Cell::Empty);
}

NNH_TEST(marking_an_unknown_tile_asserts_and_is_ignored) {
    Level l;
    g_map = &l.map;
    CHECK(fake::run_expecting_crash(mark_unknown_tile) == nnh::build::kAssertsEnabled);
    CHECK(l.map.tile_collision(9) == Cell::Empty);
}

NNH_TEST(marking_a_tile_keeps_its_property_flag) {
    Level l;
    CHECK(l.map.set_int(nnh::PropertyOwner::tile(2), "damage", 1).ok());
    l.map.set_tile_collision(2, Cell::OneWay);
    CHECK(l.map.properties(nnh::PropertyOwner::tile(2)).get_int("damage") == 1);
    CHECK(l.map.tile_collision(2) == Cell::OneWay);
}

NNH_TEST(every_tile_of_a_collision_layer_is_solid) {
    Level l;
    l.map.set_tile(l.ground, 1, 4, tile(1)); // not marked, but the layer is `collision`
    l.map.set_tile(l.ground, 2, 4, tile(3)); // one-way tile on a collision layer: still solid
    CHECK(l.map.collision_at(1, 4) == Cell::Solid && l.map.collision_at(2, 4) == Cell::Solid);
    CHECK(l.map.collision_at(3, 4) == Cell::Empty);
}

NNH_TEST(marked_tiles_collide_on_any_layer) {
    Level l;
    l.map.set_tile(l.back, 1, 1, tile(2));
    l.map.set_tile(l.back, 2, 1, tile(3));
    l.map.set_tile(l.back, 3, 1, tile(1));
    CHECK(l.map.collision_at(1, 1) == Cell::Solid);
    CHECK(l.map.collision_at(2, 1) == Cell::OneWay);
    CHECK(l.map.collision_at(3, 1) == Cell::Empty);
}

NNH_TEST(solid_wins_over_one_way_across_layers) {
    Level l;
    l.map.set_tile(l.back, 5, 2, tile(3));
    l.map.set_tile(l.ground, 5, 2, tile(1));
    CHECK(l.map.collision_at(5, 2) == Cell::Solid);
    l.map.set_layer_collision(l.ground, false);
    CHECK(l.map.collision_at(5, 2) == Cell::OneWay);
    l.map.set_tile(l.ground, 5, 2, tile(2));
    CHECK(l.map.collision_at(5, 2) == Cell::Solid);
}

NNH_TEST(cells_outside_the_map_are_empty) {
    Level l;
    l.map.fill(l.ground, 0, 0, 8, 6, tile(1));
    CHECK(l.map.collision_at(-1, 0) == Cell::Empty && l.map.collision_at(8, 0) == Cell::Empty);
    CHECK(l.map.collision_at(0, -1) == Cell::Empty && l.map.collision_at(0, 6) == Cell::Empty);
}

NNH_TEST(only_layers_that_can_collide_are_in_the_mask) {
    Level l;
    CHECK(mask(l.map) == 0b10); // ground is a collision layer
    l.map.set_tile(l.back, 0, 0, tile(1));
    CHECK(mask(l.map) == 0b10); // a plain tile: back can't collide
    l.map.set_tile(l.back, 1, 0, tile(3));
    CHECK(mask(l.map) == 0b11);
    l.map.set_layer_collision(l.ground, false);
    CHECK(mask(l.map) == 0b01); // ground holds nothing marked
}

NNH_TEST(marking_a_tile_already_placed_makes_its_layers_collide) {
    Level l;
    l.map.set_tile(l.back, 4, 4, tile(5));
    CHECK(l.map.collision_at(4, 4) == Cell::Empty && mask(l.map) == 0b10);
    l.map.set_tile_collision(5, Cell::Solid);
    CHECK(mask(l.map) == 0b11);
    CHECK(l.map.collision_at(4, 4) == Cell::Solid);
    l.map.set_tile_collision(5, Cell::Empty); // the mask may stay wider: only a cost, never wrong
    CHECK(l.map.collision_at(4, 4) == Cell::Empty);
}

NNH_TEST(the_collision_grid_describes_the_map) {
    map_test::Engine engine;
    Tilemap map = make({.width = 5, .height = 3, .tile_width = 32, .tile_height = 8});
    map_test::tileset(map, {.texture = &engine.sheet});
    const int l = map_test::layer(map, {.collision = true});
    map.set_tile(l, 4, 2, tile(1));
    const collision::Grid grid = map.collision_grid();
    CHECK(grid.width == 5 && grid.height == 3 && grid.cell_w == 32 && grid.cell_h == 8);
    CHECK(grid.origin == nnh::Vec2(0, 0) && grid.outside == Cell::Empty && grid.cells == nullptr);
    CHECK(grid.cell != nullptr && grid.cell(grid.context, 4, 2) == Cell::Solid);
    CHECK(grid.cell(grid.context, 3, 2) == Cell::Empty);
}

NNH_TEST(set_tile_is_seen_by_the_grid_at_once) {
    Level l;
    const collision::Grid grid = l.map.collision_grid();
    const nnh::Rect box{0, 0, 16, 16}; // falls from cell (0, 0)
    CHECK(collision::move(grid, box, {0, 64}).position.y == 64);
    l.map.set_tile(l.ground, 0, 3, tile(1));
    const auto r = collision::move(grid, box, {0, 64});
    CHECK(r.position.y == 32 && r.touching.floor);
    l.map.set_tile(l.ground, 0, 3, tile(0)); // the block breaks
    CHECK(collision::move(grid, box, {0, 64}).position.y == 64);
}

NNH_TEST(the_grid_stays_valid_when_the_map_object_moves) {
    Level l;
    l.map.set_tile(l.ground, 2, 0, tile(1));
    const collision::Grid grid = l.map.collision_grid();
    Tilemap moved = std::move(l.map);
    CHECK(collision::move(grid, {0, 0, 16, 16}, {64, 0}).position.x == 16);
    CHECK(moved.collision_at(2, 0) == Cell::Solid);
}

NNH_TEST(a_whole_level_built_in_code_can_be_played) {
    Level l;
    l.map.fill(l.ground, 0, 5, 8, 1, tile(1)); // floor (row 5, y = 80)
    l.map.fill(l.ground, 7, 0, 1, 5, tile(1)); // right wall (x = 112)
    l.map.fill(l.back, 2, 3, 2, 1, tile(3));   // one-way plank (row 3, y = 48)
    collision::Grid grid = l.map.collision_grid();
    grid.outside = Cell::Solid;
    nnh::Rect player{40, 0, 12, 14};
    auto r = collision::move(grid, player, {0, 100}); // lands on the plank
    CHECK(r.touching.floor && r.position.y == 48 - 14);
    player.x = r.position.x;
    player.y = r.position.y;
    r = collision::move(grid, player, {0, 100}, {.drop_through = true}); // drops to the floor
    CHECK(r.touching.floor && r.position.y == 80 - 14);
    player.y = r.position.y;
    r = collision::move(grid, player, {200, 0}); // runs into the wall
    CHECK(r.touching.right && r.position.x == 112 - 12);
    player.x = r.position.x;
    r = collision::move(grid, player, {0, -40}); // jumps up through the plank's row (not under it here)
    CHECK(!r.touching.ceiling && r.position.y == 80 - 14 - 40);
    r = collision::move(grid, {0, 0, 16, 16}, {-50, 0}); // the solid outside walls the level in
    CHECK(r.touching.left && r.position.x == 0);
}

NNH_TEST(queries_edits_and_moves_never_allocate) {
    Level l;
    l.map.fill(l.ground, 0, 5, 8, 1, tile(1));
    const nnh::MemStats before = nnh::mem::region_stats(nnh::MemRegion::Heap);
    for (int frame = 0; frame < 100; ++frame) {
        l.map.set_tile(l.back, frame % 8, 2, tile(frame % 3 + 1));
        (void)l.map.collision_at(frame % 8, 2);
        (void)collision::move(l.map.collision_grid(), {0, 0, 16, 16}, {3, 5});
        (void)l.map.tile_at(l.back, {static_cast<float>(frame), 40});
        (void)l.map.properties().get_int("x");
        (void)l.map.find_objects("coin", nullptr, 0);
    }
    const nnh::MemStats after = nnh::mem::region_stats(nnh::MemRegion::Heap);
    CHECK(after.bytes == before.bytes && after.count == before.count && after.peak == before.peak);
}

NNH_TEST(a_destroyed_tileset_texture_goes_stale_but_the_map_still_works) {
    Level l;
    l.map.set_tile(l.ground, 1, 1, tile(l.first));
    CHECK(nnh::detail::tileset_texture_alive(l.map, 0));
    l.engine.sheet.reset();
    CHECK(!nnh::detail::tileset_texture_alive(l.map, 0)); // drawing (stage 3) skips it
    nnh::Texture other = gfx_test::load("tile");         // may reuse the slot: still stale
    CHECK(!nnh::detail::tileset_texture_alive(l.map, 0));
    CHECK(l.map.tile(l.ground, 1, 1) == tile(l.first));
    CHECK(l.map.collision_at(1, 1) == Cell::Solid);
}

NNH_TEST(the_map_memory_is_released_with_no_leak) {
    const std::size_t before = nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).count;
    {
        Level l;
        l.map.fill(l.ground, 0, 0, 8, 6, tile(2));
        (void)l.map.set_string({}, "music", "theme");
        CHECK(nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).count == before + 1);
    }
    CHECK(nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).count == before);
    CHECK(nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).bytes == 0);
}
