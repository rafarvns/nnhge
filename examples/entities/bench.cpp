#include "bench.hpp"

using arena::Body;
using arena::Coin;
using arena::Velocity;
using nnh::Entity;
using nnh::Tag;
using nnh::World;

namespace {

constexpr Tag kTags[8] = {nnh::tag("t0"), nnh::tag("t1"), nnh::tag("t2"), nnh::tag("t3"),
                          nnh::tag("t4"), nnh::tag("t5"), nnh::tag("t6"), nnh::tag("t7")};
constexpr Tag kSought = kTags[0];

std::uint32_t hash(std::uint32_t i) {
    std::uint32_t h = i * 2654435761u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

bool make(World& out, int entities, int tags, const nnh::ComponentDesc* kinds, int count) {
    auto r = World::create({.max_entities = entities, .tags_per_entity = tags}, kinds, count);
    if (!r) return false;
    out = static_cast<World&&>(r.value());
    return true;
}

} // namespace

bool EntitiesBench::start() {
    const nnh::ComponentDesc moving[] = {nnh::component<Body>(kEntities), nnh::component<Velocity>(kEntities)};
    const nnh::ComponentDesc spawner[] = {nnh::component<Body>(kSpawns), nnh::component<Velocity>(kSpawns),
                                          nnh::component<Coin>(kSpawns)};
    const nnh::ComponentDesc tagged[] = {nnh::component<Coin>(1)};
    if (!make(moving_, kEntities, 0, moving, 2) || !make(spawner_, kSpawns, 2, spawner, 3) ||
        !make(tagged_, kEntities, 4, tagged, 1))
        return false;
    for (int i = 0; i < kEntities; ++i) {
        const Entity e = moving_.spawn();
        const float x = static_cast<float>(i % 32) * 8;
        moving_.add<Body>(e, {{x, static_cast<float>(i / 32) * 8, 8, 8}, arena::bits::kEnemy});
        moving_.add<Velocity>(e, {{0.25f, -0.125f}});
        const Entity t = tagged_.spawn();
        for (int k = 0; k < 4; ++k) tagged_.add_tag(t, kTags[(hash(static_cast<std::uint32_t>(i)) >> (3 * k)) % 8]);
    }
    for (int i = 0; i < kLookups; ++i)
        handles_[i] = moving_.storage<Body>().owner(static_cast<int>(hash(static_cast<std::uint32_t>(i) + 77u) % kEntities));
    return true;
}

int EntitiesBench::run_each() {
    moving_.each<Body, Velocity>([](Entity, Body& b, Velocity& v) {
        b.box.x += v.v.x;
        b.box.y += v.v.y;
    });
    return static_cast<int>(moving_.storage<Body>().data()[0].box.x);
}

int EntitiesBench::run_spawns() {
    for (int i = 0; i < kSpawns; ++i) {
        const Entity e = spawner_.spawn();
        spawner_.add<Body>(e);
        spawner_.add<Velocity>(e);
        spawner_.add<Coin>(e);
        spawner_.add_tag(e, kTags[1]);
        spawner_.add_tag(e, kTags[2]);
        spawned_[i] = e;
    }
    for (const Entity& e : spawned_) spawner_.destroy(e);
    spawner_.flush();
    return spawned_[kSpawns - 1].generation();
}

int EntitiesBench::run_find() { return tagged_.find(kSought, found_, kEntities); }

int EntitiesBench::run_get() {
    std::uint32_t sum = 0;
    for (const Entity& h : handles_) {
        if (const Body* b = moving_.get<Body>(h)) sum += b->layers;
    }
    return static_cast<int>(sum);
}

int EntitiesBench::run_get_storage() {
    const nnh::Storage<Body> bodies = moving_.storage<Body>();
    std::uint32_t sum = 0;
    for (const Entity& h : handles_) {
        if (const Body* b = bodies.get(h)) sum += b->layers;
    }
    return static_cast<int>(sum);
}

int EntitiesBench::run_pool() {
    int sum = 0;
    for (int i = 0; i < kPairs; ++i) {
        const nnh::Acquired<Item> a = pool_.acquire();
        sum += a.handle.slot();
        pool_.release(a.handle);
    }
    pool_.flush();
    return sum;
}
