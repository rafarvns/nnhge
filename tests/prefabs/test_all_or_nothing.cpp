// All or nothing (spec 021 T4.5, FR-8, FR-11): a full world, a full kind (first, middle, override-only) or a kind the
// world doesn't store leaves no entity; the library and the world count a miss; WhenFull::Assert asserts (and logs the
// prefab's name), WhenFull::Miss is quiet; an unregistered kind always asserts; inside clear() a spawn is refused
// without a miss or an assert.

#include "nnh/app.hpp"
#include "prefab_fixture.hpp"
#include "twin.hpp"

#include <cstring>

using nnh::Entity;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::WhenFull;
using nnh::World;
using namespace prefab_test;

namespace {

/// Body 8, Coin 2, Bob 4 slots; 8 entities.
World small_world() {
    const nnh::ComponentDesc kinds[] = {nnh::component<Body>(8), nnh::component<Coin>(2), nnh::component<Bob>(4)};
    return static_cast<World&&>(World::create({.max_entities = 8, .tags_per_entity = 2}, kinds).value());
}

struct Fixture {
    PrefabLibrary lib;
    Prefab p{};
    World a = small_world();
    World b = small_world(); ///< what `a` must still equal after a refusal (its counters adjusted by hand)
    explicit Fixture(WhenFull when) {
        lib = make_library({.max_prefabs = 4, .max_entries = 16, .value_bytes = 256, .when_full = when});
        p = must_add(lib, "walker", {.tags = {kEnemy}}, Body{}, Coin{1}, Bob{});
    }
    void fill_coins() { // both coins taken by other entities
        for (World* w : {&a, &b})
            for (int i = 0; i < 2; ++i) w->add<Coin>(w->spawn());
    }
};

Fixture* g_f = nullptr;
Entity g_e{};
void spawn_one() { g_e = g_f->lib.spawn(g_f->a, g_f->p); }
void spawn_with_a_bob() { g_e = g_f->lib.spawn_with(g_f->a, g_f->p, Bob{1}); }

} // namespace

NNH_TEST(a_full_kind_leaves_no_entity_and_counts_a_quiet_miss) {
    Fixture f(WhenFull::Miss);
    f.fill_coins();
    CHECK(!f.lib.spawn(f.a, f.p));
    CHECK(f.lib.stats().misses == 1 && f.lib.stats().spawns == 0);
    CHECK(f.a.storage<Coin>().stats().misses == 1 && f.a.stats().spawn_misses == 0 && f.a.count() == 2);
    ++nnh::WorldAccess::hot(f.b)->kinds[1].slots.misses;
    CHECK(same_state(f.a, f.b)); // no body, no bob, no entity: nothing but the counter
}

NNH_TEST(a_full_world_leaves_no_entity_and_counts_a_spawn_miss) {
    Fixture f(WhenFull::Miss);
    for (World* w : {&f.a, &f.b})
        for (int i = 0; i < 8; ++i) w->spawn();
    CHECK(!f.lib.spawn(f.a, f.p));
    CHECK(f.lib.stats().misses == 1 && f.a.stats().spawn_misses == 1);
    ++nnh::WorldAccess::hot(f.b)->entities.misses;
    CHECK(same_state(f.a, f.b));
}

NNH_TEST(the_first_full_kind_in_entry_order_is_the_one_counted) {
    Fixture f(WhenFull::Miss);
    for (World* w : {&f.a, &f.b})
        for (int i = 0; i < 4; ++i) w->add<Bob>(w->spawn()); // Bob (the last entry) full
    CHECK(!f.lib.spawn(f.a, f.p));
    CHECK(f.a.storage<Bob>().stats().misses == 1 && f.a.storage<Coin>().stats().misses == 0);
    f.fill_coins(); // now Coin (the middle entry) is full too: it is met first
    CHECK(!f.lib.spawn(f.a, f.p));
    CHECK(f.a.storage<Coin>().stats().misses == 1 && f.a.storage<Bob>().stats().misses == 1);
    CHECK(f.lib.stats().misses == 2);
}

NNH_TEST(a_full_override_only_kind_leaves_no_entity) {
    Fixture f(WhenFull::Miss);
    PrefabLibrary lib = make_library({.max_prefabs = 2, .max_entries = 4, .value_bytes = 64,
                                      .when_full = WhenFull::Miss});
    const Prefab body = must_add(lib, "body", {}, Body{});
    for (int i = 0; i < 4; ++i) f.a.add<Bob>(f.a.spawn());
    const int before = f.a.count();
    CHECK(!lib.spawn_with(f.a, body, Bob{2}));
    CHECK(f.a.count() == before && f.a.storage<Body>().count() == 0 && f.a.storage<Bob>().stats().misses == 1);
    CHECK(lib.spawn(f.a, body)); // without the override it fits
}

NNH_TEST(when_full_assert_logs_the_prefab_and_asserts) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
    {
        Fixture f(WhenFull::Assert);
        f.fill_coins();
        g_f = &f;
        CHECK(asserts_or_is_ignored(spawn_one));
        CHECK(!g_e && f.lib.stats().misses == 1 && f.a.count() == 2);
        const char* line = nnh::platform_fake::state().last_console_line;
        CHECK(std::strstr(line, "kind") != nullptr && std::strstr(line, "full") != nullptr);
        if (nnh::build::kDevToolsEnabled) CHECK(std::strstr(line, "\"walker\"") != nullptr);
        g_f = nullptr;
    }
    nnh::shutdown();
}

NNH_TEST(a_kind_the_world_does_not_store_always_asserts_and_counts_a_miss) {
    Fixture f(WhenFull::Miss);
    g_f = &f;
    PrefabLibrary lib = make_library({.max_prefabs = 2, .max_entries = 4, .value_bytes = 64,
                                      .when_full = WhenFull::Miss});
    f.lib = static_cast<PrefabLibrary&&>(lib);
    f.p = must_add(f.lib, "mover", {}, Body{}, Mover{}); // the small world has no Mover
    CHECK(asserts_or_is_ignored(spawn_one));
    CHECK(!g_e && f.a.count() == 0 && f.a.storage<Body>().count() == 0);
    f.p = must_add(f.lib, "plain", {}, Body{});
    spawn_with_a_bob(); // Bob is stored: fine
    CHECK(g_e && f.a.has<Bob>(g_e) && f.lib.stats().misses == 1);
    g_f = nullptr;
}

NNH_TEST(a_spawn_inside_clear_is_refused_without_a_miss_or_an_assert) {
    static PrefabLibrary* lib = nullptr;
    static Prefab p{};
    static Entity made{};
    PrefabLibrary l = make_library();
    lib = &l;
    p = must_add(l, "coin", {}, Coin{1});
    const nnh::ComponentDesc kinds[] = {nnh::component<Coin>(4), nnh::component<Bob>(4, [](World& w, Entity, Bob&) {
                                            made = lib->spawn(w, p); // the clear's hook: refused
                                        })};
    World w = static_cast<World&&>(World::create({.max_entities = 4}, kinds).value());
    w.add<Bob>(w.spawn());
    made = Entity::from_bits(1);
    w.clear();
    CHECK(!made && w.count() == 0 && l.stats().misses == 0 && l.stats().spawns == 0);
    CHECK(w.stats().spawn_misses == 0);
    w = World{};
    lib = nullptr;
}
