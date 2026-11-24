// Map objects name prefabs (spec 021 T5.2, FR-9, FR-6, AC-3): the `prefab` string property first, else the type; map
// order; skipped types; unknown explicit names logged with the object's id, name and layer and asserting; the layer
// filter; the check call; a restart after clear() gives the first load's slots; the name memo (test_map_names.cpp: the
// walk's memos against find() object by object).

#include "map_fixture.hpp"
#include "nnh/app.hpp"
#include "prefabs/prefab_internal.hpp"

#include <cstring>

using nnh::Entity;
using nnh::MapSpawnSummary;
using nnh::ObjectShape;
using nnh::Prefab;
using nnh::PropertyOwner;
using nnh::Rect;
using nnh::Tilemap;
using nnh::World;
using namespace prefab_test;

namespace {

bool same(const MapSpawnSummary& a, const MapSpawnSummary& b) {
    return a.spawned == b.spawned && a.misses == b.misses && a.skipped == b.skipped && a.unknown == b.unknown &&
           a.first_unknown == b.first_unknown;
}

/// coin (rect, value 3), walker (point), door, coin (point), flying_walker by property (rect).
Tilemap level() {
    Tilemap map = code_map();
    const int a = add_object(map, "c1", "coin", {10, 20, 16, 8});
    CHECK(map.set_int(PropertyOwner::object(a), "value", 3).ok());
    add_object(map, "w1", "walker", {30, 40, 0, 0}, ObjectShape::Point);
    add_object(map, "d", "door", {0, 0, 16, 32});
    add_object(map, "c2", "coin", {50, 60, 0, 0}, ObjectShape::Point);
    const int f = add_object(map, "w2", "walker", {70, 80, 16, 16});
    CHECK(map.set_string(PropertyOwner::object(f), "prefab", "flying_walker").ok());
    return map;
}

} // namespace

NNH_TEST(map_objects_spawn_in_map_order_by_property_then_type) {
    MapPrefabs p;
    Tilemap map = level();
    World w = make_world();
    const MapSpawnSummary s = p.lib.spawn_map_objects(w, map);
    CHECK(same(s, {.spawned = 4, .misses = 0, .skipped = 1, .unknown = 0, .first_unknown = -1}));
    CHECK(w.count() == 4);
    const Entity c1 = Entity::from_bits(0x00010000u), w1 = Entity::from_bits(0x00010001u);
    const Entity c2 = Entity::from_bits(0x00010002u), w2 = Entity::from_bits(0x00010003u);
    CHECK(w.get<Coin>(c1)->value == 3 && w.get<Coin>(c2)->value == 1); // the hook read `value` (1 when absent)
    CHECK(w.has<Mover>(w1) && !w.has<Bob>(w1) && w.has<Bob>(w2) && w.has_tag(w2, kFlying));
    CHECK(p.log.calls == 2 && p.log.last_id == 4 && p.log.last_index == 3);
    CHECK(p.lib.stats().map_skipped == 1 && p.lib.stats().map_unknown == 0 && p.lib.stats().spawns == 4);
}

NNH_TEST(map_objects_are_placed_by_their_bounds) {
    MapPrefabs p;
    Tilemap map = level();
    World w = make_world();
    p.lib.spawn_map_objects(w, map);
    const Rect c1 = w.get<Body>(Entity::from_bits(0x00010000u))->box;
    CHECK(c1.x == 10 && c1.y == 20 && c1.w == 16 && c1.h == 8);   // a rectangle object: its size too
    const Rect c2 = w.get<Body>(Entity::from_bits(0x00010002u))->box;
    CHECK(c2.x == 50 && c2.y == 60 && c2.w == 12 && c2.h == 12);   // a point: the prefab's size
    const nnh::Vec2 w2 = w.get<Mover>(Entity::from_bits(0x00010003u))->at;
    CHECK(w2.x == 70 && w2.y == 80);                               // a Vec2 gets a rectangle's top-left
    CHECK(p.log.last_position.x == 50 && p.log.last_position.y == 60);
}

NNH_TEST(a_restart_after_clear_gives_the_first_loads_slots) {
    MapPrefabs p;
    Tilemap map = level();
    World w = make_world();
    p.lib.spawn_map_objects(w, map);
    Entity first[8];
    const int n = w.find(kEnemy, first, 8);
    w.clear();
    p.lib.spawn_map_objects(w, map);
    Entity again[8];
    CHECK(w.find(kEnemy, again, 8) == n && n == 2);
    for (int i = 0; i < n; ++i) CHECK(again[i].slot() == first[i].slot() && again[i].generation() == first[i].generation() + 1);
    CHECK(w.count() == 4);
}

NNH_TEST(the_layer_filter_spawns_one_object_layer) {
    MapPrefabs p;
    Tilemap map = load_prefab_map();
    World w = make_world();
    const MapSpawnSummary s = p.lib.spawn_map_objects(w, map, {.object_layer = 1});
    CHECK(same(s, {.spawned = 1}));
    const Body* b = w.get<Body>(Entity::from_bits(0x00010000u));
    CHECK(b != nullptr && b->box.x == 104 && b->box.w == 8 && w.get<Coin>(Entity::from_bits(0x00010000u))->value == 2);
    CHECK(p.log.last_id == 14 && p.log.last_index == 9);
}

NNH_TEST(check_map_objects_reports_without_spawning_logging_or_asserting) {
    MapPrefabs p;
    Tilemap map = load_prefab_map();
    const nnh::PrefabStats before = p.lib.stats();
    const MapSpawnSummary s = p.lib.check_map_objects(map);
    CHECK(same(s, {.spawned = 5, .misses = 0, .skipped = 3, .unknown = 2, .first_unknown = 7}));
    CHECK(same(p.lib.check_map_objects(map, {.object_layer = 0}),
               {.spawned = 4, .misses = 0, .skipped = 3, .unknown = 2, .first_unknown = 7}));
    CHECK(p.lib.stats().map_unknown == before.map_unknown && p.lib.stats().spawns == 0 && p.log.calls == 0);
}

namespace {
MapPrefabs* g_p = nullptr;
World* g_w = nullptr;
const Tilemap* g_map = nullptr;
MapSpawnSummary g_s{};
void spawn_the_fixture() { g_s = g_p->lib.spawn_map_objects(*g_w, *g_map); }
} // namespace

NNH_TEST(an_unknown_explicit_prefab_is_a_logged_level_error_that_asserts) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    {
        MapPrefabs p;
        Tilemap map = load_prefab_map();
        World w = make_world();
        g_p = &p;
        g_w = &w;
        g_map = &map;
        g_s = {};
        CHECK(asserts_or_is_ignored(spawn_the_fixture));
        const char* line = nnh::platform_fake::state().last_console_line;
        if (nnh::build::kAssertsEnabled) { // stopped at the first level error
            CHECK(std::strstr(line, "object 12 \"coin\" (layer things): no prefab \"coinn\"") != nullptr);
            CHECK(w.count() == 4 && p.lib.stats().map_unknown == 1 && p.lib.stats().map_skipped == 3);
        } else { // Release: both logged, the rest spawned
            CHECK(std::strstr(line, "object 13 \"w3\" (layer things): property \"prefab\" is not a string") != nullptr);
            CHECK(same(g_s, {.spawned = 5, .misses = 0, .skipped = 3, .unknown = 2, .first_unknown = 7}));
            CHECK(p.lib.stats().map_unknown == 2 && p.lib.stats().map_skipped == 3 && w.count() == 5);
        }
        world_test::recover(w);
        g_p = nullptr;
        g_w = nullptr;
        g_map = nullptr;
    }
    nnh::shutdown();
}

NNH_TEST(the_fixtures_objects_keep_their_properties_through_the_converter) {
    Tilemap map = load_prefab_map();
    CHECK(map.object_count() == 10 && map.object_layer_count() == 2);
    const nnh::MapObject w2 = map.object(3);
    CHECK(w2.id == 4 && std::strcmp(w2.type, "walker") == 0);
    CHECK(std::strcmp(w2.properties.get_string("prefab", ""), "flying_walker") == 0);
    CHECK(map.object(8).properties.type("prefab") == nnh::PropertyType::Int);
    CHECK(map.object(1).shape == ObjectShape::Point && map.object(0).properties.get_int("value") == 5);
}

NNH_TEST(map_misses_are_counted_when_a_kind_is_full) {
    MapPrefabs p;
    Tilemap map = level();
    const nnh::ComponentDesc kinds[] = {nnh::component<Body>(8), nnh::component<Coin>(1), nnh::component<Mover>(8),
                                        nnh::component<Bob>(8)};
    World w = static_cast<World&&>(World::create({.max_entities = 8, .tags_per_entity = 2}, kinds).value());
    const MapSpawnSummary s = p.lib.spawn_map_objects(w, map);
    CHECK(same(s, {.spawned = 3, .misses = 1, .skipped = 1}));
    CHECK(p.lib.stats().misses == 1);
}

NNH_TEST(the_name_memo_answers_equal_text_at_different_pointers) {
    MapPrefabs p;
    nnh::detail::NameMemo cache;
    const char a[] = "walker";
    const char b[] = "walker";
    const char c[] = "coin";
    CHECK(cache.find(p.lib, a) == p.walker && cache.find(p.lib, a) == p.walker);
    CHECK(cache.find(p.lib, b) == p.walker && cache.find(p.lib, c) == p.coin && cache.find(p.lib, a) == p.walker);
    CHECK(cache.find(p.lib, "door") == Prefab{});
}

NNH_TEST(map_spawns_inside_clear_are_refused_and_counted_as_nothing) {
    static MapPrefabs* prefabs = nullptr;
    static const Tilemap* level_map = nullptr;
    static MapSpawnSummary seen{};
    MapPrefabs p;
    Tilemap map = level();
    prefabs = &p;
    level_map = &map;
    const nnh::ComponentDesc kinds[] = {nnh::component<Body>(8), nnh::component<Coin>(8), nnh::component<Mover>(8),
                                        nnh::component<Bob>(8, [](World& w, Entity, Bob&) {
                                            if (w.clearing()) seen = prefabs->lib.spawn_map_objects(w, *level_map);
                                        })};
    World w = static_cast<World&&>(World::create({.max_entities = 8, .tags_per_entity = 2}, kinds).value());
    w.add<Bob>(w.spawn());
    w.clear();
    CHECK(same(seen, {.spawned = 0, .misses = 0, .skipped = 1}) && w.count() == 0 && p.lib.stats().misses == 0);
    w = World{};
}
