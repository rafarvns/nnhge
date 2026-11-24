// The pass (spec 022 FR-8, FR-21, T4.1): each node once, parents before children whatever their slots, roots bit-equal
// to their locals, nodes created or attached between passes holding their at-creation worlds, rotation-unchanged nodes
// reading the sine cache (proven with a planted cache value), stats, refusals inside iterations and hooks.

#include "hierarchy_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Reparent;
using nnh::Transform2D;
using nnh::Vec2;
using nnh::World;
using nnh::WorldTransform2D;
using namespace hierarchy_test;
using nnh::detail::NA;

NNH_TEST(propagate_computes_each_node_once_parents_first) {
    World w = make_world();
    Hierarchy h(w);
    // The child takes a lower slot than its parent: a slot-order pass would read the parent's old world.
    const Entity child = spawn_node(w, {.position = {1, 0}});
    const Entity parent = spawn_node(w, {.position = {10, 0}});
    const Entity grandchild = spawn_node(w, {.position = {0, 1}});
    h.attach(child, parent, Reparent::KeepLocal);
    h.attach(grandchild, child, Reparent::KeepLocal);
    h.local(parent)->position = {100, 50};
    CHECK(h.propagate() == 3);
    CHECK(h.world_transform(child)->position() == (Vec2{101, 50}));
    CHECK(h.world_transform(grandchild)->position() == (Vec2{101, 51}));
    const nnh::HierarchyStats s = h.stats();
    CHECK(s.roots == 1 && s.deepest == 2 && s.computed == 3);
}

NNH_TEST(propagate_roots_equal_their_locals_bit_for_bit) {
    World w = make_world();
    Hierarchy h(w);
    for (int i = 0; i < 20; ++i) {
        const Transform2D l{.position = {0.1f * i, -3.3f * i}, .rotation = 0.77f * i, .scale = {1.0f + i, -0.5f}};
        spawn_node(w, l);
    }
    h.propagate();
    for (auto [e, node] : w.storage<Node>().items()) {
        const WorldTransform2D& ww = node.world();
        CHECK(ww.position() == node.local.position && bits(ww.rotation) == bits(node.local.rotation));
        CHECK(ww.scale == node.local.scale && same_bits(ww, nnh::to_world(node.local)));
    }
}

NNH_TEST(propagate_matches_compose_along_every_chain) {
    World w = make_world();
    Hierarchy h(w);
    const Entity a = spawn_node(w, {.position = {5, 6}, .rotation = 0.3f, .scale = {2, 2}});
    const Entity b = spawn_node(w, {.position = {7, -1}, .rotation = -1.2f, .scale = {1, 0.5f}});
    const Entity c = spawn_node(w, {.position = {3, 3}});
    h.attach(b, a, Reparent::KeepLocal);
    h.attach(c, b, Reparent::KeepLocal);
    h.propagate();
    const WorldTransform2D wa = nnh::to_world(*h.local(a));
    const WorldTransform2D wb = nnh::compose(wa, *h.local(b));
    CHECK(same_bits(*h.world_transform(b), wb));
    CHECK(same_bits(*h.world_transform(c), nnh::compose(wb, *h.local(c))));
}

NNH_TEST(propagate_leaves_nodes_made_between_passes_at_their_creation_worlds) {
    World w = make_world();
    Hierarchy h(w);
    const Entity p = spawn_node(w, {.position = {10, 10}});
    h.propagate();
    const Entity fresh = spawn_node(w, {.position = {4, 4}});
    CHECK(same_bits(*h.world_transform(fresh), nnh::to_world({.position = {4, 4}})));
    h.attach(fresh, p, Reparent::KeepLocal);
    CHECK(h.world_transform(fresh)->position() == (Vec2{14, 14}));
    h.local(p)->position = {0, 0}; // not yet passed: the stored world is still the attach's
    CHECK(h.world_transform(fresh)->position() == (Vec2{14, 14}));
    h.propagate();
    CHECK(h.world_transform(fresh)->position() == (Vec2{4, 4}));
}

NNH_TEST(propagate_reads_the_sine_cache_while_the_rotation_is_unchanged) {
    World w = make_world();
    Hierarchy h(w);
    const Entity a = spawn_node(w, {.rotation = 0.5f});
    Node& n = *h.node(a);
    h.propagate();
    // Plant a wrong cached sine under the same rotation bits: the pass uses it (no recomputation)...
    NA::cached_sin(n) = 0.25f;
    h.propagate();
    CHECK(h.world_transform(a)->map.x.y == 0.25f);
    // ...and a changed rotation recomputes it.
    n.local.rotation = 0.6f;
    h.propagate();
    CHECK(NA::cached_rotation(n) == bits(0.6f) && NA::cached_sin(n) == nnh::sin_cos(0.6f).sin);
}

NNH_TEST(propagate_pure_translations_skip_the_sine) {
    World w = make_world();
    Hierarchy h(w);
    const Entity p = spawn_node(w, {.rotation = 1.0f});
    const Entity c = spawn_node(w, {.position = {3, 0}});
    h.attach(c, p, Reparent::KeepLocal);
    NA::cached_rotation(*h.node(c)) = 0x12345678u; // would be refreshed if the sine were needed
    h.propagate();
    CHECK(NA::cached_rotation(*h.node(c)) == 0x12345678u);
}

NNH_TEST(propagate_is_refused_inside_iterations_and_hooks) {
    static Hierarchy* g = nullptr;
    static int inside = -1;
    World w = make_world();
    Hierarchy h(w);
    g = &h;
    spawn_node(w);
    w.each<Node>([&](Entity, Node&) { CHECK(asserts_or_is_ignored([] { inside = g->propagate(); })); });
    world_test::recover(w);
    if (!nnh::build::kAssertsEnabled) CHECK(inside == 0);
    CHECK(h.propagate() == 1);
    g = nullptr;
}
