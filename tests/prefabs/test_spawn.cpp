// Spawning a prefab (spec 021 T4.1, FR-5, FR-12, FR-13; FR-1's several libraries and worlds): one entity, bit for bit
// what spawn() + add<T>(e, value) per entry + add_tag per tag make on a twin world, in the lowest free slots, with only
// the prefab's kinds and tags, and no event published.

#include "nnh/app.hpp"
#include "nnh/events.hpp"
#include "prefab_fixture.hpp"
#include "twin.hpp"

using nnh::Entity;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::World;
using namespace prefab_test;

namespace {

constexpr Body kBody{{1, 2, 3, 4}, 5};
constexpr Coin kCoin{7};

Big big() {
    Big b;
    for (int i = 0; i < 16; ++i) b.v[i] = i * 3 + 1;
    return b;
}

/// The hand-written twin of the "enemy" prefab below.
Entity spawn_enemy_by_hand(World& w) {
    const Entity e = w.spawn();
    w.add<Body>(e, kBody);
    w.add<Coin>(e, kCoin);
    w.add<Big>(e, big());
    w.add_tag(e, kEnemy);
    w.add_tag(e, kFx);
    return e;
}

struct Fixture {
    PrefabLibrary lib = make_library();
    Prefab enemy = must_add(lib, "enemy", {.tags = {kEnemy, kFx}}, kBody, kCoin, big());
    World a = make_world();
    World b = make_world();
};

} // namespace

NNH_TEST(a_prefab_spawn_equals_the_hand_written_calls) {
    Fixture f;
    const Entity x = f.lib.spawn(f.a, f.enemy);
    const Entity y = spawn_enemy_by_hand(f.b);
    CHECK(x && x == y && same_state(f.a, f.b));
    CHECK(f.a.get<Big>(x)->v[15] == 46 && f.a.get<Body>(x)->layers == 5 && f.a.has_tag(x, kFx));
    CHECK(f.lib.stats().spawns == 1);
}

NNH_TEST(prefab_spawns_take_the_lowest_free_slots_and_skip_quarantined_ones) {
    Fixture f;
    Entity xs[6], ys[6];
    for (int i = 0; i < 6; ++i) {
        xs[i] = f.lib.spawn(f.a, f.enemy);
        ys[i] = spawn_enemy_by_hand(f.b);
    }
    for (int i : {1, 3}) {
        f.a.destroy(xs[i]);
        f.b.destroy(ys[i]);
    }
    const Entity q = f.lib.spawn(f.a, f.enemy); // slots 1 and 3 are quarantined
    CHECK(q.slot() == 6 && q == spawn_enemy_by_hand(f.b) && same_state(f.a, f.b));
    f.a.flush();
    f.b.flush();
    const Entity r = f.lib.spawn(f.a, f.enemy); // the lowest free again: slot 1, generation 2
    CHECK(r.slot() == 1 && r.generation() == 2 && r == spawn_enemy_by_hand(f.b) && same_state(f.a, f.b));
}

NNH_TEST(a_spawned_entity_has_only_the_prefabs_kinds_and_tags) {
    Fixture f;
    const Entity e = f.lib.spawn(f.a, f.enemy);
    CHECK(f.a.has<Body>(e) && f.a.has<Coin>(e) && f.a.has<Big>(e));
    CHECK(!f.a.has<Mover>(e) && !f.a.has<Bob>(e) && !f.a.has<Counted>(e));
    nnh::Entity tagged[4];
    CHECK(f.a.find(kEnemy, tagged, 4) == 1 && f.a.find(kFx, tagged, 4) == 1 && f.a.find(kFlying, tagged, 4) == 0);
}

NNH_TEST(prefab_registration_and_spawns_publish_no_event) {
    nnh::shutdown(); // with the engine shut down, any publish would assert (spec 020's check)
    const nnh::events::Stats before = nnh::events::stats();
    {
        Fixture f;
        for (int i = 0; i < 10; ++i) f.lib.spawn(f.a, f.enemy);
        CHECK(f.a.count() == 10);
    }
    const nnh::events::Stats after = nnh::events::stats();
    CHECK(before.subscribers == after.subscribers && before.dropped_events == after.dropped_events);
}

NNH_TEST(a_prefab_spawned_inside_an_iteration_is_not_visited) {
    Fixture f;
    f.lib.spawn(f.a, f.enemy);
    spawn_enemy_by_hand(f.b);
    int visits_a = 0, visits_b = 0;
    f.a.each<Coin>([&](Entity, Coin&) {
        if (visits_a++ == 0) f.lib.spawn(f.a, f.enemy);
    });
    f.b.each<Coin>([&](Entity, Coin&) {
        if (visits_b++ == 0) spawn_enemy_by_hand(f.b);
    });
    CHECK(visits_a == 1 && visits_b == 1 && f.a.count() == 2 && same_state(f.a, f.b));
}

namespace {
PrefabLibrary* g_lib = nullptr;
Prefab g_prefab{};
} // namespace

NNH_TEST(a_prefab_spawns_from_a_worlds_remove_hook) {
    PrefabLibrary lib = make_library();
    g_lib = &lib;
    g_prefab = must_add(lib, "coin", {.tags = {kCoinTag}}, Coin{3});
    const nnh::ComponentDesc kinds[] = {nnh::component<Coin>(8), nnh::component<Bob>(8, [](World& w, Entity, Bob&) {
                                            if (!w.clearing()) g_lib->spawn(w, g_prefab);
                                        })};
    World w = static_cast<World&&>(World::create({.max_entities = 8}, kinds).value());
    const Entity e = w.spawn();
    w.add<Bob>(e);
    w.destroy(e);
    const Entity made = w.first(kCoinTag);
    CHECK(made.slot() == 1 && w.get<Coin>(made)->value == 3 && w.count() == 1);
    g_lib = nullptr;
}

NNH_TEST(several_libraries_spawn_into_one_world_and_one_library_into_two) {
    PrefabLibrary l1 = make_library();
    PrefabLibrary l2 = make_library();
    const Prefab c1 = must_add(l1, "coin", {}, Coin{1});
    const Prefab c2 = must_add(l2, "coin", {}, Coin{2});
    World w = make_world();
    World v = make_world();
    const Entity a = l1.spawn(w, c1);
    const Entity b = l2.spawn(w, c2);
    const Entity c = l1.spawn(v, c1);
    CHECK(a.slot() == 0 && b.slot() == 1 && c.slot() == 0);
    CHECK(w.get<Coin>(a)->value == 1 && w.get<Coin>(b)->value == 2 && v.get<Coin>(c)->value == 1);
    CHECK(l1.stats().spawns == 2 && l2.stats().spawns == 1);
}

NNH_TEST(a_counted_value_is_copied_once_per_spawn_and_balances) {
    Counted::reset();
    {
        PrefabLibrary lib = make_library();
        Counted c;
        c.id = 4;
        const Prefab p = must_add(lib, "counted", {}, c);
        World w = make_world();
        const int before = Counted::constructed;
        const Entity e = lib.spawn(w, p);
        CHECK(Counted::constructed == before + 1 && w.get<Counted>(e)->id == 4);
    }
    CHECK(Counted::alive() == 0);
}
