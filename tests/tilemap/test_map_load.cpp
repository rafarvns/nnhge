// Loading converted maps (spec 012 FR-0, FR-1, FR-11): nnh::load_tilemap reads a `.nnhmap` asset, validates it,
// copies it into the map's block and loads its tileset images; every failure is an error, never a crash.

#include "load_fixture.hpp"
#include "nnh/texture.hpp"

namespace fake = nnh::platform_fake;
using load_test::load;
using load_test::load_error;
using load_test::map_blocks;
using load_test::start;
using nnh::ErrorCode;
using nnh::PropertyOwner;
using nnh::Tile;
using nnh::Tilemap;
using nnh::collision::Cell;
namespace fmt = nnh::detail::mapfmt;

NNH_TEST(a_converted_map_loads_with_its_size_tilesets_and_layers) {
    start();
    {
        Tilemap map = load("levels/one");
        CHECK(map.width() == 8 && map.height() == 4 && map.tile_width() == 16 && map.tile_height() == 16);
        CHECK(map.tileset_count() == 1 && map.tile_id_count() == 8);
        CHECK(map.tile_layer_count() == 2 && map.find_tile_layer("ground") == 1);
        CHECK(std::strcmp(map.tile_layer_name(0), "back") == 0);
        CHECK(map.layer_parallax(0) == (nnh::Vec2{0.5f, 1.0f}) && map.layer_opacity(0) == 0.75f);
        CHECK(map.layer_visible(1) && map.layer_collision(1) && !map.layer_collision(0));
        CHECK(map.parallax_origin() == (nnh::Vec2{8.0f, 4.0f}));
    }
    nnh::shutdown();
}

NNH_TEST(a_converted_map_keeps_its_tiles_and_flips) {
    start();
    {
        Tilemap map = load("levels/one");
        CHECK(map.tile(0, 0, 0) == Tile{1});
        CHECK(map.tile(0, 1, 0) == (Tile{2, true, false, false}));
        CHECK(map.tile(0, 2, 1) == (Tile{5, false, false, true}));
        CHECK(map.tile(0, 3, 1) == (Tile{4, false, true, false}));
        CHECK(map.tile(0, 4, 0).empty());
        CHECK(map.tile(1, 7, 3) == Tile{3});
    }
    nnh::shutdown();
}

NNH_TEST(a_converted_map_keeps_its_objects_and_properties) {
    start();
    {
        Tilemap map = load("levels/one");
        CHECK(map.object_layer_count() == 1 && std::strcmp(map.object_layer_name(0), "things") == 0);
        CHECK(map.object_count() == 2 && map.find_object("coin") == 1);
        const nnh::MapObject spawn = map.object(0);
        CHECK(spawn.id == 7 && spawn.shape == nnh::ObjectShape::Point && spawn.bounds.x == 16 && spawn.bounds.y == 32);
        const nnh::MapObject coin = map.object(1);
        CHECK(coin.id == 9 && std::strcmp(coin.type, "coin") == 0 && coin.bounds.w == 16);
        CHECK(coin.properties.get_int("value") == 5);
        CHECK(std::strcmp(map.properties().get_string("music"), "theme") == 0);
        CHECK(map.properties(PropertyOwner::tile(3)).get_int("damage") == 2);
        CHECK(map.properties(PropertyOwner::tile_layer(0)).get_float("depth") == 1.5f);
        CHECK(map.properties(PropertyOwner::object_layer(0)).get_bool("active"));
    }
    nnh::shutdown();
}

NNH_TEST(a_converted_map_collides_as_its_tiles_and_layers_say) {
    // The file carries no collision mask (a runtime field): the loader works it out.
    start();
    {
        Tilemap map = load("levels/one");
        CHECK(map.tile_collision(3) == Cell::Solid && map.tile_collision(5) == Cell::OneWay);
        CHECK(map.collision_at(0, 3) == Cell::Solid);  // the collision layer
        CHECK(map.collision_at(2, 1) == Cell::OneWay); // tile 5 on the back layer
        CHECK(map.collision_at(0, 0) == Cell::Empty);
    }
    nnh::shutdown();
}

NNH_TEST(a_collision_layer_of_a_converted_map_blocks_with_any_tile) {
    // Tile 1 is not marked solid; on the `collision` layer (which holds nothing else) it still blocks.
    map_file::Bytes file = map_file::sample("tiles/world");
    for (int x = 0; x < 8; ++x) map_file::view(file).cells(1)[3 * 8 + x] = fmt::encode_cell(1, false, false, false);
    map_file::seal(file);
    start(file);
    {
        Tilemap map = load("levels/other");
        CHECK(map.tile_collision(1) == Cell::Empty && map.collision_at(0, 3) == Cell::Solid);
    }
    nnh::shutdown();
}

NNH_TEST(freeing_a_map_built_in_code_leaves_its_textures_alone) {
    start();
    nnh::Texture world = std::move(nnh::load_texture("tiles/world").value());
    {
        auto made = Tilemap::create({.width = 2, .height = 2});
        Tilemap built = std::move(made.value());
        CHECK(built.add_tileset({.texture = &world, .name = "tiles/world"}).ok());
        CHECK(built.add_tileset({.texture = &world, .tile_count = 1, .name = "tiles/world"}).ok());
    }
    CHECK(world.valid()); // borrowed, not owned
    world.reset();
    nnh::shutdown();
}

NNH_TEST(a_converted_map_loads_its_tileset_image_and_frees_it_with_the_map) {
    start();
    const int before = fake::state().live_textures;
    Tilemap map = load("levels/one");
    CHECK(fake::state().live_textures == before + 1);
    CHECK(nnh::detail::tileset_texture_alive(map, 0));
    CHECK(map_blocks() == 1);
    map.reset();
    CHECK(fake::state().live_textures == before && map_blocks() == 0);
    nnh::shutdown();
}

NNH_TEST(a_moved_map_still_frees_its_tileset_image_once) {
    start();
    const int before = fake::state().live_textures;
    {
        Tilemap a = load("levels/one");
        Tilemap b = std::move(a);
        a.reset();
        CHECK(fake::state().live_textures == before + 1 && nnh::detail::tileset_texture_alive(b, 0));
    }
    CHECK(fake::state().live_textures == before);
    nnh::shutdown();
}

NNH_TEST(tilesets_with_the_same_image_share_one_texture) {
    load_test::start();
    nnh::Texture world = std::move(nnh::load_texture("tiles/world").value());
    auto made = Tilemap::create({.width = 2, .height = 2});
    Tilemap built = std::move(made.value());
    CHECK(built.add_tileset({.texture = &world, .name = "tiles/world"}).ok());
    CHECK(built.add_tileset({.texture = &world, .tile_count = 2, .name = "tiles/world"}).ok());
    const map_file::Bytes file = map_file::from_map(built);
    built.reset();
    world.reset();
    start(file);
    const int before = fake::state().live_textures;
    {
        Tilemap map = load("levels/other");
        CHECK(map.tileset_count() == 2 && fake::state().live_textures == before + 1);
        CHECK(nnh::detail::tileset_texture_alive(map, 0) && nnh::detail::tileset_texture_alive(map, 1));
    }
    CHECK(fake::state().live_textures == before);
    nnh::shutdown();
}

// --- errors ---

NNH_TEST(load_tilemap_refuses_bad_paths_and_missing_maps) {
    start();
    CHECK(load_error(nullptr) == ErrorCode::InvalidArgument);
    CHECK(load_error("levels/missing") == ErrorCode::NotFound);
    CHECK(load_error("levels/one.nnhmap") == ErrorCode::NotFound); // no extension in the name
    CHECK(load_error("../levels/one") == ErrorCode::InvalidArgument);
    CHECK(map_blocks() == 0);
    nnh::shutdown();
    CHECK(load_error("levels/one") == ErrorCode::Unsupported); // before init
}

NNH_TEST(load_tilemap_refuses_damaged_and_truncated_files) {
    map_file::Bytes damaged = map_file::sample("tiles/world");
    damaged[200] ^= 0x40; // no seal: the CRC catches it
    start(damaged);
    CHECK(load_error("levels/other") == ErrorCode::Corrupted);
    map_file::Bytes truncated = map_file::sample("tiles/world");
    truncated.resize(truncated.size() / 2);
    start(truncated);
    CHECK(load_error("levels/other") == ErrorCode::Corrupted);
    start(map_file::Bytes{});
    const int base = fake::state().live_textures;
    CHECK(load_error("levels/other") == ErrorCode::Corrupted);
    CHECK(map_blocks() == 0 && fake::state().live_textures == base);
    nnh::shutdown();
}

NNH_TEST(load_tilemap_refuses_newer_formats_as_unsupported) {
    map_file::Bytes newer = map_file::sample("tiles/world");
    map_file::view(newer).header().version = 2;
    map_file::seal(newer);
    start(newer);
    CHECK(load_error("levels/other") == ErrorCode::Unsupported);
    nnh::shutdown();
}

NNH_TEST(load_tilemap_refuses_maps_over_the_limits) {
    map_file::Bytes big = map_file::sample("tiles/world");
    map_file::view(big).header().tile_layers_cap = 17;
    map_file::seal(big);
    start(big);
    CHECK(load_error("levels/other") == ErrorCode::LimitReached);
    nnh::shutdown();
}

NNH_TEST(load_tilemap_names_a_missing_tileset_image) {
    start(map_file::sample("tiles/nope"));
    const int base = fake::state().live_textures;
    auto result = nnh::load_tilemap("levels/other");
    CHECK(!result.ok() && result.error().code == ErrorCode::NotFound);
    CHECK(!result.ok() && std::strstr(result.error().message, "tiles/nope") != nullptr);
    CHECK(map_blocks() == 0 && fake::state().live_textures == base);
    nnh::shutdown();
}

NNH_TEST(load_tilemap_refuses_a_tileset_image_of_another_size) {
    // The map was converted for a 64x32 image; tiles/small is 32x32 and tiles/tall 64x64.
    for (const char* image : {"tiles/small", "tiles/tall"}) {
        start(map_file::sample(image));
        const int base = fake::state().live_textures;
        auto result = nnh::load_tilemap("levels/other");
        CHECK(!result.ok() && result.error().code == ErrorCode::Corrupted);
        CHECK(!result.ok() && std::strstr(result.error().message, image) != nullptr);
        CHECK(map_blocks() == 0 && fake::state().live_textures == base);
    }
    nnh::shutdown();
}

NNH_TEST(load_tilemap_frees_the_images_it_loaded_when_a_later_one_fails) {
    load_test::start();
    nnh::Texture world = std::move(nnh::load_texture("tiles/world").value());
    auto made = Tilemap::create({.width = 2, .height = 2});
    Tilemap built = std::move(made.value());
    CHECK(built.add_tileset({.texture = &world, .name = "tiles/world"}).ok());
    CHECK(built.add_tileset({.texture = &world, .name = "tiles/missing"}).ok());
    const map_file::Bytes file = map_file::from_map(built);
    built.reset();
    world.reset();
    start(file);
    const int base = fake::state().live_textures;
    CHECK(load_error("levels/other") == ErrorCode::NotFound);
    CHECK(map_blocks() == 0 && fake::state().live_textures == base);
    nnh::shutdown();
}

NNH_TEST(load_tilemap_reports_a_full_texture_table) {
    load_test::start();
    nnh::shutdown();
    nnh::AppConfig config;
    config.gfx.max_textures = 4; // the smallest table
    CHECK(nnh::init(config));
    nnh::Texture taken[4];
    for (nnh::Texture& t : taken) t = std::move(nnh::load_texture("tiles/small").value());
    CHECK(load_error("levels/one") == ErrorCode::LimitReached);
    CHECK(map_blocks() == 0);
    for (nnh::Texture& t : taken) t.reset();
    nnh::shutdown();
}
