// Tiled objects → the map model (spec 012 T5.4): rectangles and points, names, types (`type` or `class`),
// properties, object layers, and the shapes that are refused.

#include <string>

#include "test.hpp"
#include "tiled_fixture.hpp"

using namespace nnh::assetc;
using nnh::detail::mapfmt::ObjectShape;
using tiled_fixture::Files;
using tiled_fixture::map_json;

namespace {

std::string with_objects(const std::string& objects, const std::string& layer_extra = "") {
    return map_json(R"(, "layers": [{"type": "objectgroup", "name": "things", "objects": [)" + objects + "]" +
                    layer_extra + "}]");
}

bool refused(const std::string& objects, const char* reason) {
    Files f;
    Map map;
    const bool ok = tiled_fixture::read_map(f, with_objects(objects), map);
    if (!ok && !f.mentions(reason)) std::printf("    got: %s\n", f.problem().c_str());
    return !ok && f.mentions(reason);
}

} // namespace

NNH_TEST(objects_rectangles_and_points) {
    Files f;
    Map map;
    CHECK(tiled_fixture::read_map(f, with_objects(R"(
        {"id": 3, "name": "coin", "type": "pickup", "x": 48, "y": 16.5, "width": 16, "height": 8},
        {"id": 7, "name": "spawn", "class": "start", "x": 8, "y": 32, "width": 0, "height": 0, "point": true})"),
                                  map));
    CHECK(map.objects.size() == 2);
    const Object& coin = map.objects[0];
    CHECK(coin.id == 3 && coin.name == "coin" && coin.type == "pickup" && coin.shape == ObjectShape::Rect);
    CHECK(coin.x == 48 && coin.y == 16.5f && coin.w == 16 && coin.h == 8 && coin.object_layer == 0);
    const Object& spawn = map.objects[1];
    CHECK(spawn.id == 7 && spawn.type == "start" && spawn.shape == ObjectShape::Point);
    CHECK(spawn.x == 8 && spawn.y == 32 && spawn.w == 0 && spawn.h == 0);
}

NNH_TEST(objects_points_drop_any_size) {
    Files f;
    Map map;
    CHECK(tiled_fixture::read_map(f, with_objects(R"({"id": 1, "x": 1, "y": 2, "width": 5, "height": 5,
                                                      "point": true})"), map));
    CHECK(map.objects[0].w == 0 && map.objects[0].h == 0);
}

NNH_TEST(objects_keep_their_properties_and_layer) {
    Files f;
    Map map;
    CHECK(tiled_fixture::read_map(f, map_json(R"(, "layers": [
        {"type": "objectgroup", "name": "a", "objects": [{"id": 1, "x": 0, "y": 0}]},
        {"type": "objectgroup", "name": "b", "properties": [{"name": "active", "type": "bool", "value": true}],
         "objects": [{"id": 2, "x": 0, "y": 0, "properties": [{"name": "value", "type": "int", "value": 5}]}]}])"),
                                  map));
    CHECK(map.object_layers.size() == 2 && map.object_layers[1].name == "b");
    CHECK(map.object_layers[1].properties.size() == 1 && map.object_layers[1].properties[0].boolean);
    CHECK(map.objects[0].object_layer == 0 && map.objects[1].object_layer == 1);
    CHECK(map.objects[1].properties.size() == 1 && map.objects[1].properties[0].integer == 5);
}

NNH_TEST(objects_type_wins_over_class_and_both_may_be_absent) {
    Files f;
    Map map;
    CHECK(tiled_fixture::read_map(f, with_objects(R"({"id": 1, "x": 0, "y": 0, "type": "t", "class": "c"},
                                                     {"id": 2, "x": 0, "y": 0, "type": "", "class": "c"},
                                                     {"id": 3, "x": 0, "y": 0})"), map));
    CHECK(map.objects[0].type == "t" && map.objects[1].type == "c" && map.objects[2].type.empty());
    CHECK(map.objects[2].name.empty());
}

NNH_TEST(objects_refuse_other_shapes) {
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "ellipse": true})", "ellipse objects aren't supported"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "polygon": [{"x": 0, "y": 0}]})", "polygon objects aren't supported"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "polyline": [{"x": 0, "y": 0}]})", "polyline objects aren't supported"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "text": {"text": "hi"}})", "text objects aren't supported"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "gid": 3})", "tile objects aren't supported"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "template": "a.tj"})", "templates aren't supported"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "rotation": 45})", "rotated objects aren't supported"));
}

NNH_TEST(objects_refuse_bad_values) {
    CHECK(refused(R"({"x": 0, "y": 0})", "\"id\" is missing"));
    CHECK(refused(R"({"id": 0, "x": 0, "y": 0})", "\"id\" must be a whole number from 1 to 2147483647"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "width": -1})", "\"width\" must be a number from 0"));
    CHECK(refused(R"({"id": 1, "x": "a", "y": 0})", "\"x\" must be a number"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "name": ")" + std::string(256, 'n') + "\"}",
                  "longer than 255 bytes"));
    CHECK(refused(R"({"id": 1, "x": 0, "y": 0, "properties": [{"name": "k", "type": "class", "value": {}}]})",
                  "class properties"));
}

NNH_TEST(objects_messages_name_the_object) {
    Files f;
    Map map;
    CHECK(!tiled_fixture::read_map(f, with_objects(R"({"id": 9, "name": "door", "x": 0, "y": 0, "ellipse": true})"),
                                   map));
    CHECK(f.mentions("object 9 (\"door\") in layer \"things\""));
}
