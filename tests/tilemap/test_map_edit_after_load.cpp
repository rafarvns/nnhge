// Changing a loaded map in code (spec 012 FR-0, FR-11): tiles and collision as for a map built in code, and the
// extra room LoadTilemapOptions asks for (a file has no spare capacity of its own).

#include "load_fixture.hpp"
#include "nnh/texture.hpp"

using load_test::load;
using load_test::load_error;
using load_test::map_bytes;
using load_test::start;
using nnh::ErrorCode;
using nnh::LoadTilemapOptions;
using nnh::PropertyOwner;
using nnh::Tile;
using nnh::Tilemap;
using nnh::collision::Cell;

NNH_TEST(a_loaded_map_takes_tile_changes_like_a_built_one) {
    start();
    {
        Tilemap map = load("levels/one");
        map.set_tile(0, 6, 0, Tile{5});
        CHECK(map.tile(0, 6, 0) == Tile{5});
        CHECK(map.collision_at(6, 0) == Cell::OneWay);
        map.set_tile_collision(1, Cell::Solid);
        CHECK(map.collision_at(0, 0) == Cell::Solid);
        map.fill(1, 0, 3, 8, 1, Tile{});
        CHECK(map.collision_at(0, 3) == Cell::Empty);
    }
    nnh::shutdown();
}

NNH_TEST(a_loaded_map_has_no_spare_room_without_options) {
    start();
    {
        Tilemap map = load("levels/one");
        CHECK(map.add_tile_layer({.name = "more"}).error().code == ErrorCode::LimitReached);
        CHECK(map.add_object(0, {.name = "door"}).error().code == ErrorCode::LimitReached);
        CHECK(map.set_int(PropertyOwner::map(), "level", 1).error().code == ErrorCode::LimitReached);
        CHECK(map.set_int(PropertyOwner::map(), "music", 3).ok()); // replacing needs no room
    }
    nnh::shutdown();
}

NNH_TEST(load_options_add_room_for_layers_objects_properties_and_strings) {
    start();
    {
        Tilemap map = load("levels/one", {.extra_tile_layers = 1, .extra_objects = 2, .extra_properties = 3,
                                          .extra_string_bytes = 64});
        auto layer = map.add_tile_layer({.name = "front"});
        CHECK(layer.ok() && layer.value() == 2 && map.tile(2, 0, 0).empty() && map.tile(2, 7, 3).empty());
        map.set_tile(2, 7, 3, Tile{8});
        CHECK(map.tile(2, 7, 3) == Tile{8} && map.tile(1, 7, 3) == Tile{3}); // the loaded cells did not move
        auto door = map.add_object(0, {.name = "door", .type = "door"});
        CHECK(door.ok() && map.object(door.value()).id == 10); // after the file's highest id (9)
        CHECK(map.set_string(PropertyOwner::map(), "next", "levels/two").ok());
        CHECK(map.set_int(PropertyOwner::tile(5), "bounce", 3).ok()); // a tile without properties yet
        CHECK(map.properties(PropertyOwner::tile(5)).get_int("bounce") == 3);
        // New strings went after the loaded ones: every loaded name is intact.
        CHECK(std::strcmp(map.properties().get_string("music"), "theme") == 0);
        CHECK(std::strcmp(map.tile_layer_name(0), "back") == 0 && std::strcmp(map.tile_layer_name(1), "ground") == 0);
        CHECK(std::strcmp(map.object(0).name, "spawn") == 0 && std::strcmp(map.object_layer_name(0), "things") == 0);
        CHECK(map.properties(PropertyOwner::tile(3)).get_int("damage") == 2);
        CHECK(map.object(1).properties.get_int("value") == 5);
    }
    nnh::shutdown();
}

NNH_TEST(load_options_cost_what_the_memory_formula_says) {
    start();
    {
        const std::size_t before = map_bytes();
        Tilemap plain = load("levels/one");
        const std::size_t file_bytes = load_test::files().one.size();
        const std::size_t runtime = 132u + 4u * 9; // spec 019: the runtime area, a collision set per tile id
        CHECK(map_bytes() - before == file_bytes + runtime); // the block is the file, then the runtime area
        const std::size_t with_plain = map_bytes();
        Tilemap roomy = load("levels/one", {.extra_tile_layers = 1, .extra_objects = 2});
        CHECK(map_bytes() - with_plain == file_bytes + runtime + 2u * 4 * 8 + 40 + 2 * 36); // cells, layer, objects
    }
    nnh::shutdown();
}

NNH_TEST(load_options_must_not_be_negative_or_over_the_limits) {
    start();
    CHECK(load_error("levels/one", {.extra_objects = -1}) == ErrorCode::InvalidArgument);
    CHECK(load_error("levels/one", {.extra_string_bytes = -5}) == ErrorCode::InvalidArgument);
    CHECK(load_error("levels/one", {.extra_tile_layers = 15}) == ErrorCode::LimitReached); // 2 + 15 > 16
    CHECK(load_error("levels/one", {.extra_properties = 2147483647}) == ErrorCode::LimitReached);
    CHECK(load_error("levels/one", {.extra_string_bytes = 65536}) == ErrorCode::LimitReached);
    CHECK(load_test::map_blocks() == 0);
    nnh::shutdown();
}

NNH_TEST(a_map_built_in_code_loads_back_the_same) {
    start();
    map_file::Bytes file;
    {
        auto texture = nnh::load_texture("tiles/world");
        auto made = Tilemap::create({.width = 5, .height = 3, .max_tile_layers = 2});
        Tilemap built = std::move(made.value());
        const int first = built.add_tileset({.texture = &texture.value(), .name = "tiles/world"}).value();
        const int ground = built.add_tile_layer({.name = "ground", .parallax = {0.25f, 1}}).value();
        built.fill(ground, 0, 2, 5, 1, Tile{static_cast<std::uint16_t>(first + 2), false, true, false});
        built.set_tile_collision(first + 2, Cell::OneWay);
        CHECK(built.add_object_layer("o").ok() && built.add_object(0, {.name = "a", .bounds = {1, 2, 3, 4}}).ok());
        CHECK(built.set_float(PropertyOwner::object(0), "speed", 2.5f).ok());
        file = map_file::from_map(built);
    }
    start(file);
    {
        Tilemap loaded = load("levels/other");
        CHECK(loaded.width() == 5 && loaded.height() == 3 && loaded.tile_layer_count() == 1);
        CHECK(loaded.tile(0, 4, 2) == (Tile{3, false, true, false}) && loaded.collision_at(4, 2) == Cell::OneWay);
        CHECK(loaded.layer_parallax(0) == (nnh::Vec2{0.25f, 1.0f}));
        CHECK(loaded.object(0).bounds.h == 4 && loaded.object(0).properties.get_float("speed") == 2.5f);
        CHECK(loaded.add_tile_layer({}).ok()); // the code map's spare capacity came along
    }
    nnh::shutdown();
}

NNH_TEST(a_loaded_map_keeps_its_animations_with_room_for_no_more) {
    start(map_file::animated("tiles/world"));
    {
        for (const int extra_layers : {0, 2}) { // with extras, every later section moves
            Tilemap map = load("levels/other", {.extra_tile_layers = extra_layers, .extra_properties = 3});
            nnh::Tilemap::AnimationFrame frames[4] = {};
            CHECK(map.tile_animation(2, frames, 4) == 2 && frames[1].tile_id == 4 && frames[1].ms == 50);
            CHECK(map.animated_tile_id(2, nnh::Duration::from_ms(120)) == 4);
            CHECK(map.animated_tile_id(6, nnh::Duration::from_ms(5)) == 7);
            CHECK(map.tile_animation(3, frames, 4) == 0);
            const nnh::Tilemap::AnimationFrame one[] = {{.tile_id = 3, .ms = 10}};
            CHECK(!map.set_tile_animation(3, one, 1).ok()); // no spare animation
            CHECK(map.set_tile_animation(2, one, 1).ok());  // a shorter one fits
            CHECK(map.animated_tile_id(2, {}) == 3 && map.animated_tile_id(6, {}) == 7);
        }
    }
    nnh::shutdown();
}
