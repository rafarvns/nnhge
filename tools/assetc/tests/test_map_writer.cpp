// The map model → `.nnhmap` (spec 012 T5.5): every section written as the engine reads it, capacities equal to
// counts, checked by the run-time validator; limits refused with a message.

#include <cstring>
#include <string>
#include <vector>

#include "map_writer.hpp"
#include "test.hpp"
#include "tilemap/map_layout.hpp"
#include "tilemap/map_validate.hpp"

using namespace nnh::assetc;
namespace fmt = nnh::detail::mapfmt;
using nnh::detail::MapView;

namespace {

Property prop_int(const char* name, int v) {
    Property p;
    p.name = name;
    p.type = fmt::PropertyType::Int;
    p.integer = v;
    return p;
}

Property prop_string(const char* name, const char* v) {
    Property p;
    p.name = name;
    p.type = fmt::PropertyType::String;
    p.text = v;
    return p;
}

/// A 4x2 map: two tilesets, two tile layers, one object layer with two objects, properties on every owner.
Map sample() {
    Map m;
    m.width = 4;
    m.height = 2;
    m.tile_width = m.tile_height = 16;
    m.parallax_origin_x = 8;
    m.parallax_origin_y = -2;
    m.properties = {prop_string("music", "theme")};
    Tileset a;
    a.first_id = 1;
    a.tile_count = 8;
    a.columns = 4;
    a.image_width = 64;
    a.image_height = 32;
    a.image = "tiles/world";
    a.tiles[2].collision = 1;
    a.tiles[4].collision = 2;
    a.tiles[4].properties = {prop_int("damage", 3)};
    Tileset b;
    b.first_id = 9;
    b.tile_count = 4;
    b.columns = 3;
    b.margin = 1;
    b.spacing = 2;
    b.image_width = b.image_height = 64;
    b.image = "tiles/extra";
    b.tiles[0].properties = {prop_string("kind", "theme")};
    m.tilesets = {a, b};
    TileLayer back;
    back.name = "back";
    back.parallax_x = 0.5f;
    back.opacity = 0.75f;
    back.visible = false;
    back.cells = {1, 0, 0, 0, 0, 0, 0, fmt::encode_cell(3, true, false, true)};
    back.properties = {prop_int("depth", 2)};
    TileLayer ground;
    ground.name = "ground";
    ground.collision = true;
    ground.offset_x = 3;
    ground.cells = {0, 9, 12, 0, 2, 2, 2, 2};
    m.tile_layers = {back, ground};
    ObjectLayer things;
    things.name = "things";
    things.properties = {prop_int("active", 1)};
    m.object_layers = {things};
    Object spawn;
    spawn.id = 7;
    spawn.name = "spawn";
    spawn.type = "start";
    spawn.shape = fmt::ObjectShape::Point;
    spawn.x = 16;
    spawn.y = 32;
    Object coin;
    coin.id = 9;
    coin.name = coin.type = "coin";
    coin.x = 48;
    coin.y = coin.w = coin.h = 16;
    coin.properties = {prop_int("value", 5)};
    m.objects = {spawn, coin};
    return m;
}

std::vector<std::uint8_t> written(const Map& m) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    CHECK(write_map(m, bytes, error));
    CHECK(error.empty());
    return bytes;
}

std::string error_of(const Map& m) {
    std::vector<std::uint8_t> bytes;
    std::string error;
    CHECK(!write_map(m, bytes, error));
    return error;
}

const char* str(const MapView& v, std::uint16_t offset) { return v.string(offset); }

/// The properties of a range as "name=value" joined by ';' (strings and ints only).
std::string props(const MapView& v, int first, int count) {
    std::string out;
    for (int i = first; i < first + count; ++i) {
        const fmt::PropertyEntry& p = v.properties()[i];
        out += std::string(str(v, p.name)) + "=";
        out += p.type == static_cast<std::uint8_t>(fmt::PropertyType::String) ? std::string(str(v, static_cast<std::uint16_t>(p.value)))
                                                                              : std::to_string(p.value);
        out += ";";
    }
    return out;
}

} // namespace

NNH_TEST(writer_output_passes_the_engine_validator) {
    std::vector<std::uint8_t> bytes = written(sample());
    CHECK(nnh::detail::validate_map(bytes.data(), bytes.size()).ok());
    const MapView view{bytes.data()};
    const fmt::Header& h = view.header();
    CHECK(h.file_size == bytes.size());
    CHECK(h.width == 4 && h.height == 2 && h.tile_width == 16 && h.tile_height == 16);
    CHECK(h.parallax_origin_x == 8 && h.parallax_origin_y == -2);
}

NNH_TEST(writer_capacities_equal_counts) {
    std::vector<std::uint8_t> bytes = written(sample());
    const MapView view{bytes.data()};
    const fmt::Header& h = view.header();
    CHECK(h.tilesets == 2 && h.tilesets_cap == 2 && h.tile_layers == 2 && h.tile_layers_cap == 2);
    CHECK(h.object_layers == 1 && h.object_layers_cap == 1 && h.objects == 2 && h.objects_cap == 2);
    CHECK(h.tile_ids == 12 && h.tile_ids_cap == 12 && h.tile_props == 2 && h.tile_props_cap == 2);
    CHECK(h.properties == 6 && h.properties_cap == 6);
    CHECK(h.string_bytes == h.string_cap);
    CHECK(h.animations == 0 && h.frames == 0 && h.animations_cap == 0 && h.frames_cap == 0);
}

NNH_TEST(writer_tilesets_and_tile_table) {
    std::vector<std::uint8_t> bytes = written(sample());
    const MapView v{bytes.data()};
    const fmt::TilesetEntry& b = v.tilesets()[1];
    CHECK(b.first_id == 9 && b.tile_count == 4 && b.columns == 3 && b.margin == 1 && b.spacing == 2);
    CHECK(std::strcmp(str(v, v.tilesets()[0].name), "tiles/world") == 0);
    CHECK(std::strcmp(str(v, b.name), "tiles/extra") == 0);
    CHECK(v.tile_sources()[2].x == 16 && v.tile_sources()[2].y == 0);
    CHECK(v.tile_sources()[6].x == 16 && v.tile_sources()[6].y == 16);
    CHECK(v.tile_sources()[10].x == 19 && v.tile_sources()[10].y == 1);  // margin 1, spacing 2
    CHECK(v.tile_sources()[12].x == 1 && v.tile_sources()[12].y == 19);
    CHECK((v.tile_flags()[3] & fmt::kTileCollisionMask) == 1);
    CHECK((v.tile_flags()[5] & fmt::kTileCollisionMask) == 2);
    CHECK((v.tile_flags()[5] & fmt::kTileHasProperties) != 0 && (v.tile_flags()[9] & fmt::kTileHasProperties) != 0);
    CHECK(v.tile_flags()[1] == 0);
    CHECK(v.tile_props()[0].tile_id == 5 && v.tile_props()[1].tile_id == 9);
    CHECK(props(v, v.tile_props()[0].properties_first, v.tile_props()[0].properties_count) == "damage=3;");
    CHECK(props(v, v.tile_props()[1].properties_first, v.tile_props()[1].properties_count) == "kind=theme;");
}

NNH_TEST(writer_tile_layers) {
    std::vector<std::uint8_t> bytes = written(sample());
    const MapView v{bytes.data()};
    const fmt::TileLayerEntry& back = v.tile_layers()[0];
    const fmt::TileLayerEntry& ground = v.tile_layers()[1];
    CHECK(std::strcmp(str(v, back.name), "back") == 0 && back.flags == 0);
    CHECK(back.parallax_x == 0.5f && back.parallax_y == 1 && back.opacity == 0.75f && back.tilesets_used == 1);
    CHECK(ground.flags == (fmt::kLayerVisible | fmt::kLayerCollision) && ground.tilesets_used == 3);
    CHECK(ground.offset_x == 3 && ground.offset_y == 0);
    CHECK(v.cells(0)[7] == fmt::encode_cell(3, true, false, true) && v.cells(1)[2] == 12 && v.cells(1)[0] == 0);
    CHECK(props(v, back.properties_first, back.properties_count) == "depth=2;");
    CHECK(ground.properties_count == 0);
}

NNH_TEST(writer_objects_and_object_layers) {
    std::vector<std::uint8_t> bytes = written(sample());
    const MapView v{bytes.data()};
    const fmt::ObjectLayerEntry& things = v.object_layers()[0];
    CHECK(std::strcmp(str(v, things.name), "things") == 0 && things.object_count == 2);
    CHECK(props(v, things.properties_first, things.properties_count) == "active=1;");
    const fmt::ObjectEntry& spawn = v.objects()[0];
    CHECK(spawn.id == 7 && spawn.shape == static_cast<std::uint8_t>(fmt::ObjectShape::Point));
    CHECK(std::strcmp(str(v, spawn.name), "spawn") == 0 && std::strcmp(str(v, spawn.type), "start") == 0);
    CHECK(spawn.x == 16 && spawn.y == 32 && spawn.w == 0);
    const fmt::ObjectEntry& coin = v.objects()[1];
    CHECK(coin.id == 9 && coin.x == 48 && coin.y == 16 && coin.w == 16 && coin.h == 16 && coin.object_layer == 0);
    CHECK(props(v, coin.properties_first, coin.properties_count) == "value=5;");
    CHECK(props(v, v.header().properties_first, v.header().properties_count) == "music=theme;");
}

NNH_TEST(writer_property_values_of_every_type) {
    Map m = sample();
    Property b, f;
    b.name = "on";
    b.type = fmt::PropertyType::Bool;
    b.boolean = true;
    f.name = "speed";
    f.type = fmt::PropertyType::Float;
    f.real = 2.5f;
    m.properties.push_back(b);
    m.properties.push_back(f);
    m.properties.push_back(prop_int("neg", -4));
    Property off = b;
    off.name = "off";
    off.boolean = false;
    m.properties.push_back(off);
    std::vector<std::uint8_t> bytes = written(m);
    const MapView v{bytes.data()};
    const fmt::PropertyEntry* p = v.properties() + v.header().properties_first;
    CHECK(v.header().properties_count == 5);
    CHECK(p[1].type == static_cast<std::uint8_t>(fmt::PropertyType::Bool) && p[1].value == 1);
    CHECK(p[4].type == static_cast<std::uint8_t>(fmt::PropertyType::Bool) && p[4].value == 0);
    float real = 0;
    std::memcpy(&real, &p[2].value, sizeof real);
    CHECK(p[2].type == static_cast<std::uint8_t>(fmt::PropertyType::Float) && real == 2.5f);
    CHECK(static_cast<std::int32_t>(p[3].value) == -4);
}

NNH_TEST(writer_stores_equal_strings_once) {
    std::vector<std::uint8_t> bytes = written(sample());
    // "", tiles/world, tiles/extra, back, ground, things, spawn, start, coin, music, theme, damage, kind, depth,
    // active, value: "theme" (a map property and a tile property) and "coin" (name and type) once each.
    std::size_t expected = 1;
    for (const char* s : {"tiles/world", "tiles/extra", "back", "ground", "things", "spawn", "start", "coin", "music",
                          "theme", "damage", "kind", "depth", "active", "value"}) {
        expected += std::strlen(s) + 1;
    }
    CHECK(MapView{bytes.data()}.header().string_bytes == expected);
}

NNH_TEST(writer_a_minimal_map) {
    Map m;
    m.width = m.height = 1;
    m.tile_width = m.tile_height = 8;
    std::vector<std::uint8_t> bytes = written(m);
    CHECK(nnh::detail::validate_map(bytes.data(), bytes.size()).ok());
}

NNH_TEST(writer_refuses_maps_over_the_limits) {
    Map m = sample();
    m.objects.resize(4097, m.objects[0]);
    CHECK(error_of(m).find("more than 4096 objects") != std::string::npos);
    m = sample();
    for (int i = 0; i < 16384; ++i) m.properties.push_back(prop_int("p", i));
    CHECK(error_of(m).find("more than 16384 properties") != std::string::npos);
    m = sample();
    for (int i = 0; i < 300; ++i) m.properties.push_back(prop_string(("k" + std::to_string(i)).c_str(), std::string(250, static_cast<char>('a' + i % 26)).append(std::to_string(i)).c_str()));
    CHECK(error_of(m).find("more than 64 KiB of strings") != std::string::npos);
}

NNH_TEST(writer_refuses_strings_over_255_bytes) {
    Map m = sample();
    m.tile_layers[0].name = std::string(256, 'x');
    CHECK(error_of(m).find("longer than 255 bytes") != std::string::npos);
}
