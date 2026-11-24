// How a map walk names prefabs (spec 021 FR-9, plan 021 "Optimisation round"): whatever the walk memoises — the
// `prefab` property among an object's properties, a type or an explicit name — it answers exactly what
// `properties.type("prefab")`, `get_string("prefab")` and `find()` answer object by object, also for many distinct names
// and for prefabs a hook registers during the walk.

#include "map_fixture.hpp"

#include <cstdio>

using nnh::Entity;
using nnh::MapSpawnSummary;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::PropertyOwner;
using nnh::PropertyType;
using nnh::Tilemap;
using nnh::World;
using namespace prefab_test;

namespace {

constexpr int kObjects = 120;

/// What the hooks saw: per spawn, the object index and which prefab (its slot).
struct Seen {
    int count = 0;
    int object[kObjects] = {};
    int prefab[kObjects] = {};
};

/// The summary the walk must give, computed object by object through the public queries (no memo).
MapSpawnSummary oracle(const PrefabLibrary& lib, const Tilemap& map, Seen* expected) {
    MapSpawnSummary s;
    for (int i = 0; i < map.object_count(); ++i) {
        const nnh::MapObject o = map.object(i);
        const PropertyType t = o.properties.type("prefab");
        Prefab p{};
        if (t != PropertyType::None) {
            if (t == PropertyType::String) p = lib.find(o.properties.get_string("prefab", ""));
            if (!p) {
                if (s.unknown++ == 0) s.first_unknown = i;
                continue;
            }
        } else {
            if (o.type[0] != '\0') p = lib.find(o.type);
            if (!p) {
                ++s.skipped;
                continue;
            }
        }
        ++s.spawned;
        if (expected != nullptr) {
            expected->object[expected->count] = i;
            expected->prefab[expected->count++] = p.slot();
        }
    }
    return s;
}

bool same(const MapSpawnSummary& a, const MapSpawnSummary& b) {
    return a.spawned == b.spawned && a.misses == b.misses && a.skipped == b.skipped && a.unknown == b.unknown &&
           a.first_unknown == b.first_unknown;
}

/// 24 types t0..t23 (the even ones registered), objects cycling through them; every fifth object also has properties
/// in a rotating order: `prefabs`, `pref`, `p`, `value`, and on every tenth a string `prefab` naming a registered type.
Tilemap many_names(PrefabLibrary& lib, Seen& seen) {
    Seen* log = &seen;
    for (int t = 0; t < 24; t += 2) {
        char name[8];
        std::snprintf(name, sizeof name, "t%d", t);
        const int slot = t / 2; // registration order
        const auto hook = [log, slot](World&, Entity, const nnh::SpawnInfo& info) {
            if (log->count >= kObjects) return;
            log->object[log->count] = info.object_index;
            log->prefab[log->count++] = slot;
        };
        must_add(lib, name, {.place = nnh::place(&Body::box), .on_spawn = hook}, Body{});
    }
    auto made = Tilemap::create({.width = 8, .height = 4, .max_object_layers = 1, .max_objects = kObjects,
                                 .max_properties = 512, .string_bytes = 2048});
    CHECK(made.ok());
    Tilemap map = std::move(made.value());
    CHECK(map.add_object_layer("things").ok());
    const char* names[] = {"prefabs", "pref", "p", "value"};
    for (int i = 0; i < kObjects; ++i) {
        char type[8];
        std::snprintf(type, sizeof type, "t%d", (i * 7) % 24);
        const int o = add_object(map, "o", type, {static_cast<float>(i), 0, 4, 4});
        if (i % 5 != 0) continue;
        for (int k = 0; k < 4; ++k) CHECK(map.set_int(PropertyOwner::object(o), names[(i / 5 + k) % 4], k).ok());
        if (i % 10 == 0) {
            char named[8];
            std::snprintf(named, sizeof named, "t%d", (i / 10 * 2) % 24);
            CHECK(map.set_string(PropertyOwner::object(o), "prefab", named).ok());
        }
    }
    return map;
}

} // namespace

NNH_TEST(a_map_walk_over_many_names_answers_as_find_does_object_by_object) {
    PrefabLibrary lib = make_library({.max_prefabs = 32, .max_entries = 32, .value_bytes = 1024});
    Seen seen;
    Tilemap map = many_names(lib, seen);
    Seen expected;
    const MapSpawnSummary want = oracle(lib, map, &expected);
    CHECK(want.spawned > 40 && want.skipped > 40 && want.unknown == 0);
    CHECK(same(lib.check_map_objects(map), want));
    World w = make_world(kObjects, kObjects, 2);
    CHECK(same(lib.spawn_map_objects(w, map), want));
    CHECK(seen.count == want.spawned);
    for (int i = 0; i < seen.count; ++i)
        CHECK(seen.object[i] == expected.object[i] && seen.prefab[i] == expected.prefab[i]);
}

NNH_TEST(a_non_string_or_unknown_prefab_property_is_unknown_among_other_properties) {
    PrefabLibrary lib = make_library();
    must_add(lib, "coin", {.place = nnh::place(&Body::box)}, Body{});
    Tilemap map = code_map();
    const int a = add_object(map, "a", "coin", {0, 0, 4, 4});
    CHECK(map.set_int(PropertyOwner::object(a), "value", 1).ok());
    CHECK(map.set_int(PropertyOwner::object(a), "prefab", 3).ok()); // not a string: unknown
    const int b = add_object(map, "b", "coin", {0, 0, 4, 4});
    CHECK(map.set_string(PropertyOwner::object(b), "prefabs", "nothing").ok()); // another name: the type spawns
    const int c = add_object(map, "c", "", {0, 0, 4, 4});
    CHECK(map.set_bool(PropertyOwner::object(c), "solid", true).ok());
    CHECK(map.set_string(PropertyOwner::object(c), "prefab", "coinn").ok()); // unknown
    add_object(map, "d", "coin", {0, 0, 4, 4});
    const MapSpawnSummary want = oracle(lib, map, nullptr);
    CHECK(same(want, {.spawned = 2, .misses = 0, .skipped = 0, .unknown = 2, .first_unknown = 0}));
    CHECK(same(lib.check_map_objects(map), want));
}

NNH_TEST(a_prefab_a_hook_registers_names_the_later_objects_of_its_type) {
    PrefabLibrary lib = make_library();
    PrefabLibrary* l = &lib;
    must_add(lib, "maker",
             {.place = nnh::place(&Body::box), .on_spawn = [l](World&, Entity, const nnh::SpawnInfo&) {
                  if (!l->find("late")) CHECK(l->add("late", {}, Coin{7}).ok());
              }},
             Body{});
    Tilemap map = code_map();
    add_object(map, "l1", "late", {0, 0, 4, 4});  // before: skipped
    add_object(map, "l2", "late", {0, 0, 4, 4});  // skipped (the same type again)
    add_object(map, "m", "maker", {0, 0, 4, 4});  // registers `late`
    add_object(map, "l3", "late", {0, 0, 4, 4});  // spawned
    const int named = add_object(map, "n", "", {0, 0, 4, 4});
    CHECK(map.set_string(PropertyOwner::object(named), "prefab", "late").ok()); // spawned
    World w = make_world();
    const MapSpawnSummary s = lib.spawn_map_objects(w, map);
    CHECK(same(s, {.spawned = 3, .misses = 0, .skipped = 2, .unknown = 0, .first_unknown = -1}));
    CHECK(w.count() == 3 && w.get<Coin>(Entity::from_bits(0x00010001u))->value == 7);
}
