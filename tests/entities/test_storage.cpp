// Storage views (spec 020 FR-15, FR-24; T2.5): data, extent, stride, is_live, owner, slot_of, get, items(), count,
// capacity and kind stats; valid across spawns for the world's life.

#include "world_fixture.hpp"

using nnh::Entity;
using nnh::World;
using world_test::Hp;
using world_test::Pos;

NNH_TEST(storage_reads_a_kind_as_a_plain_array) {
    World w = world_test::make_world();
    nnh::Storage<Pos> pos = w.storage<Pos>();
    const Entity a = w.spawn(), b = w.spawn(), c = w.spawn();
    w.add<Pos>(a, {1, 0});
    w.add<Pos>(b, {2, 0});
    w.add<Pos>(c, {3, 0});
    w.remove<Pos>(b);
    CHECK(pos.extent() == 3);
    CHECK(nnh::Storage<Pos>::stride() == static_cast<int>(sizeof(Pos)));
    CHECK(pos.data()[0].x == 1 && pos.data()[1].x == 0 && pos.data()[2].x == 3);
    CHECK(pos.is_live(0) && !pos.is_live(1) && pos.is_live(2) && !pos.is_live(15) && !pos.is_live(-1));
    CHECK(pos.owner(2) == c && !pos.owner(1) && !pos.owner(99));
    CHECK(pos.slot_of(c) == 2 && pos.slot_of(b) == -1);
    CHECK(pos.get(c) == &pos.data()[2] && pos.get(b) == nullptr);
    CHECK(pos.count() == 2 && pos.capacity() == 16);
}

NNH_TEST(storage_items_visit_components_in_ascending_slots_with_their_owner) {
    World w = world_test::make_world();
    Entity es[5];
    for (int i = 0; i < 5; ++i) {
        es[i] = w.spawn();
        w.add<Hp>(es[i], {i});
    }
    w.destroy(es[1]);
    int n = 0;
    bool ok = true;
    for (auto [e, hp] : w.storage<Hp>().items()) {
        ok = ok && w.get<Hp>(e) == &hp && hp.value == e.slot();
        ++n;
    }
    CHECK(n == 4 && ok);
}

NNH_TEST(storage_view_stays_valid_across_spawns_and_destroys) {
    World w = world_test::make_world();
    nnh::Storage<Pos> pos = w.storage<Pos>();
    for (int i = 0; i < 10; ++i) w.add<Pos>(w.spawn(), {i, i});
    CHECK(pos.count() == 10 && pos.data()[9].y == 9);
    w.clear();
    CHECK(pos.count() == 0 && pos.extent() == 0);
    const nnh::KindStats s = pos.stats();
    CHECK(s.peak == 10 && s.capacity == 16);
}
