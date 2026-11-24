// The node component (spec 022 FR-5, T3.1): 80 bytes, defaults, Node(local) computes a root world, copies carry the
// transforms but no links, assignment keeps the links, node_component with and without a game hook (the game's first),
// worlds without the kind untouched, a Hierarchy of a world without the kind asserts.

#include "hierarchy_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Transform2D;
using nnh::Vec2;
using nnh::World;
using namespace hierarchy_test;

NNH_TEST(node_is_80_bytes_with_identity_defaults) {
    static_assert(sizeof(Node) == 80);
    const Node n;
    CHECK(n.local.position == Vec2{} && n.local.rotation == 0.0f && n.local.scale == (Vec2{1, 1}));
    CHECK(same_bits(n.world(), nnh::WorldTransform2D{}) && n.depth() == 0);
    CHECK(nnh::detail::NA::parent(n) == nnh::detail::kNoNode && nnh::detail::NA::first(n) == nnh::detail::kNoNode);
}

NNH_TEST(node_made_at_a_local_holds_its_root_world) {
    const Transform2D l{.position = {3.5f, -2}, .rotation = 0.6f, .scale = {2, -1}};
    const Node n(l);
    CHECK(same_bits(n.world(), nnh::to_world(l)));
    CHECK(n.world().position() == l.position && n.world().rotation == l.rotation);
}

NNH_TEST(node_copy_carries_transforms_not_links) {
    World w = make_world();
    Hierarchy h(w);
    const Entity a = spawn_node(w, {.position = {10, 10}});
    const Entity b = spawn_node(w, {.position = {1, 2}});
    CHECK(h.attach(b, a, nnh::Reparent::KeepLocal));
    const Node copy(*w.get<Node>(b));
    CHECK(nnh::detail::NA::parent(copy) == nnh::detail::kNoNode && copy.depth() == 0);
    CHECK(same_bits(copy.world(), w.get<Node>(b)->world()) && copy.local.position == (Vec2{1, 2}));
    // Assignment keeps the destination's links.
    Node& target = *w.get<Node>(b);
    target = Node({.position = {7, 7}});
    CHECK(h.parent(b) == a && target.depth() == 1 && target.local.position == (Vec2{7, 7}));
    check_links(w);
}

NNH_TEST(node_component_runs_the_game_hook_first) {
    static int order = 0, game_saw = 0;
    static bool linked_in_game_hook = false;
    const nnh::ComponentDesc kinds[] = {nnh::node_component(8, [](World& w, Entity e, Node&) {
        game_saw = ++order;
        linked_in_game_hook = Hierarchy(w).parent(e) != Entity{};
    })};
    auto r = World::create({.max_entities = 8}, kinds);
    World w = static_cast<World&&>(r.value());
    Hierarchy h(w);
    const Entity a = spawn_node(w), b = spawn_node(w);
    h.attach(b, a);
    w.remove<Node>(b);
    CHECK(game_saw == 1 && linked_in_game_hook && h.child_count(a) == 0); // the engine's unlink came after
}

NNH_TEST(node_world_without_the_kind_is_untouched_and_a_view_of_it_asserts) {
    static World* g = nullptr;
    World w = world_test::make_world();
    g = &w;
    CHECK(asserts_or_is_ignored([] { Hierarchy h(*g); }));
    if (!nnh::build::kAssertsEnabled) {
        Hierarchy h(w);
        CHECK(h.stats().capacity == 0 && !h.parent(w.spawn()) && h.propagate() == 0);
    }
    g = nullptr;
    Hierarchy empty;
    CHECK(empty.depth(Entity{}) == -1 && empty.world_transform(Entity{}) == nullptr);
}
