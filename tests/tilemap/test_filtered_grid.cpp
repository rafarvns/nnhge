// collision_grid(mask) (spec 019 FR-6, AC-3, D34): masks that keep, drop or leave a one-way cell; equal to
// collision_grid() for all layers and on random maps with default sets; live edits; move through it; slots.

#include <cstdint>
#include <utility>

#include "collision/grid_internal.hpp"
#include "map_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "tilemap/map_runtime.hpp"
#include "tilemap/tilemap_internal.hpp"

using map_test::tile;
using nnh::Tilemap;
using nnh::collision::Cell;
using nnh::collision::Layers;
namespace collision = nnh::collision;

namespace {

constexpr Layers kWorld = collision::kDefaultLayer;
constexpr Layers kWater = collision::layer(1);
constexpr Layers kPlank = collision::layer(2);

/// 8 x 4 cells; ids 1..8. Layer 0 "ground" (collision, kWorld), layer 1 "water" (collision, kWater), layer 2
/// "deco" (plain). Tile 2: solid on kWorld; tile 3: one-way on kPlank; tile 4: solid on kWater.
struct Level {
    map_test::Engine engine;
    Tilemap map = map_test::make({.width = 8, .height = 4});
    int first = map_test::tileset(map, {.texture = &engine.sheet});
    int ground = map_test::layer(map, {.name = "ground", .collision = true});
    int water = map_test::layer(map, {.name = "water", .collision = true, .collision_layers = kWater});
    int deco = map_test::layer(map, {.name = "deco"});
    Level() {
        map.set_tile_collision(2, Cell::Solid);
        map.set_tile_collision(3, Cell::OneWay);
        map.set_tile_collision(4, Cell::Solid);
        map.set_tile_collision_layers(3, kPlank);
        map.set_tile_collision_layers(4, kWater);
        map.fill(ground, 0, 3, 8, 1, tile(1)); // floor: kWorld
        map.fill(water, 2, 2, 3, 1, tile(1));  // water: kWater
        map.set_tile(deco, 6, 1, tile(3));     // a plank: one-way on kPlank
        map.set_tile(deco, 7, 1, tile(2));     // a crate: solid on kWorld
        map.set_tile(deco, 0, 0, tile(4));     // a water block: solid on kWater
        map.set_tile(water, 6, 1, tile(1));    // water behind the plank
    }
};

Cell at(const collision::Grid& g, int x, int y) { return collision::detail::cell_at(g, x, y); }

} // namespace

NNH_TEST(a_mask_keeps_the_cells_on_its_layers) {
    Level l;
    const collision::Grid boat = l.map.collision_grid(kWater);
    CHECK(at(boat, 2, 2) == Cell::Solid && at(boat, 0, 0) == Cell::Solid && at(boat, 6, 1) == Cell::Solid);
    CHECK(at(boat, 0, 3) == Cell::Empty && at(boat, 7, 1) == Cell::Empty);
    const collision::Grid walker = l.map.collision_grid(kWorld | kPlank);
    CHECK(at(walker, 2, 2) == Cell::Empty && at(walker, 0, 0) == Cell::Empty);
    CHECK(at(walker, 0, 3) == Cell::Solid && at(walker, 7, 1) == Cell::Solid);
}

NNH_TEST(a_mask_can_leave_a_cell_one_way) {
    Level l;
    // (6, 1): solid water behind a plank. Without kWater the plank remains, without kPlank nothing.
    CHECK(at(l.map.collision_grid(kWorld | kPlank), 6, 1) == Cell::OneWay);
    CHECK(at(l.map.collision_grid(kWorld), 6, 1) == Cell::Empty);
    CHECK(at(l.map.collision_grid(kWater | kPlank), 6, 1) == Cell::Solid);
    CHECK(at(l.map.collision_grid(collision::kNoLayers), 0, 3) == Cell::Empty);
}

NNH_TEST(all_layers_is_the_unfiltered_grid) {
    Level l;
    const collision::Grid all = l.map.collision_grid(collision::kAllLayers);
    const collision::Grid plain = l.map.collision_grid();
    CHECK(all.cell == plain.cell && all.context == plain.context && all.width == plain.width);
    CHECK(all.cell_w == plain.cell_w && all.cell_h == plain.cell_h && all.outside == plain.outside);
}

NNH_TEST(the_same_mask_twice_gives_the_same_grid) {
    Level l;
    const collision::Grid a = l.map.collision_grid(kWater);
    const collision::Grid b = l.map.collision_grid(kWater);
    const collision::Grid c = l.map.collision_grid(kWorld);
    CHECK(a.context == b.context && a.cell == b.cell && a.context != c.context);
    CHECK(a.cell != l.map.collision_grid().cell);
}

NNH_TEST(a_filtered_grid_sees_edits_at_once) {
    Level l;
    const collision::Grid boat = l.map.collision_grid(kWater);
    CHECK(at(boat, 5, 2) == Cell::Empty);
    l.map.set_tile(l.water, 5, 2, tile(1));
    CHECK(at(boat, 5, 2) == Cell::Solid);
    l.map.set_layer_collision_layers(l.water, kPlank);
    CHECK(at(boat, 5, 2) == Cell::Empty && at(l.map.collision_grid(kPlank), 5, 2) == Cell::Solid);
    l.map.set_tile_collision(4, Cell::OneWay); // the water block becomes a one-way block (still on kWater)
    CHECK(at(boat, 0, 0) == Cell::OneWay);
}

NNH_TEST(move_and_cells_touching_go_through_a_filtered_grid) {
    Level l;
    collision::Grid boat = l.map.collision_grid(kWater);
    // A boat falling at x 40 (cell 2) lands on the water at y 32; a walker falls through it onto the floor.
    const auto r = collision::move(boat, {36, 0, 8, 8}, {0, 60});
    CHECK(r.touching.floor && r.position.y == 32 - 8);
    const auto w = collision::move(l.map.collision_grid(kWorld), {36, 0, 8, 8}, {0, 60});
    CHECK(w.touching.floor && w.position.y == 48 - 8);
    collision::CellHit hits[8];
    CHECK(collision::cells_touching(boat, {30, 30, 20, 20}, hits, 8) == 2);
    CHECK(collision::cells_touching(l.map.collision_grid(), {30, 30, 20, 20}, hits, 8) == 5); // + 3 floor cells
}

NNH_TEST(filtered_grids_survive_moving_the_tilemap) {
    Level l;
    const collision::Grid boat = l.map.collision_grid(kWater);
    const Tilemap moved = std::move(l.map);
    CHECK(at(boat, 2, 2) == Cell::Solid && at(boat, 0, 3) == Cell::Empty);
    CHECK(moved.collision_grid(kWater).context == boat.context);
}

namespace {

Tilemap* g_map = nullptr;
collision::Grid g_grid;
void ninth_mask() { g_grid = g_map->collision_grid(collision::layer(9)); }

} // namespace

NNH_TEST(a_map_holds_eight_masks_and_a_ninth_asserts) {
    Level l;
    for (int n = 0; n < 8; ++n) CHECK(l.map.collision_grid(collision::layer(n)).cell != l.map.collision_grid().cell);
    for (int n = 0; n < 8; ++n) CHECK(l.map.collision_grid(collision::layer(n)).context != nullptr); // reused
    g_map = &l.map;
    CHECK(nnh::platform_fake::run_expecting_crash(ninth_mask) == nnh::build::kAssertsEnabled);
    if (!nnh::build::kAssertsEnabled) CHECK(g_grid.cell == l.map.collision_grid().cell); // walls stay walls
}

NNH_TEST(release_logs_running_out_of_masks_once_per_map) {
    if (nnh::build::kAssertsEnabled) return;
    Level l;
    for (int n = 0; n < 8; ++n) (void)l.map.collision_grid(collision::layer(n));
    const int before = nnh::platform_fake::state().console_lines;
    (void)l.map.collision_grid(collision::layer(20));
    (void)l.map.collision_grid(collision::layer(21));
    CHECK(nnh::platform_fake::state().console_lines == before + 1);
}

NNH_TEST(an_empty_map_gives_an_empty_filtered_grid) {
    const collision::Grid g = Tilemap{}.collision_grid(kWater);
    CHECK(g.width == 0 && g.height == 0 && g.cell == nullptr);
}

namespace {

std::uint32_t g_seed = 0x2545F491u;
int rnd(int n) {
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return static_cast<int>(g_seed % static_cast<std::uint32_t>(n));
}

} // namespace

NNH_TEST(with_default_sets_a_mask_with_the_default_layer_changes_nothing) {
    map_test::Engine engine;
    int compared = 0;
    for (int m = 0; m < 200; ++m) {
        const int w = 1 + rnd(12), h = 1 + rnd(9);
        Tilemap map = map_test::make({.width = w, .height = h, .max_tile_layers = 4, .max_tile_ids = 16});
        map_test::tileset(map, {.texture = &engine.sheet});
        map_test::tileset(map, {.texture = &engine.sheet});
        const int layers = 1 + rnd(4);
        for (int i = 0; i < layers; ++i) map_test::layer(map, {.collision = rnd(3) == 0});
        for (int id = 1; id <= 16; ++id) map.set_tile_collision(id, static_cast<Cell>(rnd(3)));
        for (int n = rnd(w * h * layers); n > 0; --n) map.set_tile(rnd(layers), rnd(w), rnd(h), tile(rnd(17)));
        const Layers mask = collision::kDefaultLayer | static_cast<Layers>(rnd(1 << 16)) << 1;
        const collision::Grid filtered = map.collision_grid(mask);
        const collision::Grid plain = map.collision_grid();
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                CHECK(at(filtered, x, y) == at(plain, x, y));
                ++compared;
            }
        }
        const Layers others = static_cast<Layers>(rnd(1 << 16)) << 1; // without the default layer: nothing
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) CHECK(at(map.collision_grid(others), x, y) == Cell::Empty);
        }
    }
    CHECK(compared > 3000);
}
