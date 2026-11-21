// Properties of the map, its layers and its tiles (spec 012 FR-0, FR-6). Objects: test_map_objects.cpp.

#include <cstring>

#include "map_fixture.hpp"
#include "tilemap/tilemap_internal.hpp"

using map_test::make;
using nnh::ErrorCode;
using nnh::PropertyOwner;
using nnh::PropertyType;
using nnh::Tilemap;

namespace {

ErrorCode code(const nnh::Result<void>& r) { return r ? ErrorCode::None : r.error().code; }
bool same(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
std::uint32_t string_bytes(const Tilemap& map) { return nnh::detail::map_view(map).header().string_bytes; }

/// A map with two tile layers, one object layer and an 8-tile tileset.
struct Level {
    map_test::Engine engine;
    Tilemap map = make({.width = 4, .height = 4});
    int first = map_test::tileset(map, {.texture = &engine.sheet});
    int back = map_test::layer(map, {.name = "back"});
    int front = map_test::layer(map, {.name = "front"});
    int objects = map.add_object_layer("things").value_or(-1);
};

} // namespace

NNH_TEST(map_properties_of_each_type_are_read_back) {
    Tilemap map = make({.width = 2, .height = 2});
    CHECK(code(map.set_bool({}, "boss", true)) == ErrorCode::None);
    CHECK(code(map.set_int({}, "lives", -3)) == ErrorCode::None);
    CHECK(code(map.set_float({}, "gravity", 9.5f)) == ErrorCode::None);
    CHECK(code(map.set_string({}, "music", "music/level1")) == ErrorCode::None);
    const nnh::Properties p = map.properties();
    CHECK(p.count() == 4);
    CHECK(p.get_bool("boss") && p.get_int("lives") == -3 && p.get_float("gravity") == 9.5f);
    CHECK(same(p.get_string("music"), "music/level1"));
    CHECK(p.type("boss") == PropertyType::Bool && p.type("lives") == PropertyType::Int);
    CHECK(p.type("gravity") == PropertyType::Float && p.type("music") == PropertyType::String);
    CHECK(p.has("music") && !p.has("nope") && p.type("nope") == PropertyType::None);
    CHECK(same(p.name(0), "boss") && same(p.name(3), "music") && same(p.name(4), "") && same(p.name(-1), ""));
}

NNH_TEST(missing_properties_give_the_fallback) {
    Tilemap map = make({.width = 2, .height = 2});
    const nnh::Properties p = map.properties();
    CHECK(!p.get_bool("x", false) && p.get_bool("x", true));
    CHECK(p.get_int("x", 7) == 7 && p.get_float("x", 1.5f) == 1.5f && same(p.get_string("x", "fb"), "fb"));
    CHECK(p.get_int(nullptr, 4) == 4 && !p.has(nullptr));
}

NNH_TEST(properties_of_another_type_give_the_fallback_except_int_as_float) {
    Tilemap map = make({.width = 2, .height = 2});
    (void)map.set_int({}, "n", 3);
    (void)map.set_float({}, "f", 2.5f);
    (void)map.set_string({}, "s", "7");
    (void)map.set_bool({}, "b", true);
    const nnh::Properties p = map.properties();
    CHECK(p.get_float("n") == 3.0f);                // ints convert
    CHECK(p.get_int("f", -1) == -1);                // no silent truncation
    CHECK(p.get_int("s", -1) == -1 && p.get_int("b", -1) == -1);
    CHECK(!p.get_bool("n") && same(p.get_string("n", "fb"), "fb") && p.get_float("s", 0.5f) == 0.5f);
}

NNH_TEST(setting_an_existing_property_replaces_its_value_and_type) {
    Tilemap map = make({.width = 2, .height = 2});
    (void)map.set_int({}, "score", 1);
    (void)map.set_int({}, "score", 2);
    CHECK(map.properties().count() == 1 && map.properties().get_int("score") == 2);
    (void)map.set_string({}, "score", "high");
    CHECK(map.properties().count() == 1 && map.properties().type("score") == PropertyType::String);
    CHECK(same(map.properties().get_string("score"), "high"));
}

NNH_TEST(each_owner_has_its_own_properties) {
    Level l;
    (void)l.map.set_int(PropertyOwner::map(), "n", 1);
    (void)l.map.set_int(PropertyOwner::tile_layer(l.back), "n", 2);
    (void)l.map.set_int(PropertyOwner::tile_layer(l.front), "n", 3);
    (void)l.map.set_int(PropertyOwner::object_layer(l.objects), "n", 4);
    (void)l.map.set_int(PropertyOwner::tile(l.first + 2), "n", 5);
    CHECK(l.map.properties().get_int("n") == 1);
    CHECK(l.map.properties(PropertyOwner::tile_layer(l.back)).get_int("n") == 2);
    CHECK(l.map.properties(PropertyOwner::tile_layer(l.front)).get_int("n") == 3);
    CHECK(l.map.properties(PropertyOwner::object_layer(l.objects)).get_int("n") == 4);
    CHECK(l.map.properties(PropertyOwner::tile(l.first + 2)).get_int("n") == 5);
    CHECK(l.map.properties(PropertyOwner::tile(l.first)).count() == 0);
}

NNH_TEST(properties_added_in_any_order_stay_with_their_owner) {
    Level l;
    const PropertyOwner back = PropertyOwner::tile_layer(l.back), front = PropertyOwner::tile_layer(l.front);
    const PropertyOwner t3 = PropertyOwner::tile(3), t1 = PropertyOwner::tile(1);
    (void)l.map.set_int(front, "a", 10);
    (void)l.map.set_int(back, "a", 20);
    (void)l.map.set_int(t3, "a", 30);
    (void)l.map.set_int(front, "b", 11); // inserted inside: later ranges move
    (void)l.map.set_int({}, "a", 40);
    (void)l.map.set_int(t1, "a", 50);  // a tile before tile 3 in the sparse table
    (void)l.map.set_int(back, "b", 21);
    (void)l.map.set_int(t3, "b", 31);
    CHECK(l.map.properties(front).count() == 2 && l.map.properties(front).get_int("a") == 10);
    CHECK(l.map.properties(front).get_int("b") == 11);
    CHECK(l.map.properties(back).count() == 2 && l.map.properties(back).get_int("a") == 20);
    CHECK(l.map.properties(back).get_int("b") == 21);
    CHECK(l.map.properties(t3).count() == 2 && l.map.properties(t3).get_int("a") == 30);
    CHECK(l.map.properties(t3).get_int("b") == 31);
    CHECK(l.map.properties(t1).count() == 1 && l.map.properties(t1).get_int("a") == 50);
    CHECK(l.map.properties().count() == 1 && l.map.properties().get_int("a") == 40);
}

NNH_TEST(tile_properties_are_kept_in_a_sparse_table_by_tile_id) {
    Level l;
    (void)l.map.set_bool(PropertyOwner::tile(6), "x", true);
    (void)l.map.set_bool(PropertyOwner::tile(2), "x", true);
    (void)l.map.set_bool(PropertyOwner::tile(4), "x", true);
    const nnh::detail::MapView v = nnh::detail::map_view(l.map);
    CHECK(v.header().tile_props == 3);
    CHECK(v.tile_props()[0].tile_id == 2 && v.tile_props()[1].tile_id == 4 && v.tile_props()[2].tile_id == 6);
    CHECK((v.tile_flags()[4] & nnh::detail::mapfmt::kTileHasProperties) != 0);
    CHECK((v.tile_flags()[5] & nnh::detail::mapfmt::kTileHasProperties) == 0);
}

NNH_TEST(properties_beyond_the_capacity_are_refused) {
    map_test::Engine engine;
    Tilemap map = make({.width = 2, .height = 2, .max_properties = 2});
    map_test::tileset(map, {.texture = &engine.sheet});
    CHECK(code(map.set_int({}, "a", 1)) == ErrorCode::None);
    CHECK(code(map.set_int(PropertyOwner::tile(1), "a", 1)) == ErrorCode::None);
    CHECK(code(map.set_int({}, "b", 1)) == ErrorCode::LimitReached);
    CHECK(code(map.set_int(PropertyOwner::tile(2), "a", 1)) == ErrorCode::LimitReached);
    CHECK(code(map.set_int({}, "a", 5)) == ErrorCode::None); // replacing needs no room
    CHECK(map.properties().count() == 1 && map.properties().get_int("a") == 5);
    CHECK(map.properties(PropertyOwner::tile(2)).count() == 0);
}

NNH_TEST(tiles_with_properties_are_limited_by_the_sparse_table) {
    map_test::Engine engine;
    Tilemap map = make({.width = 2, .height = 2, .max_tile_ids = 8, .max_properties = 64});
    map_test::tileset(map, {.texture = &engine.sheet});
    for (int id = 1; id <= 8; ++id) CHECK(code(map.set_int(PropertyOwner::tile(id), "a", id)) == ErrorCode::None);
    for (int id = 1; id <= 8; ++id) CHECK(map.properties(PropertyOwner::tile(id)).get_int("a") == id);
}

NNH_TEST(a_full_string_pool_refuses_new_names_and_values) {
    Tilemap map = make({.width = 2, .height = 2, .string_bytes = 6});
    CHECK(code(map.set_string({}, "ab", "cd")) == ErrorCode::LimitReached); // "" + "ab" + "cd" = 7 bytes > 6
    CHECK(map.properties().count() == 0);
    CHECK(code(map.set_string({}, "ab", "ab")) == ErrorCode::None); // the value shares the name's string
    CHECK(string_bytes(map) == 4);
    CHECK(code(map.set_int({}, "xy", 1)) == ErrorCode::LimitReached);
    CHECK(map.properties().count() == 1 && same(map.properties().get_string("ab"), "ab"));
}

NNH_TEST(equal_strings_are_stored_once) {
    Tilemap map = make({.width = 2, .height = 2});
    (void)map.set_string({}, "music", "theme");
    const std::uint32_t used = string_bytes(map);
    (void)map.set_string({}, "music", "theme");
    (void)map.set_string({}, "next", "theme");
    CHECK(string_bytes(map) == used + 5); // only "next"
}

NNH_TEST(bad_owners_and_names_are_invalid) {
    Level l;
    CHECK(code(l.map.set_int(PropertyOwner::tile_layer(2), "a", 1)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_int(PropertyOwner::tile_layer(-1), "a", 1)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_int(PropertyOwner::object_layer(1), "a", 1)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_int(PropertyOwner::tile(0), "a", 1)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_int(PropertyOwner::tile(9), "a", 1)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_int(PropertyOwner::object(0), "a", 1)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_int({}, "", 1)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_int({}, nullptr, 1)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_string({}, "a", nullptr)) == ErrorCode::InvalidArgument);
    char long_text[300];
    std::memset(long_text, 'z', sizeof long_text - 1);
    long_text[299] = '\0';
    CHECK(code(l.map.set_string({}, "a", long_text)) == ErrorCode::InvalidArgument);
    CHECK(code(l.map.set_int({}, long_text, 1)) == ErrorCode::InvalidArgument);
    CHECK(l.map.properties().count() == 0);
    CHECK(l.map.properties(PropertyOwner::tile_layer(5)).count() == 0);
    CHECK(l.map.properties(PropertyOwner::tile(99)).count() == 0);
}

NNH_TEST(an_empty_map_has_no_properties) {
    Tilemap map;
    CHECK(map.properties().count() == 0 && !map.properties().has("a"));
    CHECK(code(map.set_int({}, "a", 1)) == ErrorCode::InvalidArgument);
    const nnh::Properties none{};
    CHECK(none.count() == 0 && none.get_int("a", 3) == 3 && same(none.name(0), ""));
}
