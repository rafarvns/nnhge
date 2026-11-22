// Tiled maps → the map model (spec 012 T5.3): map checks, tilesets' ids, tile layers (CSV and base64 data, flips,
// settings, `collision`), parallax origin, properties, and what is refused.

#include <string>

#include "test.hpp"
#include "tiled_fixture.hpp"
#include "tiled_map.hpp"
#include "tilemap/map_format.hpp"

using namespace nnh::assetc;
using nnh::detail::mapfmt::encode_cell;
using tiled_fixture::Files;
using tiled_fixture::map_json;

namespace {

constexpr const char* kWorldSet = R"("tilesets": [{"firstgid": 1, "source": "../tiles/world.tsj"}])";

Files world() {
    Files f;
    f.add("tiles/world.tsj", R"({"image": "world.png", "imagewidth": 64, "imageheight": 32, "tilewidth": 16,
                                 "tileheight": 16, "tilecount": 8, "columns": 4})");
    f.add("tiles/world.png", tiled_fixture::png(64, 32));
    f.add("tiles/small.png", tiled_fixture::png(32, 16));
    return f;
}

/// A map with the world tileset and one tile layer of 4x2 cells `data` and `extra` layer members.
std::string one_layer(const std::string& data, const std::string& extra = "") {
    return map_json(std::string(", ") + kWorldSet + R"(, "layers": [{"type": "tilelayer", "name": "ground",
        "width": 4, "height": 2, "data": )" + data + extra + "}]");
}

bool refused(Files& f, const std::string& json, const char* reason) {
    Map map;
    const bool ok = tiled_fixture::read_map(f, json, map);
    if (!ok && !f.mentions(reason)) std::printf("    got: %s\n", f.problem().c_str());
    return !ok && f.mentions(reason);
}

} // namespace

NNH_TEST(map_reads_size_tile_size_and_parallax_origin) {
    Files f = world();
    Map map;
    CHECK(tiled_fixture::read_map(f, map_json(R"(, "parallaxoriginx": 8, "parallaxoriginy": -4.5)"), map));
    CHECK(map.width == 4 && map.height == 2 && map.tile_width == 16 && map.tile_height == 16);
    CHECK(map.parallax_origin_x == 8 && map.parallax_origin_y == -4.5f);
    CHECK(map.tilesets.empty() && map.tile_layers.empty() && map.object_layers.empty());
}

NNH_TEST(map_reads_its_properties) {
    Files f = world();
    Map map;
    CHECK(tiled_fixture::read_map(f, map_json(R"(, "properties": [
        {"name": "music", "type": "string", "value": "theme"}, {"name": "gravity", "type": "float", "value": 9.5},
        {"name": "tint", "type": "color", "value": "#ff102030"}, {"name": "next", "type": "file", "value": "b.tmj"},
        {"name": "boss", "type": "object", "value": 12}, {"name": "hard", "type": "bool", "value": true},
        {"name": "lives", "type": "int", "value": -3}, {"name": "plain", "value": "no type"}])"), map));
    CHECK(map.properties.size() == 8);
    CHECK(map.properties[0].name == "music" && map.properties[0].text == "theme");
    CHECK(map.properties[1].real == 9.5f);
    CHECK(map.properties[2].text == "#ff102030");
    CHECK(map.properties[3].text == "b.tmj");
    CHECK(map.properties[4].integer == 12);
    CHECK(map.properties[5].boolean);
    CHECK(map.properties[6].integer == -3);
    CHECK(map.properties[7].text == "no type");
}

NNH_TEST(map_refuses_bad_properties) {
    Files f = world();
    CHECK(refused(f, map_json(R"(, "properties": [{"name": "c", "type": "class", "value": {}}])"),
                  "class properties aren't supported"));
    CHECK(refused(f, map_json(R"(, "properties": [{"name": "a", "value": "1"}, {"name": "a", "value": "2"}])"),
                  "property \"a\" is there twice"));
    CHECK(refused(f, map_json(R"(, "properties": [{"name": "i", "type": "int", "value": 1.5}])"), "whole number"));
    CHECK(refused(f, map_json(R"(, "properties": [{"name": "i", "type": "int", "value": 3000000000}])"),
                  "whole number"));
    CHECK(refused(f, map_json(R"(, "properties": [{"name": "", "value": "x"}])"), "name of 1 to 255 bytes"));
    CHECK(refused(f, map_json(R"(, "properties": [{"name": "s", "value": ")" + std::string(256, 'x') + "\"}]"),
                  "longer than 255 bytes"));
    CHECK(refused(f, map_json(R"(, "properties": [{"name": "w", "type": "weird", "value": 1}])"),
                  "unknown property type \"weird\""));
}

NNH_TEST(map_gives_tilesets_consecutive_ids_whatever_their_first_gids) {
    Files f = world();
    f.add("tiles/small.tsj", R"({"image": "small.png", "imagewidth": 32, "imageheight": 16, "tilewidth": 16,
                                 "tileheight": 16, "tilecount": 2, "columns": 2})");
    Map map;
    CHECK(tiled_fixture::read_map(f, map_json(R"(, "tilesets": [{"firstgid": 100, "source": "../tiles/small.tsj"},
        {"firstgid": 1, "source": "../tiles/world.tsj"}], "layers": [{"type": "tilelayer", "name": "a",
        "width": 4, "height": 2, "data": [1, 8, 100, 101, 0, 0, 0, 0]}])"), map));
    CHECK(map.tilesets.size() == 2);
    CHECK(map.tilesets[0].image == "tiles/world" && map.tilesets[0].first_id == 1);
    CHECK(map.tilesets[1].image == "tiles/small" && map.tilesets[1].first_id == 9);
    const std::vector<std::uint16_t>& cells = map.tile_layers[0].cells;
    CHECK(cells[0] == 1 && cells[1] == 8 && cells[2] == 9 && cells[3] == 10 && cells[4] == 0);
}

NNH_TEST(map_reads_csv_layer_data_with_flips) {
    Files f = world();
    Map map;
    // gid 2 flipped x (bit 31), gid 3 flipped y (bit 30), gid 4 diagonal (bit 29), gid 5 all three.
    CHECK(tiled_fixture::read_map(f, one_layer("[2147483650, 1073741827, 536870916, 3758096389, 0, 1, 0, 8]"), map));
    const std::vector<std::uint16_t>& c = map.tile_layers[0].cells;
    CHECK(c.size() == 8);
    CHECK(c[0] == encode_cell(2, true, false, false));
    CHECK(c[1] == encode_cell(3, false, true, false));
    CHECK(c[2] == encode_cell(4, false, false, true));
    CHECK(c[3] == encode_cell(5, true, true, true));
    CHECK(c[4] == 0 && c[5] == 1 && c[7] == 8);
}

NNH_TEST(map_reads_uncompressed_base64_layer_data) {
    Files f = world();
    Map map;
    // gids 1, 2, 0, 0, 0, 0, 0, 2^31 + 8 as little-endian u32.
    CHECK(tiled_fixture::read_map(f, one_layer(R"("AQAAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAIA=")",
                                               R"(, "encoding": "base64", "compression": "")"), map));
    const std::vector<std::uint16_t>& c = map.tile_layers[0].cells;
    CHECK(c[0] == 1 && c[1] == 2 && c[2] == 0 && c[7] == encode_cell(8, true, false, false));
}

NNH_TEST(map_refuses_compressed_or_broken_layer_data) {
    Files f = world();
    for (const char* compression : {"zlib", "gzip", "zstd"}) {
        CHECK(refused(f, one_layer(R"("eJxjYGBgAAAABAAB")",
                                   std::string(R"(, "encoding": "base64", "compression": ")") + compression + "\""),
                      "save the layer format as CSV"));
    }
    CHECK(refused(f, one_layer(R"("AQAAAA==")", R"(, "encoding": "base64")"), "8 cells"));
    CHECK(refused(f, one_layer(R"("A*AAAA==")", R"(, "encoding": "base64")"), "not valid base64"));
    CHECK(refused(f, one_layer("[1, 2, 3]"), "8 cells"));
    CHECK(refused(f, one_layer("[1, 2, 3, 4, 5, 6, 7, 8, 1]"), "8 cells"));
    CHECK(refused(f, one_layer(R"("AQAAAAIAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAIABAAAA")", R"(, "encoding": "base64")"),
                  "8 cells")); // 9 gids
    CHECK(refused(f, one_layer("[1, 2, 3, 4, 5, 6, 7, -1]"), "whole numbers from 0 to 4294967295"));
    CHECK(refused(f, one_layer("[1, 2, 3, 4, 5, 6, 7, 1.5]"), "whole numbers from 0 to 4294967295"));
    CHECK(refused(f, one_layer("[1, 2, 3, 4, 5, 6, 7, 9]"), "cell (3, 1) uses tile gid 9, which is in no tileset"));
    CHECK(refused(f, one_layer("[268435457, 0, 0, 0, 0, 0, 0, 0]"), "hexagonal"));
    CHECK(refused(f, one_layer("{}"), "\"data\""));
}

NNH_TEST(map_reads_tile_layer_settings_and_collision) {
    Files f = world();
    Map map;
    CHECK(tiled_fixture::read_map(f, one_layer("[0, 0, 0, 0, 0, 0, 0, 0]", R"(, "offsetx": 3, "offsety": -2.5,
        "parallaxx": 0.5, "parallaxy": 0, "opacity": 0.25, "visible": false, "properties": [
        {"name": "collision", "type": "bool", "value": true}, {"name": "depth", "type": "int", "value": 2}])"), map));
    const TileLayer& l = map.tile_layers[0];
    CHECK(l.name == "ground");
    CHECK(l.offset_x == 3 && l.offset_y == -2.5f && l.parallax_x == 0.5f && l.parallax_y == 0);
    CHECK(l.opacity == 0.25f && !l.visible && l.collision);
    CHECK(l.properties.size() == 1 && l.properties[0].name == "depth");
}

NNH_TEST(map_tile_layer_defaults) {
    Files f = world();
    Map map;
    CHECK(tiled_fixture::read_map(f, one_layer("[0, 0, 0, 0, 0, 0, 0, 0]"), map));
    const TileLayer& l = map.tile_layers[0];
    CHECK(l.offset_x == 0 && l.parallax_x == 1 && l.parallax_y == 1 && l.opacity == 1 && l.visible && !l.collision);
}

NNH_TEST(map_keeps_layer_order_by_kind) {
    Files f = world();
    Map map;
    CHECK(tiled_fixture::read_map(f, map_json(R"(, "layers": [
        {"type": "tilelayer", "name": "back", "width": 4, "height": 2, "data": [0,0,0,0,0,0,0,0]},
        {"type": "objectgroup", "name": "things", "objects": []},
        {"type": "tilelayer", "name": "front", "width": 4, "height": 2, "data": [0,0,0,0,0,0,0,0]}])"), map));
    CHECK(map.tile_layers.size() == 2 && map.tile_layers[0].name == "back" && map.tile_layers[1].name == "front");
    CHECK(map.object_layers.size() == 1 && map.object_layers[0].name == "things");
}

NNH_TEST(map_refuses_other_orientations_and_infinite_maps) {
    Files f = world();
    CHECK(refused(f, R"({"orientation": "isometric", "width": 4, "height": 2, "tilewidth": 16, "tileheight": 16})",
                  "isometric maps aren't supported"));
    CHECK(refused(f, R"({"orientation": "hexagonal", "width": 4, "height": 2, "tilewidth": 16, "tileheight": 16})",
                  "hexagonal maps aren't supported"));
    CHECK(refused(f, R"({"orientation": "staggered", "width": 4, "height": 2, "tilewidth": 16, "tileheight": 16})",
                  "staggered maps aren't supported"));
    CHECK(refused(f, R"({"orientation": "orthogonal", "infinite": true, "width": 4, "height": 2, "tilewidth": 16,
                         "tileheight": 16})", "infinite maps aren't supported"));
    CHECK(refused(f, R"({"type": "tileset", "orientation": "orthogonal"})", "not a map"));
    CHECK(f.ctx.problem.file == "/game/assets/levels/map.tmj");
}

NNH_TEST(map_refuses_other_layer_kinds) {
    Files f = world();
    CHECK(refused(f, map_json(R"(, "layers": [{"type": "imagelayer", "name": "sky", "image": "sky.png"}])"),
                  "image layers aren't supported"));
    CHECK(refused(f, map_json(R"(, "layers": [{"type": "group", "name": "g", "layers": []}])"),
                  "group layers aren't supported"));
    CHECK(refused(f, map_json(R"(, "layers": [{"type": "tilelayer", "name": "c", "width": 4, "height": 2,
                                  "chunks": []}])"), "infinite"));
    CHECK(refused(f, map_json(R"(, "layers": [{"type": "tilelayer", "name": "s", "width": 2, "height": 2,
                                  "data": [0, 0, 0, 0]}])"), "layer \"s\": the layer is 2x2 but the map is 4x2"));
    CHECK(refused(f, map_json(R"(, "layers": [{"type": "tilelayer", "name": "t", "width": 4, "height": 3,
                                  "data": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]}])"), "the layer is 4x3 but the map is 4x2"));
    CHECK(refused(f, one_layer("[0,0,0,0,0,0,0,0]", R"(, "opacity": 1.5)"), "\"opacity\" must be a number from 0 to 1"));
    CHECK(refused(f, one_layer("[0,0,0,0,0,0,0,0]", R"(, "tintcolor": "#ff0000")"), "tint"));
}

NNH_TEST(map_refuses_sizes_outside_the_limits) {
    Files f = world();
    CHECK(refused(f, R"({"orientation": "orthogonal", "width": 4, "height": 2, "tilewidth": 4, "tileheight": 4})",
                  "tiles are 4x4: NNHGE tiles are 8 to 64 pixels per side"));
    CHECK(refused(f, R"({"orientation": "orthogonal", "width": 4, "height": 2, "tilewidth": 128, "tileheight": 16})",
                  "tiles are 128x16"));
    CHECK(refused(f, R"({"orientation": "orthogonal", "width": 1025, "height": 2, "tilewidth": 16, "tileheight": 16})",
                  "\"width\" must be a whole number from 1 to 1024"));
}

NNH_TEST(map_refuses_diagonal_flips_of_non_square_tiles) {
    Files f;
    f.add("tiles/wide.png", tiled_fixture::png(64, 16));
    const std::string json = R"({"orientation": "orthogonal", "width": 2, "height": 1, "tilewidth": 32,
        "tileheight": 16, "tilesets": [{"firstgid": 1, "image": "../tiles/wide.png", "imagewidth": 64,
        "imageheight": 16, "tilewidth": 32, "tileheight": 16, "tilecount": 2, "columns": 2}],
        "layers": [{"type": "tilelayer", "name": "w", "width": 2, "height": 1, "data": [2147483649, X]}]})";
    Map map;
    std::string ok = json;
    ok.replace(ok.find('X'), 1, "2");
    CHECK(tiled_fixture::read_map(f, ok, map));
    std::string bad = json;
    bad.replace(bad.find('X'), 1, "536870914");
    CHECK(refused(f, bad, "layer \"w\": cell (1, 0) is flipped diagonally, which needs square tiles"));
}

NNH_TEST(map_refuses_too_many_layers_tilesets_or_tiles) {
    Files f = world();
    std::string layers = R"(, "layers": [)";
    for (int i = 0; i < 17; ++i) layers += std::string(i ? "," : "") + R"({"type": "tilelayer", "name": "l",
        "width": 4, "height": 2, "data": [0,0,0,0,0,0,0,0]})";
    CHECK(refused(f, map_json(layers + "]"), "more than 16 tile layers"));
    std::string objects = R"(, "layers": [)";
    for (int i = 0; i < 17; ++i) objects += std::string(i ? "," : "") + R"({"type": "objectgroup", "objects": []})";
    CHECK(refused(f, map_json(objects + "]"), "more than 16 object layers"));
    // 5 full layers of 1024x1024: refused before any layer data is read (the data here is empty).
    std::string big = R"({"orientation": "orthogonal", "width": 1024, "height": 1024, "tilewidth": 16,
                          "tileheight": 16, "layers": [)";
    for (int i = 0; i < 5; ++i) big += std::string(i ? "," : "") + R"({"type": "tilelayer", "name": "l", "data": []})";
    CHECK(refused(f, big + "]}", "the tile layers have 5242880 cells in all: a map can have at most 4194304"));
    std::string sets = R"(, "tilesets": [)";
    for (int i = 0; i < 9; ++i) {
        sets += std::string(i ? "," : "") + R"({"firstgid": )" + std::to_string(1 + 8 * i) +
                R"(, "source": "../tiles/world.tsj"})";
    }
    CHECK(refused(f, map_json(sets + "]"), "more than 8 tilesets"));
    f.add("tiles/big.png", tiled_fixture::png(1024, 1024));
    CHECK(refused(f, R"({"orientation": "orthogonal", "width": 4, "height": 2, "tilewidth": 8, "tileheight": 8,
        "tilesets": [{"firstgid": 1, "image": "../tiles/big.png", "imagewidth": 1024, "imageheight": 1024,
        "tilewidth": 8, "tileheight": 8, "tilecount": 16384, "columns": 128}]})",
                  "16384 tiles in all: a map can use at most 8191"));
    f.add("tiles/half.png", tiled_fixture::png(1024, 512)); // 8192 tiles of 8x8
    const std::string ids = R"({"orientation": "orthogonal", "width": 4, "height": 2, "tilewidth": 8, "tileheight": 8,
        "tilesets": [{"firstgid": 1, "image": "../tiles/half.png", "imagewidth": 1024, "imageheight": 512,
        "tilewidth": 8, "tileheight": 8, "tilecount": COUNT, "columns": 128}]})";
    std::string over = ids, limit = ids;
    over.replace(over.find("COUNT"), 5, "8192");
    limit.replace(limit.find("COUNT"), 5, "8191");
    CHECK(refused(f, over, "the tilesets have 8192 tiles in all: a map can use at most 8191"));
    Map map;
    CHECK(tiled_fixture::read_map(f, limit, map));
    CHECK(refused(f, map_json(R"(, "tilesets": [{"firstgid": 1, "source": "../tiles/world.tsj"},
                                 {"firstgid": 1, "source": "../tiles/world.tsj"}])"),
                  "two tilesets start at firstgid 1"));
}

NNH_TEST(map_refuses_a_missing_or_broken_file) {
    Files f;
    Map map;
    CHECK(!tiled_fixture::read_map(f, "", map));
    CHECK(f.mentions("map.tmj: line 1, column 1: expected a value"));
    Files g;
    CHECK(!read_tiled_map("/game/assets/levels/none.tmj", g.ctx, map));
    CHECK(g.problem() == "/game/assets/levels/none.tmj: file not found");
}
