#include "bench.hpp"

#include <cstdio>

using nnh::Entity;
using nnh::World;

namespace {

constexpr nnh::Tag kT1 = nnh::tag("t1"), kT2 = nnh::tag("t2");

std::uint32_t mix(std::uint32_t i) {
    std::uint32_t h = i * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    return h ^ (h >> 13);
}

PrefabsBench::Big big() {
    PrefabsBench::Big b;
    for (int i = 0; i < 16; ++i) b.v[i] = i;
    return b;
}

const PrefabsBench::Body kBody{{0, 0, 8, 8}, 1};
const PrefabsBench::Big kBig = big();
const PrefabsBench::Velocity kVelocity{{0.5f, -0.25f}};

bool make_world(World& out, int entities, int tags) {
    const nnh::ComponentDesc kinds[] = {nnh::component<PrefabsBench::Body>(entities),
                                        nnh::component<PrefabsBench::Big>(entities),
                                        nnh::component<PrefabsBench::Velocity>(entities),
                                        nnh::component<PrefabsBench::Coin>(entities)};
    auto r = World::create({.max_entities = entities, .tags_per_entity = tags}, kinds);
    if (!r) return false;
    out = static_cast<World&&>(r.value());
    return true;
}

bool make_library(nnh::PrefabLibrary& out, int prefabs, int entries, int bytes) {
    auto r = nnh::PrefabLibrary::create({.max_prefabs = prefabs, .max_entries = entries, .value_bytes = bytes});
    if (!r) return false;
    out = static_cast<nnh::PrefabLibrary&&>(r.value());
    return true;
}

} // namespace

bool PrefabsBench::start() {
    if (!make_world(spawn_world_, kSpawns, 2) || !make_world(hand_world_, kSpawns, 2) ||
        !make_world(map_world_, kMapObjects, 2) || !make_library(library_, 1, 3, 128) ||
        !make_library(map_library_, 2, 4, 128) || !make_library(find_library_, kLibrary, 0, 0))
        return false;
    auto p = library_.add("thing", {.tags = {kT1, kT2}, .place = nnh::place(&Body::box)}, kBody, kBig, kVelocity);
    if (!p) return false;
    prefab_ = p.value();
    for (int i = 0; i < kLibrary; ++i) {
        std::snprintf(names_[i], sizeof names_[i], "enemy_%03d", i);
        if (!find_library_.add(names_[i], {.tags = {kT1}})) return false;
    }
    for (int i = 0; i < kFinds; ++i) order_[i] = static_cast<std::uint8_t>(mix(static_cast<std::uint32_t>(i)) % kLibrary);
    return make_map();
}

bool PrefabsBench::make_map() {
    const nnh::Placement box = nnh::place(&Body::box);
    const auto coin = map_library_.add(
        "coin",
        {.tags = {kT1}, .place = box, .on_spawn = [](World& w, Entity e, const nnh::SpawnInfo& info) {
             if (Coin* c = w.get<Coin>(e)) c->value = info.object->properties.get_int("value", 1);
         }},
        kBody, Coin{1});
    const auto walker = map_library_.add(
        "walker",
        {.tags = {kT2}, .place = box, .on_spawn = [](World& w, Entity e, const nnh::SpawnInfo& info) {
             if (Velocity* v = w.get<Velocity>(e)) v->v.x = static_cast<float>(info.object->properties.get_int("value", 1));
         }},
        kBody, kVelocity);
    auto made = nnh::Tilemap::create({.width = 64, .height = 32, .max_object_layers = 1, .max_objects = kMapObjects,
                                      .max_properties = kMapObjects, .string_bytes = 256});
    if (!coin || !walker || !made) return false;
    map_ = static_cast<nnh::Tilemap&&>(made.value());
    const auto layer = map_.add_object_layer("things");
    if (!layer) return false;
    for (int i = 0; i < kMapObjects; ++i) {
        const bool is_coin = i % 2 == 0;
        const nnh::Rect bounds{static_cast<float>((i % 64) * 16), static_cast<float>((i / 64) * 16), 12, 12};
        const auto o = map_.add_object(layer.value(), {.name = is_coin ? "c" : "w", .type = is_coin ? "coin" : "walker",
                                                       .shape = is_coin ? nnh::ObjectShape::Rect : nnh::ObjectShape::Point,
                                                       .bounds = bounds});
        if (!o || !map_.set_int(nnh::PropertyOwner::object(o.value()), "value", i % 7)) return false;
    }
    return true;
}

int PrefabsBench::run_spawn() {
    for (int i = 0; i < kSpawns; ++i) spawned_[i] = library_.spawn(spawn_world_, prefab_);
    for (const Entity& e : spawned_) spawn_world_.destroy(e);
    spawn_world_.flush();
    return spawned_[kSpawns - 1].generation();
}

int PrefabsBench::run_hand() {
    for (int i = 0; i < kSpawns; ++i) {
        const Entity e = hand_world_.spawn();
        hand_world_.add<Body>(e, kBody);
        hand_world_.add<Big>(e, kBig);
        hand_world_.add<Velocity>(e, kVelocity);
        hand_world_.add_tag(e, kT1);
        hand_world_.add_tag(e, kT2);
        spawned_[i] = e;
    }
    for (const Entity& e : spawned_) hand_world_.destroy(e);
    hand_world_.flush();
    return spawned_[kSpawns - 1].generation();
}

int PrefabsBench::run_overrides() {
    for (int i = 0; i < kSpawns; ++i) {
        const nnh::Vec2 at{static_cast<float>(i), static_cast<float>(i & 15)};
        spawned_[i] = library_.spawn_with(spawn_world_, prefab_, at, Velocity{{static_cast<float>(i & 3), 1}});
    }
    for (const Entity& e : spawned_) spawn_world_.destroy(e);
    spawn_world_.flush();
    return spawned_[kSpawns - 1].generation();
}

void PrefabsBench::reset_map() { map_world_.clear(); }

int PrefabsBench::run_map() { return map_library_.spawn_map_objects(map_world_, map_).spawned; }

int PrefabsBench::run_find() {
    int sum = 0;
    for (int i = 0; i < kFinds; ++i) sum += find_library_.find(names_[order_[i]]).slot();
    return sum;
}
