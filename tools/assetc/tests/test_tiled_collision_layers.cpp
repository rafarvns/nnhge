// `collision_layers` in Tiled (spec 019 FR-7, T5.1): an int (a flag enum saved as a number) other than 0 on tile
// layers and on tiles, kept as a property; anything else refused with `file: reason`.

#include <string>

#include "test.hpp"
#include "tiled_fixture.hpp"

using namespace nnh::assetc;
using tiled_fixture::Files;
using tiled_fixture::map_json;

namespace {

Files world() {
    Files f;
    f.add("tiles/world.png", tiled_fixture::png(64, 32));
    return f;
}

/// The map with an embedded world tileset (tiles `tiles`) and a "water" layer with `properties`.
std::string map_with(const std::string& layer_properties, const std::string& tiles = "[]") {
    return map_json(R"(, "tilesets": [{"firstgid": 1, "name": "world", "image": "../tiles/world.png",
        "imagewidth": 64, "imageheight": 32, "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 4,
        "tiles": )" + tiles + R"(}], "layers": [{"type": "tilelayer", "name": "water", "width": 4, "height": 2,
        "data": [1, 1, 0, 0, 0, 0, 2, 2], "properties": [{"name": "collision", "type": "bool", "value": true})" +
                    layer_properties + "]}]");
}

bool refused(Files& f, const std::string& json, const char* reason) {
    Map map;
    const bool ok = tiled_fixture::read_map(f, json, map);
    if (!ok && f.problem() != reason) std::printf("    got: %s\n", f.problem().c_str());
    return !ok && f.problem() == reason;
}

int property(const Properties& props, const char* name) {
    for (const Property& p : props) {
        if (p.name == name) return p.integer;
    }
    return -1;
}

} // namespace

NNH_TEST(collision_layers_on_a_layer_and_a_tile_are_kept_as_int_properties) {
    Files f = world();
    Map map;
    CHECK(tiled_fixture::read_map(
        f,
        map_with(R"(, {"name": "collision_layers", "type": "int", "propertytype": "CollisionLayers", "value": 6})",
                 R"([{"id": 1, "properties": [{"name": "one_way", "type": "bool", "value": true},
                                              {"name": "collision_layers", "type": "int", "value": 1073741824}]}])"),
        map));
    CHECK(map.tile_layers.size() == 1 && map.tile_layers[0].collision);
    CHECK(property(map.tile_layers[0].properties, "collision_layers") == 6);
    CHECK(map.tilesets[0].tiles.at(1).collision == 2);
    CHECK(property(map.tilesets[0].tiles.at(1).properties, "collision_layers") == 1073741824);
}

NNH_TEST(collision_layers_must_be_an_integer) {
    Files f = world();
    CHECK(refused(f, map_with(R"(, {"name": "collision_layers", "type": "string", "value": "water"})"),
                  "/game/assets/levels/map.tmj: layer \"water\": property \"collision_layers\" must be an integer "
                  "(the layer bits; in Tiled an enum with \"allow multiple values\" saved as a number)"));
    CHECK(refused(f, map_with("", R"([{"id": 1, "properties": [{"name": "solid", "type": "bool", "value": true},
                                       {"name": "collision_layers", "type": "float", "value": 2.5}]}])"),
                  "/game/assets/levels/map.tmj: tile 1: property \"collision_layers\" must be an integer (the layer "
                  "bits; in Tiled an enum with \"allow multiple values\" saved as a number)"));
}

NNH_TEST(collision_layers_must_not_be_zero) {
    Files f = world();
    CHECK(refused(f, map_with(R"(, {"name": "collision_layers", "type": "int", "value": 0})"),
                  "/game/assets/levels/map.tmj: layer \"water\": property \"collision_layers\" must not be 0: "
                  "untick collision instead"));
    CHECK(refused(f, map_with("", R"([{"id": 0, "properties": [{"name": "collision_layers", "type": "int",
                                                                  "value": 0}]}])"),
                  "/game/assets/levels/map.tmj: tile 0: property \"collision_layers\" must not be 0: remove solid or "
                  "one_way instead"));
}

NNH_TEST(maps_without_collision_layers_are_unchanged) {
    Files f = world();
    Map map;
    CHECK(tiled_fixture::read_map(f, map_with(""), map));
    CHECK(map.tile_layers[0].properties.empty());
}
