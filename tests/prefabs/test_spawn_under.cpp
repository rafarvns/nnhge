// Spawning under a parent (spec 022 FR-18, T5.3): a prefab with or without children attached keep-local as the last
// child of an existing node, its position local to the parent, depths below the parent's; the same bytes as spawning
// it as a root and attaching it keep-local; misuse.

#include "tree_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::Reparent;
using nnh::Vec2;
using nnh::World;
using namespace tree_test;

NNH_TEST(spawn_under_attaches_keep_local_with_a_local_position) {
    PrefabLibrary lib = make_tree_library();
    const Prefab tip = must_add(lib, "tip", {}, node_at(4, 0));
    const Prefab sword = must_add(lib, "sword", {.place = nnh::place(&Node::local), .children = {nnh::child(tip)}},
                                  Node{}, Coin{7});
    World w = make_tree_world();
    Hierarchy h(w);
    const Entity hero = w.spawn();
    w.add<Node>(hero, node_at(100, 50, nnh::kQuarterTurn));
    const Entity hand = w.spawn();
    w.add<Node>(hand, node_at(8, 0));
    h.attach(hand, hero, Reparent::KeepLocal);
    const Entity s = lib.spawn_under(w, sword, hand, {2, 0});
    CHECK(h.parent(s) == hand && h.depth(s) == 2 && h.depth(h.first_child(s)) == 3);
    CHECK(h.local(s)->position == (Vec2{2, 0}) && h.world_transform(s)->position() == (Vec2{100, 60}));
    CHECK(h.world_transform(h.first_child(s))->position() == (Vec2{100, 64}));
    // A prefab without children works too.
    const Entity t = lib.spawn_under(w, tip, hand);
    CHECK(h.parent(t) == hand && h.next_sibling(s) == t && h.world_transform(t)->position() == (Vec2{100, 62}));
}

NNH_TEST(spawn_under_equals_spawn_then_attach_keep_local) {
    PrefabLibrary lib = make_tree_library();
    const Prefab tip = must_add(lib, "tip", {}, node_at(4, 0, 0.3f));
    const Prefab sword = must_add(lib, "sword", {.children = {nnh::child(tip)}}, node_at(1, 1), Coin{7});
    World a = make_tree_world(), b = make_tree_world();
    for (World* w : {&a, &b}) {
        const Entity p = w->spawn();
        w->add<Node>(p, node_at(10, 20, 0.5f));
    }
    const Entity under = lib.spawn_under(a, sword, nnh::detail::entity_at(*nnh::WorldAccess::hot(a), 0));
    const Entity root = lib.spawn(b, sword);
    Hierarchy(b).attach(root, nnh::detail::entity_at(*nnh::WorldAccess::hot(b), 0), Reparent::KeepLocal);
    Hierarchy hb(b);
    const Entity tip_b = hb.first_child(root);
    // The hand-attached tree's grandchild keeps its stored world from the spawn until a pass: compare after one.
    Hierarchy(a).propagate();
    hb.propagate();
    CHECK(under == root && same_state(a, b) && tip_b);
}

NNH_TEST(spawn_under_misuse) {
    static PrefabLibrary* g = nullptr;
    static World* gw = nullptr;
    static Prefab p{};
    PrefabLibrary lib = make_tree_library();
    p = must_add(lib, "p", {}, Node{});
    World w = make_tree_world();
    g = &lib;
    gw = &w;
    CHECK(asserts_or_is_ignored([] { (void)g->spawn_under(*gw, p, Entity{}); }));
    CHECK(asserts_or_is_ignored([] { (void)g->spawn_under(*gw, p, gw->spawn()); })); // no node
    CHECK(w.storage<Node>().count() == 0);
    g = nullptr;
    gw = nullptr;
}
