// Flush and clear (spec 020 FR-14; T3.2): quarantine of entities and components, clear in ascending slots with
// hooks, spawn refused while clearing, the first-load slots after clear (generations differ), extents back to 0.

#include "world_fixture.hpp"

using nnh::ComponentDesc;
using nnh::Entity;
using nnh::World;
using world_test::Hp;
using world_test::Pos;

namespace {

int g_order[16];
int g_count = 0;
bool g_spawned_during_clear = true;
bool g_clearing_seen = false;

World clear_world() {
    const ComponentDesc kinds[] = {
        nnh::component<Hp>(16, [](World& w, Entity e, Hp&) {
            if (g_count < 16) g_order[g_count++] = e.slot();
            g_clearing_seen = w.clearing();
            g_spawned_during_clear = static_cast<bool>(w.spawn());
        }),
        nnh::component<Pos>(16)};
    auto r = World::create({.max_entities = 16}, kinds);
    g_count = 0;
    return static_cast<World&&>(r.value());
}

} // namespace

NNH_TEST(flush_frees_quarantined_entity_and_component_slots) {
    World w = world_test::make_world();
    const Entity a = w.spawn();
    w.add<Pos>(a);
    w.destroy(a);
    const Entity b = w.spawn();
    CHECK(b.slot() == 1);
    CHECK(w.storage<Pos>().slot_of(w.add<Pos>(b) ? b : Entity{}) == 1);
    w.flush();
    const Entity c = w.spawn();
    CHECK(c.slot() == 0 && c.generation() == 2);
    w.add<Pos>(c);
    CHECK(w.storage<Pos>().slot_of(c) == 0);
}

NNH_TEST(clear_destroys_in_ascending_slots_with_hooks_and_refuses_spawns) {
    World w = clear_world();
    Entity es[5];
    for (Entity& e : es) e = w.spawn();
    for (int i : {3, 0, 4, 1}) w.add<Hp>(es[i]);
    w.clear();
    CHECK(g_count == 4);
    CHECK(g_order[0] == 0 && g_order[1] == 1 && g_order[2] == 3 && g_order[3] == 4);
    CHECK(!g_spawned_during_clear && g_clearing_seen);
    CHECK(!w.clearing());
    CHECK(w.count() == 0);
}

NNH_TEST(clear_gives_the_first_load_slots_again_with_new_generations) {
    World w = world_test::make_world();
    Entity first[6];
    for (Entity& e : first) {
        e = w.spawn();
        w.add<Pos>(e);
    }
    w.destroy(first[2]);
    w.clear();
    CHECK(w.storage<Pos>().extent() == 0);
    for (int i = 0; i < 6; ++i) {
        const Entity e = w.spawn();
        w.add<Pos>(e);
        CHECK(e.slot() == first[i].slot());
        CHECK(e.generation() != first[i].generation());
        CHECK(w.storage<Pos>().slot_of(e) == i);
    }
}

NNH_TEST(clear_keeps_the_stats_misses_since_creation) {
    World w = world_test::make_world(4, 1);
    w.add<Pos>(w.spawn());
    w.clear();
    CHECK(w.stats().peak == 1);
}

namespace {

World* g_w = nullptr;
void flush_inside_each() {
    g_w->each<Pos>([](Entity, Pos&) { g_w->flush(); });
}
void clear_inside_each() {
    g_w->each<Pos>([](Entity, Pos&) { g_w->clear(); });
}

} // namespace

NNH_TEST(flush_and_clear_inside_an_iteration_assert_or_are_ignored) {
    World w = world_test::make_world();
    const Entity a = w.spawn();
    w.add<Pos>(a);
    w.add<Pos>(w.spawn());
    w.destroy(w.spawn());
    g_w = &w;
    CHECK(world_test::asserts_or_is_ignored(flush_inside_each));
    world_test::recover(w);
    CHECK(world_test::asserts_or_is_ignored(clear_inside_each));
    world_test::recover(w);
    CHECK(w.alive(a) && w.count() == 2);
    CHECK(w.spawn().slot() == 3); // slot 2 still quarantined: nothing was flushed
}
