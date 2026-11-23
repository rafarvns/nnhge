// Loading collision layer sets (spec 019 FR-6, FR-7, T4.4): `collision_layers` int properties of tile layers and
// tiles become their sets; a non-int or 0 value keeps the default with one warning per map; maps without it are
// unchanged.

#include <cstdio>
#include <string>

#include "load_fixture.hpp"
#include "map_fixture.hpp"
#include "nnh/collision.hpp"

#ifndef NNH_TEST_MAPS_DIR
#error "NNH_TEST_MAPS_DIR is set by tests/CMakeLists.txt"
#endif

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

namespace {

std::string read_converted(const char* name) {
    std::string bytes;
    const std::string path = std::string(NNH_TEST_MAPS_DIR) + "/" + name;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    CHECK(f != nullptr);
    if (f == nullptr) return bytes;
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) bytes.append(buffer, got);
    std::fclose(f);
    return bytes;
}

Cell cell(const collision::Grid& g, int x, int y) { return g.cell(g.context, x, y); }

} // namespace

NNH_TEST(the_converted_tiled_fixture_loads_with_its_sets_and_filtered_grids) {
    // tests/data/maps/good_collision_layers.tmj: bits world 1, water 2, plank 4, boat 8. Layer "ground" (collision,
    // default), layer "water" (collision, boat); tile 2 solid on water, tile 3 one-way on world | plank.
    map_test::Engine engine;
    const std::string bytes = read_converted("good_collision_layers.nnhmap");
    auto made = nnh::detail::tilemap_from_file(bytes.data(), bytes.size(), {}, "good_collision_layers");
    CHECK(made.ok());
    if (!made) return;
    const Tilemap& map = made.value();
    CHECK(map.layer_collision_layers(0) == 1u && map.layer_collision_layers(1) == 8u);
    CHECK(map.tile_collision_layers(1) == 1u && map.tile_collision_layers(2) == 2u && map.tile_collision_layers(3) == 5u);
    CHECK(map.collision_layers_at(2, 0) == 10u && map.collision_layers_at(1, 1) == 13u);
    CHECK(map.collision_layers_at(0, 2) == 1u && map.collision_layers_at(2, 2) == 8u);
    const collision::Grid world = map.collision_grid(1u);
    CHECK(cell(world, 0, 2) == Cell::Solid && cell(world, 2, 0) == Cell::Empty && cell(world, 1, 1) == Cell::OneWay);
    CHECK(cell(world, 2, 2) == Cell::Empty);
    const collision::Grid boat = map.collision_grid(8u);
    CHECK(cell(boat, 2, 0) == Cell::Solid && cell(boat, 1, 1) == Cell::Solid && cell(boat, 2, 2) == Cell::Solid);
    CHECK(cell(boat, 0, 2) == Cell::Empty);
    const collision::Grid water = map.collision_grid(2u);
    CHECK(cell(water, 2, 0) == Cell::Solid && cell(water, 1, 1) == Cell::Empty);
    CHECK(map.properties(nnh::PropertyOwner::tile_layer(1)).get_int("collision_layers") == 8); // still a property
}
