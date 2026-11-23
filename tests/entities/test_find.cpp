// Finding tagged entities (spec 020 FR-17; T4.3): ascending order, totals beyond capacity, counting with a null
// buffer, a null buffer with capacity > 0 asserts, first(), each_tagged with destroys and spawns inside, every
// tags_per_entity from 1 to 8 (each specialised scan).

#include "world_fixture.hpp"

using nnh::Entity;
using nnh::Tag;
using nnh::World;

namespace {
constexpr Tag kEnemy = nnh::tag("enemy");
constexpr Tag kCoin = nnh::tag("coin");
} // namespace

NNH_TEST(find_writes_tagged_entities_in_ascending_slots_and_returns_the_total) {
    World w = world_test::make_world(40, 16);
    Entity es[40];
    for (Entity& e : es) e = w.spawn();
    for (int i : {33, 2, 17, 5}) w.add_tag(es[i], kEnemy);
    w.add_tag(es[17], kCoin);
    Entity out[3];
    CHECK(w.find(kEnemy, out, 3) == 4);
    CHECK(out[0] == es[2] && out[1] == es[5] && out[2] == es[17]);
    CHECK(w.find(kEnemy, nullptr, 0) == 4);
    CHECK(w.find(kCoin, out, 3) == 1 && out[0] == es[17]);
    CHECK(w.first(kEnemy) == es[2]);
    w.destroy(es[2]);
    CHECK(w.first(kEnemy) == es[5]);
    CHECK(w.find(kEnemy, out, 0) == 3);
    CHECK(!w.first(nnh::tag("nobody")));
}

NNH_TEST(find_works_for_every_tag_count_from_1_to_8) {
    for (int tags = 1; tags <= 8; ++tags) {
        World w = world_test::make_world(8, 8, tags);
        const Entity a = w.spawn(), b = w.spawn();
        for (int i = 0; i < tags - 1; ++i) w.add_tag(a, Tag{100u + static_cast<std::uint32_t>(i)});
        w.add_tag(a, kEnemy); // in a's last word
        w.add_tag(b, kEnemy);
        Entity out[2];
        CHECK(w.find(kEnemy, out, 2) == 2);
        CHECK(out[0] == a && out[1] == b);
        if (tags > 1) CHECK(w.find(Tag{100}, out, 2) == 1);
    }
}

namespace {

World* g_w = nullptr;
void find_into_a_null_buffer() { CHECK(g_w->find(kEnemy, nullptr, 2) == 0); }

} // namespace

NNH_TEST(find_with_a_null_buffer_and_room_asserts) {
    World w = world_test::make_world();
    w.add_tag(w.spawn(), kEnemy);
    g_w = &w;
    CHECK(world_test::asserts_or_is_ignored(find_into_a_null_buffer));
}

NNH_TEST(each_tagged_visits_ascending_with_each_rules) {
    World w = world_test::make_world(40, 16);
    Entity es[10];
    for (Entity& e : es) {
        e = w.spawn();
        w.add_tag(e, kEnemy);
    }
    int seen[16] = {};
    int n = 0;
    w.each_tagged(kEnemy, [&](Entity e) {
        seen[n++] = e.slot();
        if (e.slot() + 1 < 10) w.destroy(es[e.slot() + 1]); // the next one: not visited
        const Entity child = w.spawn();
        w.add_tag(child, kEnemy); // created inside: not visited
    });
    CHECK(n == 5 && seen[0] == 0 && seen[1] == 2 && seen[4] == 8);
    CHECK(w.find(kEnemy, nullptr, 0) == 10);
}
