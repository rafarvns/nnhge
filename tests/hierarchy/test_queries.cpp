// Hierarchy queries (spec 022 FR-7, FR-22, T3.4): parent, depth, root, child count, the children range (the next read
// before yielding), first/next walks, is_ancestor, local, node, world_transform, stats; stale handles and entities
// without a node answer nothing.

#include "hierarchy_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::World;
using namespace hierarchy_test;

NNH_TEST(queries_answer_a_small_tree) {
    World w = make_world();
    Hierarchy h(w);
    const Entity ship = spawn_node(w), gun = spawn_node(w), muzzle = spawn_node(w), hat = spawn_node(w);
    h.attach(gun, ship);
    h.attach(muzzle, gun);
    h.attach(hat, ship);
    CHECK(h.parent(muzzle) == gun && h.parent(ship) == Entity{});
    CHECK(h.depth(ship) == 0 && h.depth(gun) == 1 && h.depth(muzzle) == 2);
    CHECK(h.root(muzzle) == ship && h.root(ship) == ship);
    CHECK(h.child_count(ship) == 2 && h.child_count(muzzle) == 0);
    CHECK(h.first_child(ship) == gun && h.next_sibling(gun) == hat && h.next_sibling(hat) == Entity{});
    CHECK(h.is_ancestor(ship, muzzle) && h.is_ancestor(gun, muzzle) && !h.is_ancestor(muzzle, muzzle));
    CHECK(!h.is_ancestor(hat, muzzle) && !h.is_ancestor(muzzle, ship));
    CHECK(h.local(gun) == &w.get<Node>(gun)->local && h.node(gun) == w.get<Node>(gun));
    CHECK(h.world_transform(gun) == &w.get<Node>(gun)->world());
}

NNH_TEST(queries_on_stale_and_nodeless_entities_answer_nothing) {
    World w = make_world();
    Hierarchy h(w);
    const Entity plain = w.spawn();
    const Entity gone = spawn_node(w);
    w.destroy(gone);
    for (Entity e : {plain, gone, Entity{}}) {
        CHECK(h.parent(e) == Entity{} && h.depth(e) == -1 && h.root(e) == Entity{} && h.child_count(e) == 0);
        CHECK(h.first_child(e) == Entity{} && h.next_sibling(e) == Entity{} && !h.is_ancestor(e, e));
        CHECK(h.local(e) == nullptr && h.node(e) == nullptr && h.world_transform(e) == nullptr);
        int n = 0;
        for (Entity c : h.children(e)) n += c ? 1 : 1;
        CHECK(n == 0);
    }
}

NNH_TEST(queries_children_range_survives_destroying_the_current_child) {
    World w = make_world();
    Hierarchy h(w);
    const Entity p = spawn_node(w);
    Entity c[5];
    for (Entity& e : c) {
        e = spawn_node(w);
        h.attach(e, p);
    }
    int seen = 0;
    for (Entity e : h.children(p)) {
        CHECK(e == c[seen++]);
        if (seen % 2 == 1) w.destroy(e); // the current child: safe
    }
    CHECK(seen == 5 && h.child_count(p) == 2);
    seen = 0;
    for (Entity e : h.children(p)) {
        ++seen;
        h.detach(e);
    }
    CHECK(seen == 2 && h.child_count(p) == 0);
    check_links(w);
}

NNH_TEST(queries_stats_count_nodes_and_refusals) {
    World w = make_world(16, 8);
    Hierarchy h(w);
    const Entity a = spawn_node(w), b = spawn_node(w);
    h.attach(b, a);
    const nnh::HierarchyStats s = h.stats();
    CHECK(s.nodes == 2 && s.peak == 2 && s.capacity == 8 && s.refusals == 0 && s.bad_locals == 0);
}
