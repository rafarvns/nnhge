// Removing the node component (spec 022 FR-11, T4.4): the node leaves its parent, its children become roots keeping
// their stored worlds (their locals set from them), their subtrees' depths rewritten; nothing is destroyed.

#include "hierarchy_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Reparent;
using nnh::Vec2;
using nnh::World;
using namespace hierarchy_test;

NNH_TEST(remove_node_makes_children_roots_keeping_their_worlds) {
    World w = make_world();
    Hierarchy h(w);
    const Entity top = spawn_node(w, {.position = {10, 10}});
    const Entity mid = spawn_node(w, {.position = {5, 0}, .rotation = nnh::kQuarterTurn});
    const Entity a = spawn_node(w, {.position = {2, 0}});
    const Entity b = spawn_node(w, {.position = {0, 3}});
    const Entity leaf = spawn_node(w, {.position = {1, 1}});
    h.attach(mid, top, Reparent::KeepLocal);
    h.attach(a, mid, Reparent::KeepLocal);
    h.attach(b, mid, Reparent::KeepLocal);
    h.attach(leaf, a, Reparent::KeepLocal);
    h.propagate();
    const nnh::WorldTransform2D wa = *h.world_transform(a);
    CHECK(w.remove<Node>(mid));
    CHECK(w.alive(mid) && w.alive(a) && w.alive(b) && w.alive(leaf) && h.child_count(top) == 0);
    CHECK(h.parent(a) == Entity{} && h.depth(a) == 0 && h.depth(leaf) == 1 && h.parent(leaf) == a);
    CHECK(same_bits(*h.world_transform(a), wa) && h.local(a)->position == wa.position());
    CHECK(h.local(a)->rotation == nnh::kQuarterTurn);
    h.propagate();
    CHECK(h.world_transform(a)->position() == (Vec2{15, 12}));
    CHECK(h.world_transform(b)->position() == (Vec2{12, 10}));
    check_links(w);
}

NNH_TEST(remove_node_of_a_child_unlinks_it) {
    World w = make_world();
    Hierarchy h(w);
    const Entity p = spawn_node(w), c1 = spawn_node(w), c2 = spawn_node(w);
    h.attach(c1, p);
    h.attach(c2, p);
    CHECK(w.remove<Node>(c1) && h.child_count(p) == 1 && h.first_child(p) == c2 && w.alive(c1));
    check_links(w);
}
