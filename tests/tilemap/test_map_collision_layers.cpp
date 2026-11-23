// Collision layer sets of a map (spec 019 FR-6, D34): the runtime area after the format bytes, defaults, setters,
// TileLayerDesc::collision_layers and the union of a cell's contributions.

#include <utility>

#include "load_fixture.hpp"
#include "map_fixture.hpp"
#include "nnh/build.hpp"
#include "tilemap/map_runtime.hpp"

using map_test::tile;
using nnh::Tilemap;
using nnh::collision::Cell;
using nnh::collision::Layers;
namespace collision = nnh::collision;
namespace fake = nnh::platform_fake;

namespace {

constexpr Layers kWater = collision::layer(3);
constexpr Layers kBoat = collision::layer(4);

std::size_t map_bytes() { return nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).bytes; }

/// 6 x 4 cells; ids 1..8; layers: 0 "back" (plain), 1 "water" (collision, on kWater); tile 2 solid, 3 one-way.
struct Level {
    map_test::Engine engine;
    Tilemap map = map_test::make({.width = 6, .height = 4});
    int first = map_test::tileset(map, {.texture = &engine.sheet});
    int back = map_test::layer(map, {.name = "back"});
    int water = map_test::layer(map, {.name = "water", .collision = true, .collision_layers = kWater});
    Level() {
        map.set_tile_collision(2, Cell::Solid);
        map.set_tile_collision(3, Cell::OneWay);
    }
};

Tilemap* g_map = nullptr;
int g_arg = 0;
Layers g_set = 0;

} // namespace

NNH_TEST(a_created_map_has_every_set_on_the_default_layer) {
    map_test::Engine engine;
    Tilemap map = map_test::make({.width = 4, .height = 4});
    map_test::tileset(map, {.texture = &engine.sheet});
    const int l = map_test::layer(map);
    CHECK(map.layer_collision_layers(l) == collision::kDefaultLayer);
    for (int id = 1; id <= 8; ++id) CHECK(map.tile_collision_layers(id) == collision::kDefaultLayer);
    const nnh::detail::MapRuntime& r = nnh::detail::map_runtime(nnh::TilemapAccess::block(map));
    for (const auto& slot : r.grid_masks) CHECK(slot.mask == collision::kAllLayers);
    CHECK(r.flags == 0);
}

NNH_TEST(the_block_grows_by_the_runtime_area) {
    map_test::Engine engine;
    const std::size_t before = map_bytes();
    {
        Tilemap map = map_test::make({.width = 40, .height = 15});
        CHECK(map_bytes() - before == 11940u + 132u + 4u * 257u); // the format, the runtime area, a set per tile id
        const nnh::TilemapDesc desc{.width = 40, .height = 15};
        CHECK(map_bytes() - before == nnh::detail::map_block_bytes(nnh::detail::capacities_for(desc)));
    }
    CHECK(map_bytes() == before);
}

NNH_TEST(a_loaded_map_has_every_set_on_the_default_layer) {
    load_test::start();
    {
        const std::size_t before = map_bytes();
        const Tilemap map = load_test::load("levels/one");
        CHECK(map_bytes() - before == load_test::files().one.size() + 132u + 4u * 9u);
        CHECK(map.layer_collision_layers(0) == collision::kDefaultLayer);
        CHECK(map.layer_collision_layers(1) == collision::kDefaultLayer);
        CHECK(map.tile_collision_layers(3) == collision::kDefaultLayer);
    }
    nnh::shutdown();
}

NNH_TEST(layer_and_tile_sets_are_set_in_code) {
    Level l;
    CHECK(l.map.layer_collision_layers(l.water) == kWater);
    CHECK(l.map.layer_collision_layers(l.back) == collision::kDefaultLayer);
    l.map.set_layer_collision_layers(l.back, kBoat | kWater);
    CHECK(l.map.layer_collision_layers(l.back) == (kBoat | kWater));
    l.map.set_tile_collision_layers(3, kBoat);
    CHECK(l.map.tile_collision_layers(3) == kBoat && l.map.tile_collision_layers(2) == collision::kDefaultLayer);
}

NNH_TEST(unknown_layers_and_tiles_have_no_layers) {
    Level l;
    CHECK(l.map.layer_collision_layers(-1) == collision::kNoLayers);
    CHECK(l.map.layer_collision_layers(2) == collision::kNoLayers);
    CHECK(l.map.tile_collision_layers(0) == collision::kNoLayers);
    CHECK(l.map.tile_collision_layers(9) == collision::kNoLayers);
    CHECK(Tilemap{}.layer_collision_layers(0) == collision::kNoLayers);
    CHECK(Tilemap{}.tile_collision_layers(1) == collision::kNoLayers);
}

NNH_TEST(bad_sets_assert_and_keep_the_previous_value) {
    Level l;
    g_map = &l.map;
    struct Case {
        void (*fn)();
        int arg;
        Layers set;
    } cases[] = {
        {[] { g_map->set_layer_collision_layers(g_arg, g_set); }, 1, collision::kNoLayers},
        {[] { g_map->set_layer_collision_layers(g_arg, g_set); }, 5, kBoat},
        {[] { g_map->set_tile_collision_layers(g_arg, g_set); }, 2, collision::kNoLayers},
        {[] { g_map->set_tile_collision_layers(g_arg, g_set); }, 9, kBoat},
        {[] { g_map->set_tile_collision_layers(g_arg, g_set); }, 0, kBoat},
    };
    for (const Case& c : cases) {
        g_arg = c.arg;
        g_set = c.set;
        CHECK(fake::run_expecting_crash(c.fn) == nnh::build::kAssertsEnabled);
    }
    CHECK(l.map.layer_collision_layers(l.water) == kWater);
    CHECK(l.map.tile_collision_layers(2) == collision::kDefaultLayer);
}

NNH_TEST(a_tile_layer_on_no_layers_is_refused) {
    Level l;
    auto made = l.map.add_tile_layer({.name = "nothing", .collision = true, .collision_layers = collision::kNoLayers});
    CHECK(!made.ok() && made.error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(l.map.tile_layer_count() == 2);
}

NNH_TEST(collision_layers_at_is_the_union_of_a_cells_contributions) {
    Level l;
    l.map.set_tile_collision_layers(2, kBoat);
    l.map.set_tile(l.back, 0, 0, tile(2));  // a solid tile on kBoat
    l.map.set_tile(l.water, 1, 0, tile(1)); // the water layer: kWater
    l.map.set_tile(l.back, 2, 0, tile(3));  // a one-way tile: default
    l.map.set_tile(l.water, 2, 0, tile(2)); // ... under a solid tile in the water: kWater | kBoat
    l.map.set_tile(l.back, 3, 0, tile(1));  // a plain tile: nothing
    CHECK(l.map.collision_layers_at(0, 0) == kBoat);
    CHECK(l.map.collision_layers_at(1, 0) == kWater);
    CHECK(l.map.collision_layers_at(2, 0) == (collision::kDefaultLayer | kWater | kBoat));
    CHECK(l.map.collision_layers_at(3, 0) == collision::kNoLayers);
    CHECK(l.map.collision_layers_at(5, 3) == collision::kNoLayers);
    CHECK(l.map.collision_layers_at(-1, 0) == collision::kNoLayers && l.map.collision_layers_at(6, 0) == 0u);
}

NNH_TEST(sets_survive_moving_the_map) {
    Level l;
    l.map.set_tile_collision_layers(3, kBoat);
    const Tilemap moved = std::move(l.map);
    CHECK(moved.tile_collision_layers(3) == kBoat && moved.layer_collision_layers(1) == kWater);
}
