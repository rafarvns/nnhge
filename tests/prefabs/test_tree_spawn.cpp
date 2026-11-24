// Spawning a prefab hierarchy (spec 022 FR-17, T5.2): twin worlds — the tree spawn against the hand-written spawn,
// add, add_tag and attach(keep-local) calls in creation order, then the hooks in post-order — hold the same handles,
// slots, bytes, tags, links and world bits; hooks find their children ready; a hook destroying its own entity or
// spawning under itself.

#include "../hierarchy/hierarchy_fixture.hpp"
#include "tree_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::Reparent;
using nnh::Transform2D;
using nnh::Vec2;
using nnh::World;
using namespace tree_test;

namespace {

char g_log[64];
int g_n = 0;
void log(char c) {
    if (g_n < 63) g_log[g_n++] = c;
    g_log[g_n] = 0;
}
int g_children_at_ship_hook = -1;

struct Defs {
    Prefab muzzle, gun, ship;
};

Defs define(PrefabLibrary& lib) {
    Defs d;
    d.muzzle = must_add(lib, "muzzle", {.tags = {kFx}, .on_spawn = [](World&, Entity, const nnh::SpawnInfo&) { log('m'); }},
                        node_at(6, 0));
    d.gun = must_add(lib, "gun",
                     {.on_spawn = [](World&, Entity, const nnh::SpawnInfo&) { log('g'); }, .children = {nnh::child(d.muzzle)}},
                     node_at(10, 0), Coin{2});
    d.ship = must_add(lib, "ship",
                      {.tags = {kEnemy},
                       .place = nnh::place(&Node::local),
                       .on_spawn =
                           [](World& w, Entity e, const nnh::SpawnInfo&) {
                               log('s');
                               g_children_at_ship_hook = Hierarchy(w).child_count(e);
                           },
                       .children = {nnh::child(d.gun, {.position = {12, -4}, .rotation = 0.1f}), nnh::child(d.gun)}},
                      Node{}, Body{{0, 0, 16, 16}, 3});
    return d;
}

Entity hand_node(World& w, const Transform2D& local, Entity parent) {
    const Entity e = w.spawn();
    w.add<Node>(e, Node(local));
    if (parent) Hierarchy(w).attach(e, parent, Reparent::KeepLocal);
    return e;
}

/// The ship by hand: creation order ship, gun 1, muzzle 1, gun 2, muzzle 2; hooks in post-order.
Entity ship_by_hand(World& w, Vec2 at) {
    const Entity ship = hand_node(w, {.position = at}, {});
    w.add<Body>(ship, {{0, 0, 16, 16}, 3});
    w.add_tag(ship, kEnemy);
    const Entity g1 = hand_node(w, {.position = {12, -4}, .rotation = 0.1f}, ship);
    w.add<Coin>(g1, {2});
    const Entity m1 = hand_node(w, {.position = {6, 0}}, g1);
    w.add_tag(m1, kFx);
    const Entity g2 = hand_node(w, {.position = {10, 0}}, ship);
    w.add<Coin>(g2, {2});
    const Entity m2 = hand_node(w, {.position = {6, 0}}, g2);
    w.add_tag(m2, kFx);
    log('m');
    log('g');
    log('m');
    log('g');
    log('s');
    return ship;
}

} // namespace

NNH_TEST(tree_spawn_equals_the_hand_written_calls) {
    PrefabLibrary lib = make_tree_library();
    const Defs d = define(lib);
    World a = make_tree_world(), b = make_tree_world();
    for (int round = 0; round < 3; ++round) {
        g_n = 0;
        const Entity e = lib.spawn(a, d.ship, {50.0f + round, 60});
        char kept[64];
        std::memcpy(kept, g_log, sizeof kept);
        g_n = 0;
        const Entity f = ship_by_hand(b, {50.0f + round, 60});
        CHECK(e == f && std::memcmp(kept, g_log, sizeof kept) == 0 && std::strcmp(kept, "mgmgs") == 0);
        CHECK(same_state(a, b));
        if (round == 1) { // a destroy between spawns: the same slots reused on both sides
            a.destroy(e);
            b.destroy(f);
            a.flush();
            b.flush();
        }
    }
    CHECK(lib.stats().spawns == 3);
}

NNH_TEST(tree_spawn_links_and_worlds) {
    PrefabLibrary lib = make_tree_library();
    const Defs d = define(lib);
    World w = make_tree_world();
    Hierarchy h(w);
    const Entity ship = lib.spawn(w, d.ship, {100, 60});
    CHECK(h.child_count(ship) == 2 && g_children_at_ship_hook == 2);
    const Entity g1 = h.first_child(ship), g2 = h.next_sibling(g1);
    CHECK(h.local(g1)->position == (Vec2{12, -4}) && h.local(g2)->position == (Vec2{10, 0}));
    const Entity m2 = h.first_child(g2);
    CHECK(h.world_transform(m2)->position() == (Vec2{116, 60}) && h.depth(m2) == 2);
    CHECK(w.has_tag(m2, kFx) && w.get<Coin>(g2)->value == 2 && w.get<Body>(ship)->layers == 3);
    for (Entity e : {ship, g1, g2, m2}) CHECK(hierarchy_test::same_bits(h.world_now(e), *h.world_transform(e)));
}

NNH_TEST(tree_spawn_hooks_may_destroy_or_spawn_under_themselves) {
    static Prefab spark{};
    static PrefabLibrary* lib_ptr = nullptr;
    PrefabLibrary lib = make_tree_library();
    lib_ptr = &lib;
    spark = must_add(lib, "spark", {}, node_at(1, 1));
    const Prefab doomed = must_add(lib, "doomed", {.on_spawn = [](World& w, Entity e, const nnh::SpawnInfo&) { w.destroy(e); }},
                                   Node{});
    const Prefab breeder = must_add(lib, "breeder",
                                    {.on_spawn = [](World& w, Entity e, const nnh::SpawnInfo&) { lib_ptr->spawn_under(w, spark, e); },
                                     .children = {nnh::child(doomed)}},
                                    Node{});
    World w = make_tree_world();
    Hierarchy h(w);
    const Entity e = lib.spawn(w, breeder, {});
    CHECK(w.alive(e) && h.child_count(e) == 1 && w.count() == 2); // the doomed child went, the spark arrived
    lib_ptr = nullptr;
}
