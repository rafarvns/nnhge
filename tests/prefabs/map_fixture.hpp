#pragma once
// Maps for the prefab map tests (spec 021 T5.2): the converted fixture tests/data/maps/good_prefabs.tmj, a map built in
// code, and a library whose prefabs those maps name (coin, walker, flying_walker).

#include <cstdio>
#include <string>

#include "nnh/tilemap.hpp"
#include "prefab_fixture.hpp"
#include "tilemap/tilemap_internal.hpp"

#ifndef NNH_TEST_MAPS_DIR
#error "NNH_TEST_MAPS_DIR is set by tests/CMakeLists.txt"
#endif

namespace prefab_test {

/// tests/data/maps/good_prefabs.tmj, converted by the build.
inline nnh::Tilemap load_prefab_map() {
    std::string bytes;
    std::FILE* f = std::fopen(NNH_TEST_MAPS_DIR "/good_prefabs.nnhmap", "rb");
    CHECK(f != nullptr);
    if (f == nullptr) return {};
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) bytes.append(buffer, got);
    std::fclose(f);
    auto made = nnh::detail::tilemap_from_file(bytes.data(), bytes.size(), {}, "good_prefabs");
    CHECK(made.ok());
    return made ? std::move(made.value()) : nnh::Tilemap{};
}

/// An empty map with one object layer "things" (index 0) and room for `objects`.
inline nnh::Tilemap code_map(int objects = 32) {
    auto made = nnh::Tilemap::create({.width = 8, .height = 4, .max_object_layers = 2, .max_objects = objects,
                                      .max_properties = 64, .string_bytes = 2048});
    CHECK(made.ok());
    nnh::Tilemap map = std::move(made.value());
    CHECK(map.add_object_layer("things").ok());
    return map;
}

inline int add_object(nnh::Tilemap& map, const char* name, const char* type, nnh::Rect bounds,
                      nnh::ObjectShape shape = nnh::ObjectShape::Rect) {
    auto r = map.add_object(0, {.name = name, .type = type, .shape = shape, .bounds = bounds});
    CHECK(r.ok());
    return r ? r.value() : -1;
}

/// What the map prefabs' hooks saw.
struct MapLog {
    int calls = 0;
    int last_id = -1;
    int last_index = -1;
    nnh::Vec2 last_position{};
};

/// coin (Body placed by its box, a rectangle object's size; Coin, its hook reads `value`; tag coin), walker (Mover
/// placed by `at`; tag enemy), flying_walker (walker + Bob, tag flying).
struct MapPrefabs {
    nnh::PrefabLibrary lib;
    MapLog log;
    nnh::Prefab coin{}, walker{}, flying{};
    explicit MapPrefabs(nnh::WhenFull when = nnh::WhenFull::Miss) {
        lib = make_library({.max_prefabs = 8, .max_entries = 16, .value_bytes = 256, .when_full = when});
        MapLog* l = &log;
        coin = must_add(lib, "coin",
                        {.tags = {kCoinTag},
                         .place = nnh::place(&Body::box),
                         .on_spawn =
                             [l](nnh::World& w, nnh::Entity e, const nnh::SpawnInfo& info) {
                                 ++l->calls;
                                 l->last_position = info.position;
                                 l->last_index = info.object_index;
                                 if (info.object == nullptr) return;
                                 l->last_id = info.object->id;
                                 w.get<Coin>(e)->value = info.object->properties.get_int("value", 1);
                             }},
                        Body{{0, 0, 12, 12}, 4}, Coin{1});
        walker = must_add(lib, "walker", {.tags = {kEnemy}, .place = nnh::place(&Mover::at)}, Mover{{}, {1, 0}});
        flying = must_add(lib, "flying_walker", {.base = walker, .tags = {kFlying}}, Bob{1});
    }
};

} // namespace prefab_test
