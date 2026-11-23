// Destroy and remove hooks (spec 020 FR-13; T3.1): hooks in registration order with the data intact, then
// components T{} and quarantined, tags gone, handle stale; destroys from hooks queued FIFO (each entity once, no
// recursion: a 1000-long chain), spawns from hooks, remove<T>() and its hook, adds to a dying entity refused.

#include "world_fixture.hpp"

using nnh::ComponentDesc;
using nnh::Entity;
using nnh::World;
using world_test::Hp;
using world_test::Pos;
using world_test::Vel;

namespace {

struct Log {
    char text[256] = {};
    int n = 0;
    void add(char c) {
        if (n < 255) text[n++] = c;
    }
    bool is(const char* s) const {
        int i = 0;
        for (; s[i] != 0; ++i)
            if (i >= n || text[i] != s[i]) return false;
        return i == n;
    }
};

Log g_log;
World* g_world = nullptr;

struct Link {
    Entity next{};
};

World hooked_world(int entities = 16) {
    // Hooks log a letter per kind and read the other kinds (data intact during step 1).
    const ComponentDesc kinds[] = {
        nnh::component<Pos>(entities, [](World& w, Entity e, Pos& p) {
            g_log.add('P');
            if (const Vel* v = w.get<Vel>(e)) g_log.add(static_cast<char>('0' + v->dx));
            (void)p;
        }),
        nnh::component<Vel>(entities, [](World& w, Entity e, Vel&) {
            g_log.add('V');
            if (w.alive(e) && w.get<Pos>(e) != nullptr) g_log.add('+');
        }),
        nnh::component<Link>(entities, [](World& w, Entity, Link& l) {
            g_log.add('L');
            w.destroy(l.next); // a chain: queued, processed after
        }),
    };
    auto r = World::create({.max_entities = entities, .tags_per_entity = 2}, kinds);
    CHECK(r.ok());
    g_log = {};
    return static_cast<World&&>(r.value());
}

} // namespace

NNH_TEST(destroy_runs_hooks_in_registration_order_with_the_data_intact) {
    World w = hooked_world();
    const Entity e = w.spawn();
    w.add<Vel>(e, {3, 0}); // added first, registered second: order is by registration
    w.add<Pos>(e);
    CHECK(w.destroy(e));
    CHECK(g_log.is("P3V+"));
}

NNH_TEST(destroy_then_leaves_t_in_the_slots_no_tags_and_a_stale_handle) {
    World w = hooked_world();
    const Entity e = w.spawn();
    Pos* p = w.add<Pos>(e, {4, 4});
    w.add_tag(e, nnh::tag("enemy"));
    w.destroy(e);
    CHECK(p->x == 0 && p->y == 0);
    CHECK(!w.alive(e) && !w.has_tag(e, nnh::tag("enemy")));
    CHECK(w.find(nnh::tag("enemy"), nullptr, 0) == 0);
    CHECK(w.storage<Pos>().count() == 0 && !w.storage<Pos>().is_live(0));
}

NNH_TEST(hooks_that_destroy_others_are_queued_fifo_and_processed_once) {
    World w = hooked_world();
    Entity es[4];
    for (Entity& e : es) e = w.spawn();
    w.add<Link>(es[0], {es[1]});
    w.add<Link>(es[1], {es[2]});
    w.add<Link>(es[2], {es[0]}); // back to the first: already dying, refused
    w.add<Pos>(es[3]);
    w.add<Link>(es[3], {es[1]}); // es[1] destroyed twice: the second is refused
    w.destroy(es[3]);
    CHECK(g_log.is("PLLLL"));
    for (const Entity& e : es) CHECK(!w.alive(e));
}

NNH_TEST(a_1000_long_hook_chain_does_not_recurse) {
    World w = hooked_world(1001);
    Entity es[1001];
    for (Entity& e : es) e = w.spawn();
    for (int i = 0; i < 1000; ++i) w.add<Link>(es[i], {es[i + 1]});
    w.destroy(es[0]);
    CHECK(w.count() == 0);
    CHECK(g_log.n == 255); // the log is full: 1000 hooks ran
}

NNH_TEST(inside_a_hook_a_queued_entity_stays_alive_until_the_outermost_call_returns) {
    static bool seen_alive = false;
    static Entity other{};
    const ComponentDesc kinds[] = {nnh::component<Hp>(4, [](World& w, Entity, Hp&) {
        w.destroy(other);
        seen_alive = w.alive(other) && w.get<Hp>(other) != nullptr;
    })};
    auto r = World::create({.max_entities = 4}, kinds);
    World& w = r.value();
    const Entity a = w.spawn();
    other = w.spawn();
    w.add<Hp>(a);
    w.add<Hp>(other);
    w.destroy(a);
    CHECK(seen_alive);
    CHECK(!w.alive(other));
}

NNH_TEST(hooks_may_spawn_entities) {
    static Entity spawned{};
    const ComponentDesc kinds[] = {nnh::component<Hp>(4, [](World& w, Entity, Hp&) {
        spawned = w.spawn();
        w.add<Hp>(spawned, {77});
    })};
    auto r = World::create({.max_entities = 4}, kinds);
    World& w = r.value();
    const Entity a = w.spawn();
    w.add<Hp>(a);
    w.destroy(a);
    CHECK(w.alive(spawned) && w.get<Hp>(spawned)->value == 77);
    CHECK(spawned.slot() == 1); // slot 0 is quarantined
}

NNH_TEST(remove_runs_the_kinds_hook_with_the_component_readable) {
    World w = hooked_world();
    const Entity e = w.spawn();
    w.add<Pos>(e);
    w.add<Vel>(e, {5, 0});
    CHECK(w.remove<Vel>(e));
    CHECK(g_log.is("V+"));
    CHECK(w.alive(e) && w.has<Pos>(e) && !w.has<Vel>(e));
}

NNH_TEST(remove_from_its_own_hook_returns_false) {
    static bool again = true;
    const ComponentDesc kinds[] = {
        nnh::component<Hp>(4, [](World& w, Entity e, Hp&) { again = w.remove<Hp>(e); })};
    auto r = World::create({.max_entities = 4}, kinds);
    World& w = r.value();
    const Entity e = w.spawn();
    w.add<Hp>(e);
    CHECK(w.remove<Hp>(e));
    CHECK(!again && !w.has<Hp>(e));
}

NNH_TEST(destroying_the_entity_from_a_remove_hook_is_processed_after_the_remove) {
    static int order = 0, hp_seen = 0, pos_seen = 0;
    const ComponentDesc kinds[] = {
        nnh::component<Hp>(4, [](World& w, Entity e, Hp&) {
            hp_seen = ++order;
            w.destroy(e); // queued: the remove finishes first
        }),
        nnh::component<Pos>(4, [](World&, Entity, Pos&) { pos_seen = ++order; }),
    };
    auto r = World::create({.max_entities = 4}, kinds);
    World& w = r.value();
    const Entity e = w.spawn();
    w.add<Hp>(e);
    w.add<Pos>(e);
    CHECK(w.remove<Hp>(e));
    CHECK(hp_seen == 1 && pos_seen == 2);
    CHECK(!w.alive(e));
}

NNH_TEST(remove_and_destroy_of_a_dying_entity_return_false) {
    static bool removed = true, destroyed = true;
    const ComponentDesc kinds[] = {nnh::component<Hp>(4, [](World& w, Entity e, Hp&) {
                                       removed = w.remove<Pos>(e);
                                       destroyed = w.destroy(e);
                                   }),
                                   nnh::component<Pos>(4)};
    auto r = World::create({.max_entities = 4}, kinds);
    World& w = r.value();
    const Entity e = w.spawn();
    w.add<Hp>(e);
    w.add<Pos>(e);
    w.destroy(e);
    CHECK(!removed && !destroyed);
}

namespace {

void add_to_a_dying_entity() {
    // The world lives outside: an assert jumps out of this function (its locals are not destroyed).
    World& w = *g_world;
    const Entity e = w.spawn();
    w.add<Hp>(e);
    w.destroy(e);
}

} // namespace

NNH_TEST(adding_to_a_dying_entity_asserts_or_is_refused) {
    static bool add_in_hook = true;
    add_in_hook = true;
    const ComponentDesc kinds[] = {nnh::component<Hp>(4, [](World& w, Entity e, Hp&) {
                                       if (add_in_hook) CHECK(w.add<Pos>(e) == nullptr);
                                   }),
                                   nnh::component<Pos>(4)};
    auto r = World::create({.max_entities = 4}, kinds);
    g_world = &r.value();
    CHECK(world_test::asserts_or_is_ignored(add_to_a_dying_entity));
    world_test::recover(r.value());
    add_in_hook = false; // the final clear runs the hook again
    CHECK(r.value().storage<Pos>().count() == 0);
}

// Mutation testing (plan 020 T8.3) found these gaps.

NNH_TEST(two_destroys_from_one_hook_are_processed_in_call_order) {
    static char order[8];
    static int n = 0;
    static Entity b{}, c{};
    n = 0;
    const ComponentDesc kinds[] = {nnh::component<Hp>(4, [](World& w, Entity, Hp& hp) {
        if (n < 8) order[n++] = static_cast<char>('a' + hp.value);
        if (hp.value == 0) {
            w.destroy(b); // queued first
            w.destroy(c); // queued second
        }
    })};
    auto r = World::create({.max_entities = 4}, kinds);
    World& w = r.value();
    const Entity a = w.spawn();
    b = w.spawn();
    c = w.spawn();
    w.add<Hp>(a, {0});
    w.add<Hp>(b, {1});
    w.add<Hp>(c, {2});
    w.destroy(a);
    CHECK(n == 3 && order[0] == 'a' && order[1] == 'b' && order[2] == 'c');
}

NNH_TEST(destroying_a_world_runs_the_remove_hooks_of_what_is_left) {
    static int hooks = 0;
    hooks = 0;
    {
        const ComponentDesc kinds[] = {nnh::component<Hp>(4, [](World&, Entity, Hp&) { ++hooks; })};
        auto r = World::create({.max_entities = 4}, kinds);
        World& w = r.value();
        w.add<Hp>(w.spawn());
        w.add<Hp>(w.spawn());
    }
    CHECK(hooks == 2);
}
