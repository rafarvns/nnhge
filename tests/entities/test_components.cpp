// Components (spec 020 FR-10, FR-12, FR-2; T2.4): add (with and without a value), get, has, remove, at; add twice,
// a full kind, an unregistered kind; stable addresses; component slots quarantined until the flush.

#include "world_fixture.hpp"

using nnh::Entity;
using nnh::World;
using world_test::Hp;
using world_test::Pos;
using world_test::Vel;

namespace {
struct Unregistered {
    int v = 0;
};
} // namespace

NNH_TEST(add_hands_out_the_slots_t_and_get_finds_it) {
    World w = world_test::make_world();
    const Entity e = w.spawn();
    Hp* hp = w.add<Hp>(e);
    CHECK(hp != nullptr && hp->value == 10);
    CHECK(w.get<Hp>(e) == hp);
    CHECK(w.has<Hp>(e) && !w.has<Pos>(e));
    CHECK(w.get<Pos>(e) == nullptr);
    const World& cw = w;
    CHECK(cw.get<Hp>(e) == hp);
}

NNH_TEST(add_with_a_value_copies_it) {
    World w = world_test::make_world();
    const Entity e = w.spawn();
    Pos* p = w.add<Pos>(e, {5, 6});
    CHECK(p->x == 5 && p->y == 6);
}

NNH_TEST(components_take_the_lowest_free_slot_of_their_kind) {
    World w = world_test::make_world();
    const Entity a = w.spawn(), b = w.spawn(), c = w.spawn();
    w.add<Pos>(c);
    w.add<Pos>(a);
    w.add<Vel>(b);
    CHECK(w.storage<Pos>().slot_of(c) == 0 && w.storage<Pos>().slot_of(a) == 1);
    CHECK(w.storage<Vel>().slot_of(b) == 0);
}

NNH_TEST(add_to_a_stale_entity_returns_null) {
    World w = world_test::make_world();
    const Entity e = w.spawn();
    w.destroy(e);
    CHECK(w.add<Pos>(e) == nullptr);
    CHECK(w.add<Pos>(Entity{}) == nullptr);
    CHECK(w.storage<Pos>().count() == 0);
}

NNH_TEST(remove_resets_the_component_and_quarantines_its_slot) {
    World w = world_test::make_world();
    const Entity a = w.spawn(), b = w.spawn();
    Pos* pa = w.add<Pos>(a, {1, 2});
    CHECK(w.remove<Pos>(a));
    CHECK(!w.has<Pos>(a) && w.get<Pos>(a) == nullptr);
    CHECK(pa->x == 0 && pa->y == 0); // the slot holds Pos{}
    CHECK(!w.remove<Pos>(a));        // absent: false
    CHECK(w.storage<Pos>().slot_of(w.spawn()) == -1);
    w.add<Pos>(b);
    CHECK(w.storage<Pos>().slot_of(b) == 1); // slot 0 waits for the flush
    w.flush();
    w.add<Pos>(a);
    CHECK(w.storage<Pos>().slot_of(a) == 0);
}

NNH_TEST(component_addresses_stay_put_across_other_changes) {
    World w = world_test::make_world(32, 32);
    const Entity keep = w.spawn();
    Pos* p = w.add<Pos>(keep, {7, 7});
    for (int i = 0; i < 20; ++i) {
        const Entity e = w.spawn();
        w.add<Pos>(e);
        w.add<Vel>(e);
        if (i % 3 == 0) w.remove<Pos>(e);
        if (i % 4 == 0) w.destroy(e);
        if (i % 5 == 0) w.flush();
    }
    CHECK(w.get<Pos>(keep) == p && p->x == 7);
}

namespace {

World* g_w = nullptr;
Entity g_e{};

void add_twice() {
    Pos* again = g_w->add<Pos>(g_e, {9, 9});
    CHECK(again == g_w->get<Pos>(g_e)); // the existing one, unchanged
}
void add_to_a_full_kind() { CHECK(g_w->add<Vel>(g_w->spawn()) == nullptr); }
void use_an_unregistered_kind() {
    CHECK(g_w->add<Unregistered>(g_e) == nullptr);
    CHECK(g_w->get<Unregistered>(g_e) == nullptr);
}
void at_on_a_stale_entity() { g_w->at<Pos>(g_e).x = 99; }

} // namespace

NNH_TEST(add_twice_asserts_and_keeps_the_existing_component) {
    World w = world_test::make_world();
    g_w = &w;
    g_e = w.spawn();
    w.add<Pos>(g_e, {1, 1});
    CHECK(world_test::asserts_or_is_ignored(add_twice));
    CHECK(w.get<Pos>(g_e)->x == 1);
}

NNH_TEST(add_to_a_full_kind_asserts_and_counts_a_miss) {
    World w = world_test::make_world(8, 1);
    w.add<Vel>(w.spawn());
    g_w = &w;
    CHECK(world_test::asserts_or_is_ignored(add_to_a_full_kind));
    CHECK(w.storage<Vel>().stats().misses == 1 && w.stats().add_misses == 1);
}

NNH_TEST(an_unregistered_kind_asserts_and_answers_nothing) {
    World w = world_test::make_world();
    g_w = &w;
    g_e = w.spawn();
    CHECK(world_test::asserts_or_is_ignored(use_an_unregistered_kind));
}

NNH_TEST(at_reads_a_live_component_and_asserts_on_a_stale_entity) {
    World w = world_test::make_world();
    const Entity e = w.spawn();
    w.add<Pos>(e, {3, 3});
    CHECK(w.at<Pos>(e).x == 3);
    g_w = &w;
    g_e = w.spawn();
    w.destroy(g_e);
    CHECK(world_test::asserts_or_is_ignored(at_on_a_stale_entity));
    CHECK(w.get<Pos>(e)->x == 3); // no live component was handed out
    if (!nnh::build::kAssertsEnabled) CHECK(w.at<Pos>(g_e).x == 0); // the kind's scratch Pos{}
}
