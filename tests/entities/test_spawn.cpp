// Spawning entities (spec 020 FR-11, FR-2; T2.3): the lowest free slot, a full world asserts and counts a miss,
// alive(), stale handles after destroy, forged handles refused.

#include "world_fixture.hpp"

using nnh::Entity;
using nnh::World;
using world_test::Pos;

NNH_TEST(spawn_takes_the_lowest_free_entity_slot) {
    World w = world_test::make_world();
    for (int i = 0; i < 4; ++i) {
        const Entity e = w.spawn();
        CHECK(e.slot() == i && e.generation() == 1);
        CHECK(w.alive(e));
    }
    CHECK(w.count() == 4);
}

NNH_TEST(spawn_gives_a_new_entity_no_components_and_no_tags) {
    World w = world_test::make_world();
    const Entity a = w.spawn();
    w.add<Pos>(a);
    w.add_tag(a, nnh::tag("enemy"));
    w.destroy(a);
    w.flush();
    const Entity b = w.spawn();
    CHECK(b.slot() == a.slot() && b.generation() == 2);
    CHECK(!w.has<Pos>(b) && !w.has_tag(b, nnh::tag("enemy")));
}

NNH_TEST(destroyed_entities_are_stale_and_their_slots_quarantined) {
    World w = world_test::make_world();
    const Entity a = w.spawn();
    w.spawn();
    CHECK(w.destroy(a));
    CHECK(!w.alive(a));
    CHECK(!w.destroy(a)); // twice: false, nothing changes
    CHECK(w.spawn().slot() == 2); // slot 0 waits for the flush
    w.flush();
    CHECK(w.spawn().slot() == 0);
    CHECK(!w.destroy(Entity{}));
}

NNH_TEST(alive_refuses_forged_and_out_of_range_handles) {
    World w = world_test::make_world(8);
    const Entity a = w.spawn();
    w.destroy(a);
    w.flush();
    CHECK(!w.alive(nnh::detail::make_handle<World>(0, 2))); // the free slot's next generation
    CHECK(!w.alive(nnh::detail::make_handle<World>(9, 1)));
    CHECK(!w.alive(nnh::detail::make_handle<World>(65535, 1)));
    CHECK(!w.alive(Entity{}));
}

namespace {

World* g_full = nullptr;
void spawn_into_a_full_world() { CHECK(!g_full->spawn()); }

} // namespace

NNH_TEST(spawn_into_a_full_world_asserts_and_counts_a_miss) {
    World w = world_test::make_world(2);
    w.spawn();
    w.spawn();
    g_full = &w;
    CHECK(world_test::asserts_or_is_ignored(spawn_into_a_full_world));
    CHECK(w.stats().spawn_misses == 1);
    CHECK(w.count() == 2);
}

NNH_TEST(entity_generation_wraps_after_65535_reuses_of_a_slot) {
    World w = world_test::make_world(1);
    const Entity first = w.spawn();
    Entity e = first;
    for (int i = 0; i < 65535; ++i) {
        w.destroy(e);
        w.flush();
        e = w.spawn();
    }
    CHECK(e.generation() == 1);
    CHECK(w.alive(first)); // the documented wrap limit (FR-2), pinned
}
