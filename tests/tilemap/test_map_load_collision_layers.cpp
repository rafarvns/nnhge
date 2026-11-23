// Loading collision layer sets (spec 019 FR-6, FR-7, T4.4): `collision_layers` int properties of tile layers and
// tiles become their sets; a non-int or 0 value keeps the default with one warning per map; maps without it are
// unchanged.

#include "load_fixture.hpp"
#include "nnh/collision.hpp"

using nnh::Tilemap;
using nnh::collision::Cell;
namespace collision = nnh::collision;
namespace fake = nnh::platform_fake;

namespace {

/// The sample map with one more int property on tile 3 (solid): `collision_layers` = `value`.
map_file::Bytes sample_with_tile_set(int value) {
    load_test::start();
    map_file::Bytes bytes;
    {
        auto loaded = nnh::load_tilemap("levels/one", {.extra_properties = 1, .extra_string_bytes = 32});
        CHECK(loaded.ok());
        Tilemap& map = loaded.value();
        CHECK(map.set_int(nnh::PropertyOwner::tile(3), "collision_layers", value).ok());
        bytes = map_file::from_map(map);
    }
    nnh::shutdown();
    return bytes;
}

} // namespace

NNH_TEST(collision_layers_properties_become_the_sets_of_layers_and_tiles) {
    load_test::start(map_file::layered("tiles/world"));
    {
        const int before = fake::state().console_lines;
        const Tilemap map = load_test::load("levels/other");
        CHECK(map.layer_collision_layers(0) == 6u);                     // "back"
        CHECK(map.layer_collision_layers(1) == collision::kDefaultLayer); // "ground": a float, ignored
        CHECK(map.tile_collision_layers(5) == 2u);
        CHECK(map.tile_collision_layers(3) == collision::kDefaultLayer);
        CHECK(fake::state().console_lines == before + 1); // one warning for the map
        // Tile 5 (one-way, on layer 1 only) at (2, 1) of "back": one-way for mask 2, nothing for the default layer.
        CHECK(map.collision_at(2, 1) == Cell::OneWay);
        CHECK(map.collision_grid(2u).cell(map.collision_grid(2u).context, 2, 1) == Cell::OneWay);
        CHECK(map.collision_grid(1u).cell(map.collision_grid(1u).context, 2, 1) == Cell::Empty);
        CHECK(map.collision_layers_at(2, 1) == 2u);
        CHECK(map.collision_layers_at(0, 3) == collision::kDefaultLayer); // the ground's solid tiles
    }
    nnh::shutdown();
}

NNH_TEST(a_zero_collision_layers_keeps_the_default_with_a_warning) {
    const map_file::Bytes zero = sample_with_tile_set(0);
    load_test::start(zero);
    {
        const int before = fake::state().console_lines;
        const Tilemap map = load_test::load("levels/other");
        CHECK(map.tile_collision_layers(3) == collision::kDefaultLayer);
        CHECK(fake::state().console_lines == before + 1);
    }
    nnh::shutdown();
}

NNH_TEST(an_int_collision_layers_on_a_tile_is_read) {
    const map_file::Bytes bits = sample_with_tile_set(0x40000001);
    load_test::start(bits);
    {
        const int before = fake::state().console_lines;
        const Tilemap map = load_test::load("levels/other");
        CHECK(map.tile_collision_layers(3) == 0x40000001u);
        CHECK(map.properties(nnh::PropertyOwner::tile(3)).get_int("damage") == 2); // the other property stays
        CHECK(fake::state().console_lines == before);
    }
    nnh::shutdown();
}

NNH_TEST(maps_without_the_property_load_as_before_and_quietly) {
    load_test::start();
    {
        const int before = fake::state().console_lines;
        const Tilemap map = load_test::load("levels/one");
        CHECK(fake::state().console_lines == before);
        for (int l = 0; l < map.tile_layer_count(); ++l) CHECK(map.layer_collision_layers(l) == collision::kDefaultLayer);
        for (int id = 1; id <= map.tile_id_count(); ++id) CHECK(map.tile_collision_layers(id) == collision::kDefaultLayer);
        for (int y = 0; y < map.height(); ++y) {
            for (int x = 0; x < map.width(); ++x) {
                const collision::Grid g = map.collision_grid(collision::kDefaultLayer);
                CHECK(g.cell(g.context, x, y) == map.collision_at(x, y));
            }
        }
    }
    nnh::shutdown();
}
