// A map built in code behaves like the same map made in Tiled (spec 012 FR-0, AC-2, T5.9): tests/data/maps/good.tmj
// is converted by nnh-assetc at build time (tests/CMakeLists.txt), loaded with load_tilemap, and compared query by
// query with the map below, built with the C++ API only.

#include <cstdio>
#include <cstring>
#include <string>

#include "load_fixture.hpp"
#include "nnh/texture.hpp"

#ifndef NNH_TEST_MAPS_DIR
#error "NNH_TEST_MAPS_DIR is set by tests/CMakeLists.txt"
#endif

namespace fake = nnh::platform_fake;
using nnh::PropertyOwner;
using nnh::PropertyType;
using nnh::Tile;
using nnh::Tilemap;
using nnh::Vec2;
using nnh::collision::Cell;

namespace {

std::string g_file; // the converted good.tmj (assets point at it)

std::string read_converted() {
    std::string bytes;
    std::FILE* f = std::fopen(NNH_TEST_MAPS_DIR "/good.nnhmap", "rb");
    CHECK(f != nullptr);
    if (f == nullptr) return bytes;
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) bytes.append(buffer, got);
    std::fclose(f);
    return bytes;
}

void start_with_good_map() {
    nnh::shutdown();
    fake::reset();
    g_file = read_converted();
    static const unsigned char kGood[] = {'F', 'A', 'K', 'E', 64, 0, 32, 0};
    static const unsigned char kSecond[] = {'F', 'A', 'K', 'E', 64, 0, 64, 0};
    static const char index[] = "levels/good.nnhmap\ntiles/good.png\ntiles/second.png\n";
    fake::add_asset(".nnh_assets", index, sizeof index - 1);
    fake::add_asset("levels/good.nnhmap", g_file.data(), g_file.size());
    fake::add_asset("tiles/good.png", kGood, sizeof kGood);
    fake::add_asset("tiles/second.png", kSecond, sizeof kSecond);
    CHECK(nnh::init());
}

void put(Tilemap& m, int layer, int x, int y, int id, bool fx = false, bool fy = false, bool diagonal = false) {
    m.set_tile(layer, x, y, Tile{static_cast<std::uint16_t>(id), fx, fy, diagonal});
}

/// good.tmj, written with the C++ API. Tiled's second tileset starts at gid 20: here its ids are 9..17.
Tilemap build_in_code(const nnh::Texture& good, const nnh::Texture& second) {
    auto made = Tilemap::create({.width = 6, .height = 4, .tile_width = 16, .tile_height = 16, .max_tile_layers = 3,
                                 .max_tilesets = 2, .max_tile_ids = 17, .max_object_layers = 1, .max_objects = 3,
                                 .max_properties = 16, .string_bytes = 256, .max_animations = 1,
                                 .max_animation_frames = 2});
    CHECK(made.ok());
    Tilemap m = std::move(made.value());
    CHECK(m.add_tileset({.texture = &good}).ok());
    CHECK(m.add_tileset({.texture = &second, .margin = 1, .spacing = 4, .tile_count = 9}).ok());
    m.set_parallax_origin({8, 4});
    CHECK(m.add_tile_layer({.name = "back", .offset = {2, -3}, .parallax = {0.5f, 1}, .opacity = 0.75f}).ok());
    CHECK(m.add_tile_layer({.name = "ground", .collision = true}).ok());
    CHECK(m.add_tile_layer({.name = "front", .visible = false}).ok());
    put(m, 0, 0, 0, 1);
    put(m, 0, 1, 0, 2, true);
    put(m, 0, 4, 0, 3);
    put(m, 0, 0, 2, 1, false, true);
    put(m, 0, 3, 2, 9);
    put(m, 0, 5, 3, 5, false, false, true);
    put(m, 1, 2, 1, 5);
    put(m, 1, 3, 1, 5);
    put(m, 1, 5, 2, 10);
    m.fill(1, 0, 3, 5, 1, Tile{3});
    put(m, 1, 5, 3, 3, true, true, true);
    put(m, 2, 0, 3, 17);
    m.set_tile_collision(3, Cell::Solid);
    m.set_tile_collision(5, Cell::OneWay);
    CHECK(m.set_string(PropertyOwner::map(), "music", "theme").ok());
    CHECK(m.set_float(PropertyOwner::map(), "gravity", 9.5f).ok());
    CHECK(m.set_int(PropertyOwner::map(), "lives", 3).ok());
    CHECK(m.set_bool(PropertyOwner::map(), "hard", true).ok());
    CHECK(m.set_int(PropertyOwner::tile(6), "damage", 2).ok());
    CHECK(m.set_string(PropertyOwner::tile(9), "kind", "water").ok());
    const Tilemap::AnimationFrame water[] = {{.tile_id = 9, .ms = 100}, {.tile_id = 11, .ms = 50}};
    CHECK(m.set_tile_animation(9, water, 2).ok());
    CHECK(m.set_float(PropertyOwner::tile_layer(0), "depth", 1.5f).ok());
    CHECK(m.add_object_layer("things").ok());
    CHECK(m.set_bool(PropertyOwner::object_layer(0), "active", true).ok());
    using nnh::ObjectShape;
    CHECK(m.add_object(0, {.name = "spawn", .type = "start", .shape = ObjectShape::Point, .bounds = {16, 32, 0, 0},
                           .id = 1}).ok());
    CHECK(m.add_object(0, {.name = "coin", .type = "coin", .bounds = {48, 16, 16, 16}, .id = 2}).ok());
    CHECK(m.add_object(0, {.name = "coin_b", .type = "coin", .bounds = {64, 16, 16, 16}, .id = 5}).ok());
    CHECK(m.set_int(PropertyOwner::object(1), "value", 5).ok());
    return m;
}

bool same_properties(const nnh::Properties& a, const nnh::Properties& b) {
    if (a.count() != b.count()) return false;
    for (int i = 0; i < a.count(); ++i) {
        const char* name = a.name(i);
        if (std::strcmp(name, b.name(i)) != 0 || a.type(name) != b.type(name)) return false;
        if (a.get_bool(name) != b.get_bool(name) || a.get_int(name) != b.get_int(name) ||
            a.get_float(name) != b.get_float(name) || std::strcmp(a.get_string(name), b.get_string(name)) != 0) {
            return false;
        }
    }
    return true;
}

bool same_object(const nnh::MapObject& a, const nnh::MapObject& b) {
    return a.id == b.id && std::strcmp(a.name, b.name) == 0 && std::strcmp(a.type, b.type) == 0 &&
           a.shape == b.shape && a.bounds.x == b.bounds.x && a.bounds.y == b.bounds.y && a.bounds.w == b.bounds.w &&
           a.bounds.h == b.bounds.h && a.object_layer == b.object_layer && same_properties(a.properties, b.properties);
}

} // namespace

NNH_TEST(code_vs_file_the_converted_fixture_is_a_valid_map) {
    start_with_good_map();
    CHECK(!g_file.empty());
    CHECK(nnh::load_tilemap("levels/good").ok());
    nnh::shutdown();
}

NNH_TEST(code_vs_file_sizes_layers_and_settings_match) {
    start_with_good_map();
    {
        auto good = nnh::load_texture("tiles/good");
        auto second = nnh::load_texture("tiles/second");
        CHECK(good.ok() && second.ok());
        const Tilemap code = build_in_code(good.value(), second.value());
        auto loaded = nnh::load_tilemap("levels/good");
        CHECK(loaded.ok());
        const Tilemap& file = loaded.value();
        CHECK(code.width() == file.width() && code.height() == file.height());
        CHECK(code.tile_width() == file.tile_width() && code.tile_height() == file.tile_height());
        CHECK(code.tileset_count() == file.tileset_count() && code.tile_id_count() == file.tile_id_count());
        CHECK(code.parallax_origin() == file.parallax_origin());
        CHECK(code.tile_layer_count() == 3 && file.tile_layer_count() == 3);
        for (int l = 0; l < 3; ++l) {
            CHECK(std::strcmp(code.tile_layer_name(l), file.tile_layer_name(l)) == 0);
            CHECK(code.layer_offset(l) == file.layer_offset(l) && code.layer_parallax(l) == file.layer_parallax(l));
            CHECK(code.layer_opacity(l) == file.layer_opacity(l) && code.layer_visible(l) == file.layer_visible(l));
            CHECK(code.layer_collision(l) == file.layer_collision(l));
            CHECK(same_properties(code.properties(PropertyOwner::tile_layer(l)),
                                  file.properties(PropertyOwner::tile_layer(l))));
        }
    }
    nnh::shutdown();
}

NNH_TEST(code_vs_file_tiles_and_collision_match) {
    start_with_good_map();
    {
        auto good = nnh::load_texture("tiles/good");
        auto second = nnh::load_texture("tiles/second");
        const Tilemap code = build_in_code(good.value(), second.value());
        auto loaded = nnh::load_tilemap("levels/good");
        const Tilemap& file = loaded.value();
        int non_empty = 0;
        for (int l = 0; l < 3; ++l) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 6; ++x) {
                    CHECK(code.tile(l, x, y) == file.tile(l, x, y));
                    non_empty += file.tile(l, x, y).empty() ? 0 : 1;
                }
            }
        }
        CHECK(non_empty == 16);
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 6; ++x) CHECK(code.collision_at(x, y) == file.collision_at(x, y));
        }
        for (int id = 0; id <= 18; ++id) {
            CHECK(code.tile_collision(id) == file.tile_collision(id));
            CHECK(same_properties(code.properties(PropertyOwner::tile(id)), file.properties(PropertyOwner::tile(id))));
        }
        CHECK(file.tile_collision(3) == Cell::Solid && file.tile_collision(5) == Cell::OneWay);
        CHECK(file.collision_at(2, 1) == Cell::Solid && file.collision_at(1, 1) == Cell::Empty);
    }
    nnh::shutdown();
}

NNH_TEST(code_vs_file_objects_and_properties_match) {
    start_with_good_map();
    {
        auto good = nnh::load_texture("tiles/good");
        auto second = nnh::load_texture("tiles/second");
        const Tilemap code = build_in_code(good.value(), second.value());
        auto loaded = nnh::load_tilemap("levels/good");
        const Tilemap& file = loaded.value();
        CHECK(same_properties(code.properties(), file.properties()));
        CHECK(file.properties().count() == 4 && file.properties().get_float("gravity") == 9.5f);
        CHECK(code.object_layer_count() == file.object_layer_count());
        CHECK(std::strcmp(code.object_layer_name(0), file.object_layer_name(0)) == 0);
        CHECK(same_properties(code.properties(PropertyOwner::object_layer(0)),
                              file.properties(PropertyOwner::object_layer(0))));
        CHECK(code.object_count() == 3 && file.object_count() == 3);
        for (int i = 0; i < 3; ++i) CHECK(same_object(code.object(i), file.object(i)));
        int code_coins[3] = {}, file_coins[3] = {};
        CHECK(code.find_objects("coin", code_coins, 3) == 2 && file.find_objects("coin", file_coins, 3) == 2);
        CHECK(code_coins[0] == file_coins[0] && code_coins[1] == file_coins[1]);
        CHECK(code.find_object("coin_b") == file.find_object("coin_b"));
    }
    nnh::shutdown();
}

NNH_TEST(code_vs_file_animations_match) {
    start_with_good_map();
    {
        auto good = nnh::load_texture("tiles/good");
        auto second = nnh::load_texture("tiles/second");
        const Tilemap code = build_in_code(good.value(), second.value());
        auto loaded = nnh::load_tilemap("levels/good");
        const Tilemap& file = loaded.value();
        int animated = 0;
        for (int id = 0; id <= 18; ++id) {
            Tilemap::AnimationFrame a[4] = {}, b[4] = {};
            const int count = code.tile_animation(id, a, 4);
            CHECK(file.tile_animation(id, b, 4) == count);
            for (int i = 0; i < count; ++i) CHECK(a[i].tile_id == b[i].tile_id && a[i].ms == b[i].ms);
            for (const int ms : {0, 99, 100, 149, 150, 1234}) {
                CHECK(code.animated_tile_id(id, nnh::Duration::from_ms(ms)) ==
                      file.animated_tile_id(id, nnh::Duration::from_ms(ms)));
            }
            animated += count > 0 ? 1 : 0;
        }
        CHECK(animated == 1 && file.animated_tile_id(9, nnh::Duration::from_ms(120)) == 11);
    }
    nnh::shutdown();
}
