// Iterating components (spec 020 FR-15, FR-14; T3.3): one, two and three kinds, ascending slots of the first kind's
// storage, destroy/remove/spawn/add inside (self, earlier, later, into holes), nested each, entities() and break.

#include "world_fixture.hpp"

using nnh::Entity;
using nnh::World;
using world_test::Hp;
using world_test::Pos;
using world_test::Vel;

namespace {

struct Seen {
    int slots[64] = {};
    int n = 0;
    void add(int s) {
        if (n < 64) slots[n++] = s;
    }
};

} // namespace

NNH_TEST(each_with_one_kind_visits_its_storage_in_ascending_slots) {
    World w = world_test::make_world();
    Entity es[5];
    for (Entity& e : es) e = w.spawn();
    for (int i : {4, 2, 0}) w.add<Pos>(es[i], {i, 0});
    Seen seen;
    w.each<Pos>([&](Entity e, Pos& p) {
        CHECK(p.x == e.slot());
        seen.add(e.slot());
    });
    // Pos slots were taken in add order: es[4] → 0, es[2] → 1, es[0] → 2.
    CHECK(seen.n == 3 && seen.slots[0] == 4 && seen.slots[1] == 2 && seen.slots[2] == 0);
}

NNH_TEST(each_with_two_and_three_kinds_visits_only_entities_with_all_of_them) {
    World w = world_test::make_world();
    for (int i = 0; i < 6; ++i) {
        const Entity e = w.spawn();
        w.add<Pos>(e, {i, 0});
        if (i % 2 == 0) w.add<Vel>(e, {1, 0});
        if (i % 3 == 0) w.add<Hp>(e);
    }
    int two = 0, three = 0;
    w.each<Pos, Vel>([&](Entity, Pos& p, Vel& v) {
        p.x += v.dx;
        ++two;
    });
    w.each<Pos, Vel, Hp>([&](Entity e, Pos&, Vel&, Hp&) {
        CHECK(e.slot() == 0);
        ++three;
    });
    CHECK(two == 3 && three == 1);
    CHECK(w.get<Pos>(w.storage<Pos>().owner(2))->x == 3);
}

NNH_TEST(each_order_follows_the_first_listed_kind) {
    World w = world_test::make_world();
    Entity es[3];
    for (Entity& e : es) e = w.spawn();
    // Pos slots ascend with the entities; Vel slots descend.
    for (int i = 0; i < 3; ++i) w.add<Pos>(es[i]);
    for (int i = 2; i >= 0; --i) w.add<Vel>(es[i]);
    Seen by_pos, by_vel;
    w.each<Pos, Vel>([&](Entity e, Pos&, Vel&) { by_pos.add(e.slot()); });
    w.each<Vel, Pos>([&](Entity e, Vel&, Pos&) { by_vel.add(e.slot()); });
    CHECK(by_pos.slots[0] == 0 && by_pos.slots[1] == 1 && by_pos.slots[2] == 2);
    CHECK(by_vel.slots[0] == 2 && by_vel.slots[1] == 1 && by_vel.slots[2] == 0);
}

NNH_TEST(each_survives_destroying_self_earlier_and_later_entities) {
    World w = world_test::make_world(40, 40);
    Entity es[40];
    for (Entity& e : es) {
        e = w.spawn();
        w.add<Pos>(e);
    }
    Seen seen;
    w.each<Pos>([&](Entity e, Pos&) {
        seen.add(e.slot());
        w.destroy(e);                                    // self
        if (e.slot() + 3 < 40) w.destroy(es[e.slot() + 3]); // later: never visited
        if (e.slot() > 0) w.destroy(es[e.slot() - 1]);      // earlier: already gone
    });
    // 0 kills 3; 1 kills 4; 2 kills 5; 6 kills 9; ...: visited 0, 1, 2, 6, 7, 8, 12, ...
    CHECK(seen.n == 21 && seen.slots[3] == 6 && seen.slots[20] == 36 + 2 + 0);
    CHECK(w.count() == 0);
}

NNH_TEST(each_does_not_visit_what_is_spawned_or_added_inside_even_in_holes) {
    World w = world_test::make_world(64, 64);
    Entity es[8];
    for (Entity& e : es) {
        e = w.spawn();
        w.add<Pos>(e);
    }
    w.destroy(es[5]);
    w.flush(); // entity slot 5 and Pos slot 5: holes above the cursor
    int visits = 0;
    w.each<Pos>([&](Entity, Pos&) {
        ++visits;
        const Entity n = w.spawn();
        w.add<Pos>(n);
    });
    CHECK(visits == 7);
    CHECK(w.count() == 14);
    int after = 0;
    w.each<Pos>([&](Entity, Pos&) { ++after; });
    CHECK(after == 14);
}

NNH_TEST(each_does_not_visit_a_second_kind_added_or_removed_inside) {
    World w = world_test::make_world();
    Entity es[4];
    for (Entity& e : es) {
        e = w.spawn();
        w.add<Pos>(e);
    }
    w.add<Vel>(es[0]);
    w.add<Vel>(es[3]);
    Seen seen;
    w.each<Pos, Vel>([&](Entity e, Pos&, Vel&) {
        seen.add(e.slot());
        w.add<Vel>(es[1]);  // created inside: not visited
        w.remove<Vel>(es[3]); // removed: not visited
    });
    CHECK(seen.n == 1 && seen.slots[0] == 0);
}

NNH_TEST(nested_each_and_the_entities_range) {
    World w = world_test::make_world();
    for (int i = 0; i < 4; ++i) w.add<Pos>(w.spawn(), {i, 0});
    int pairs = 0;
    w.each<Pos>([&](Entity, Pos&) { w.each<Pos>([&](Entity, Pos&) { ++pairs; }); });
    CHECK(pairs == 16);
    int n = 0;
    for (Entity e : w.entities()) {
        CHECK(w.alive(e));
        if (++n == 2) break;
    }
    w.destroy(w.storage<Pos>().owner(0));
    w.flush(); // the break ended the iteration: no assert
    Seen seen;
    for (Entity e : w.entities()) {
        seen.add(e.slot());
        w.spawn(); // not visited
    }
    CHECK(seen.n == 3 && seen.slots[0] == 1);
}
