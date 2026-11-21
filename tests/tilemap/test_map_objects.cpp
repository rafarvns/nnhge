// Object layers and objects (spec 012 FR-0, FR-5, FR-6).

#include <cstring>

#include "map_fixture.hpp"

using map_test::make;
using nnh::ErrorCode;
using nnh::ObjectShape;
using nnh::PropertyOwner;
using nnh::Tilemap;

namespace {

bool same(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

ErrorCode object_error(Tilemap& map, int layer, const nnh::ObjectDesc& desc) {
    auto r = map.add_object(layer, desc);
    return r ? ErrorCode::None : r.error().code;
}

int add(Tilemap& map, int layer, const nnh::ObjectDesc& desc) {
    auto r = map.add_object(layer, desc);
    CHECK(r.ok());
    return r ? r.value() : -1;
}

} // namespace

NNH_TEST(object_layers_are_added_in_order_with_names) {
    Tilemap map = make({.width = 4, .height = 4, .max_object_layers = 2});
    CHECK(map.object_layer_count() == 0);
    CHECK(map.add_object_layer("coins").value_or(-1) == 0);
    CHECK(map.add_object_layer("enemies").value_or(-1) == 1);
    CHECK(map.object_layer_count() == 2);
    CHECK(same(map.object_layer_name(0), "coins") && same(map.object_layer_name(1), "enemies"));
    CHECK(same(map.object_layer_name(2), "") && same(map.object_layer_name(-1), ""));
}

NNH_TEST(object_layers_beyond_the_capacity_are_refused) {
    Tilemap map = make({.width = 4, .height = 4, .max_object_layers = 1});
    CHECK(map.add_object_layer("a").ok());
    auto full = map.add_object_layer("b");
    CHECK(!full.ok() && full.error().code == ErrorCode::LimitReached);
    char long_name[300];
    std::memset(long_name, 'n', sizeof long_name - 1);
    long_name[299] = '\0';
    Tilemap other = make({.width = 4, .height = 4});
    CHECK(other.add_object_layer(long_name).error().code == ErrorCode::InvalidArgument);
    CHECK(other.add_object_layer(nullptr).error().code == ErrorCode::InvalidArgument);
}

NNH_TEST(rect_and_point_objects_keep_their_fields) {
    Tilemap map = make({.width = 4, .height = 4});
    const int layer = map.add_object_layer("things").value_or(-1);
    CHECK(add(map, layer, {.name = "door", .type = "Door", .bounds = {16, 32, 16, 24}}) == 0);
    CHECK(add(map, layer, {.name = "spawn", .shape = ObjectShape::Point, .bounds = {40, 50, 9, 9}}) == 1);
    CHECK(map.object_count() == 2);
    const nnh::MapObject door = map.object(0);
    CHECK(door.id == 1 && same(door.name, "door") && same(door.type, "Door") && door.shape == ObjectShape::Rect);
    CHECK(door.bounds.x == 16 && door.bounds.y == 32 && door.bounds.w == 16 && door.bounds.h == 24);
    CHECK(door.object_layer == layer);
    const nnh::MapObject spawn = map.object(1);
    CHECK(spawn.id == 2 && spawn.shape == ObjectShape::Point && same(spawn.type, ""));
    CHECK(spawn.bounds.x == 40 && spawn.bounds.y == 50 && spawn.bounds.w == 0 && spawn.bounds.h == 0);
}

NNH_TEST(object_ids_are_automatic_or_given) {
    Tilemap map = make({.width = 4, .height = 4});
    const int layer = map.add_object_layer("o").value_or(-1);
    add(map, layer, {.id = 10});
    add(map, layer, {});          // highest + 1
    add(map, layer, {.id = 3});
    add(map, layer, {});
    CHECK(map.object(0).id == 10 && map.object(1).id == 11 && map.object(2).id == 3 && map.object(3).id == 12);
    CHECK(object_error(map, layer, {.id = -1}) == ErrorCode::InvalidArgument);
}

NNH_TEST(objects_remember_their_layer_whatever_the_order) {
    Tilemap map = make({.width = 4, .height = 4, .max_object_layers = 2});
    const int a = map.add_object_layer("a").value_or(-1);
    const int b = map.add_object_layer("b").value_or(-1);
    add(map, b, {.name = "b0"});
    add(map, a, {.name = "a0"});
    add(map, b, {.name = "b1"});
    CHECK(map.object(0).object_layer == b && map.object(1).object_layer == a && map.object(2).object_layer == b);
    CHECK(same(map.object(2).name, "b1"));
}

NNH_TEST(objects_beyond_the_capacity_or_on_bad_layers_are_refused) {
    Tilemap map = make({.width = 4, .height = 4, .max_objects = 2});
    const int layer = map.add_object_layer("o").value_or(-1);
    CHECK(object_error(map, 1, {}) == ErrorCode::InvalidArgument);
    CHECK(object_error(map, -1, {}) == ErrorCode::InvalidArgument);
    CHECK(object_error(map, layer, {.bounds = {0, 0, -1, 4}}) == ErrorCode::InvalidArgument);
    CHECK(object_error(map, layer, {.name = nullptr}) == ErrorCode::InvalidArgument);
    add(map, layer, {});
    add(map, layer, {});
    CHECK(object_error(map, layer, {}) == ErrorCode::LimitReached);
    CHECK(map.object_count() == 2);
}

NNH_TEST(objects_need_room_for_their_strings) {
    Tilemap map = make({.width = 4, .height = 4, .string_bytes = 6});
    const int layer = map.add_object_layer("").value_or(-1);
    CHECK(object_error(map, layer, {.name = "abc", .type = "def"}) == ErrorCode::LimitReached);
    CHECK(object_error(map, layer, {.name = "abc", .type = "abc"}) == ErrorCode::None);
    CHECK(map.object_count() == 1);
}

NNH_TEST(invalid_object_indices_give_an_empty_object) {
    Tilemap map = make({.width = 4, .height = 4});
    CHECK(map.object(0).id == 0 && map.object(-1).id == 0 && map.object(0).object_layer == -1);
    CHECK(same(map.object(5).name, "") && map.object(5).properties.count() == 0);
}

NNH_TEST(objects_are_found_by_name) {
    Tilemap map = make({.width = 4, .height = 4});
    const int layer = map.add_object_layer("o").value_or(-1);
    add(map, layer, {.name = "spawn"});
    add(map, layer, {.name = "exit"});
    add(map, layer, {.name = "exit"});
    CHECK(map.find_object("exit") == 1 && map.find_object("spawn") == 0);
    CHECK(map.find_object("nope") == -1 && map.find_object(nullptr) == -1);
}

NNH_TEST(objects_are_found_by_type_up_to_a_capacity) {
    Tilemap map = make({.width = 4, .height = 4});
    const int layer = map.add_object_layer("o").value_or(-1);
    add(map, layer, {.type = "coin"});
    add(map, layer, {.type = "enemy"});
    add(map, layer, {.type = "coin"});
    add(map, layer, {.type = "coin"});
    int found[2] = {-1, -1};
    CHECK(map.find_objects("coin", found, 2) == 3);
    CHECK(found[0] == 0 && found[1] == 2);
    CHECK(map.find_objects("coin", nullptr, 0) == 3);
    CHECK(map.find_objects("enemy", found, 2) == 1 && found[0] == 1);
    CHECK(map.find_objects("boss", found, 2) == 0 && map.find_objects(nullptr, found, 2) == 0);
}

NNH_TEST(object_properties_are_read_through_the_object) {
    Tilemap map = make({.width = 4, .height = 4});
    const int layer = map.add_object_layer("o").value_or(-1);
    const int coin = add(map, layer, {.name = "coin"});
    const int gem = add(map, layer, {.name = "gem"});
    CHECK(map.set_int(PropertyOwner::object(gem), "value", 50).ok());
    CHECK(map.set_int(PropertyOwner::object(coin), "value", 1).ok());
    CHECK(map.set_string(PropertyOwner::object(coin), "sound", "sfx/coin").ok());
    CHECK(map.object(coin).properties.get_int("value") == 1);
    CHECK(same(map.object(coin).properties.get_string("sound"), "sfx/coin"));
    CHECK(map.object(gem).properties.count() == 1 && map.object(gem).properties.get_int("value") == 50);
    CHECK(map.set_int(PropertyOwner::object(2), "value", 1).error().code == ErrorCode::InvalidArgument);
}

NNH_TEST(an_empty_map_has_no_objects) {
    Tilemap map;
    CHECK(map.object_layer_count() == 0 && map.object_count() == 0 && map.find_object("a") == -1);
    CHECK(map.add_object_layer("a").error().code == ErrorCode::InvalidArgument);
    CHECK(object_error(map, 0, {}) == ErrorCode::InvalidArgument);
    int found[1];
    CHECK(map.find_objects("", found, 1) == 0);
}
