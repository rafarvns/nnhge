// Destroying a parent destroys its children (spec 022 FR-11, T4.4): every descendant destroyed through the queue,
// hooks breadth-first in child order with node data and last world intact, the parent stale and parent(child) null in
// a child's hook, hooks that destroy or attach during a cascade, detach_children first, clear() with hierarchies and the
// first load's slots after it, a game hook first.

#include "hierarchy_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Reparent;
using nnh::Vec2;
using nnh::World;
using namespace hierarchy_test;

namespace {

struct Log {
    static inline int order[32];
    static inline Vec2 where[32];
    static inline bool parent_null[32];
    static inline int n = 0;
    static inline Entity also_destroy{}, attach_child{}, attach_parent{};
    static inline World* world = nullptr;
    static void reset() {
        n = 0;
        also_destroy = attach_child = attach_parent = Entity{};
    }
};

/// A world whose Mark hook logs the mark's value, the node's stored position and whether its parent is null.
World make_logged() {
    const nnh::ComponentDesc kinds[] = {
        nnh::node_component(32),
        nnh::component<Mark>(32, [](World& w, Entity e, Mark& m) {
            Hierarchy h(w);
            if (Log::n < 32) {
                Log::order[Log::n] = m.value;
                Log::where[Log::n] = h.world_transform(e)->position();
                Log::parent_null[Log::n] = h.parent(e) == Entity{};
                ++Log::n;
            }
            if (m.value == 2 && Log::also_destroy) w.destroy(Log::also_destroy);
            if (m.value == 2 && Log::attach_child) h.attach(Log::attach_child, Log::attach_parent);
        })};
    auto r = World::create({.max_entities = 32}, kinds);
    CHECK(r.ok());
    return static_cast<World&&>(r.value());
}

Entity node(World& w, int mark, Vec2 at) {
    const Entity e = spawn_node(w, {.position = at});
    w.add<Mark>(e, {mark});
    return e;
}

} // namespace

NNH_TEST(cascade_destroys_every_descendant_breadth_first) {
    Log::reset();
    World w = make_logged();
    Hierarchy h(w);
    // 1 → (2 → (4, 5 → 7), 3 → 6)
    const Entity n1 = node(w, 1, {100, 0}), n2 = node(w, 2, {1, 0}), n3 = node(w, 3, {2, 0});
    const Entity n4 = node(w, 4, {3, 0}), n5 = node(w, 5, {4, 0}), n6 = node(w, 6, {5, 0}), n7 = node(w, 7, {6, 0});
    h.attach(n2, n1, Reparent::KeepLocal);
    h.attach(n3, n1, Reparent::KeepLocal);
    h.attach(n4, n2, Reparent::KeepLocal);
    h.attach(n5, n2, Reparent::KeepLocal);
    h.attach(n6, n3, Reparent::KeepLocal);
    h.attach(n7, n5, Reparent::KeepLocal);
    h.propagate();
    CHECK(w.destroy(n1));
    const int want[] = {1, 2, 3, 4, 5, 6, 7};
    CHECK(Log::n == 7);
    for (int i = 0; i < 7 && i < Log::n; ++i) CHECK(Log::order[i] == want[i]);
    CHECK(Log::where[1] == (Vec2{101, 0}) && Log::where[6] == (Vec2{111, 0})); // last worlds intact
    for (int i = 1; i < Log::n; ++i) CHECK(Log::parent_null[i]); // the parent already gone
    CHECK(w.count() == 0);
    for (Entity e : {n2, n3, n4, n5, n6, n7}) CHECK(!w.alive(e));
}

NNH_TEST(cascade_hooks_may_destroy_and_attach) {
    Log::reset();
    World w = make_logged();
    Hierarchy h(w);
    const Entity n1 = node(w, 1, {}), n2 = node(w, 2, {}), n3 = node(w, 3, {});
    const Entity other = node(w, 8, {}), keeper = node(w, 9, {}), loose = node(w, 10, {});
    h.attach(n2, n1);
    h.attach(n3, n2);
    Log::also_destroy = other;
    Log::attach_child = loose;
    Log::attach_parent = keeper;
    w.destroy(n1);
    CHECK(!w.alive(n3) && !w.alive(other) && w.alive(keeper) && h.parent(loose) == keeper);
    // Order: 1, 2 (queues 3; destroys 8: queued after it), 3, 8.
    const int want[] = {1, 2, 3, 8};
    CHECK(Log::n == 4);
    for (int i = 0; i < 4 && i < Log::n; ++i) CHECK(Log::order[i] == want[i]);
    check_links(w);
}

NNH_TEST(cascade_detach_children_first_keeps_them) {
    Log::reset();
    World w = make_logged();
    Hierarchy h(w);
    const Entity cart = node(w, 1, {50, 50}), rider = node(w, 2, {0, -8});
    h.attach(rider, cart, Reparent::KeepLocal);
    h.propagate();
    CHECK(h.detach_children(cart) == 1);
    w.destroy(cart);
    CHECK(w.alive(rider) && h.parent(rider) == Entity{} && h.world_transform(rider)->position() == (Vec2{50, 42}));
    h.propagate();
    CHECK(h.world_transform(rider)->position() == (Vec2{50, 42}));
}

NNH_TEST(cascade_clear_with_hierarchies_gives_the_first_loads_slots) {
    Log::reset();
    World w = make_logged();
    Hierarchy h(w);
    const auto load = [&] {
        Entity made[6];
        for (int i = 0; i < 6; ++i) made[i] = node(w, i + 1, {static_cast<float>(i), 0});
        h.attach(made[1], made[0]);
        h.attach(made[2], made[1]);
        h.attach(made[4], made[3]);
        h.attach(made[0], made[5]); // a parent at a higher slot than its child
        return made[5];
    };
    const Entity first = load();
    const int slots_before = w.storage<Node>().slot_of(first);
    w.clear();
    CHECK(w.count() == 0 && Log::n == 6);
    const Entity again = load();
    CHECK(w.storage<Node>().slot_of(again) == slots_before && again.slot() == first.slot());
    check_links(w);
}

NNH_TEST(cascade_game_hook_runs_first_with_the_node_linked) {
    static int children_seen = -1;
    const nnh::ComponentDesc kinds[] = {
        nnh::node_component(8, [](World& w, Entity e, Node&) { children_seen = Hierarchy(w).child_count(e); })};
    auto r = World::create({.max_entities = 8}, kinds);
    World w = static_cast<World&&>(r.value());
    Hierarchy h(w);
    const Entity p = spawn_node(w), c = spawn_node(w);
    h.attach(c, p);
    w.destroy(p);
    CHECK(children_seen == 0 && !w.alive(c)); // the child's own hook ran last (a leaf); the parent's saw 1, then...
    children_seen = -1;
    const Entity q = spawn_node(w), d = spawn_node(w);
    h.attach(d, q);
    w.remove<Node>(q); // removal: the game hook sees the child still linked
    CHECK(children_seen == 1 && w.alive(d) && h.parent(d) == Entity{});
}
