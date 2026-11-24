// All or nothing for prefab hierarchies (spec 022 FR-17, T5.3): a full world, a full kind (the root's, a child's, an
// override-only one), too deep under a parent — nothing created, the misses counted in the library and the world,
// WhenFull::Assert asserting and Miss quiet; inside clear() a null handle and nothing counted.

#include "../hierarchy/hierarchy_fixture.hpp"
#include "tree_fixture.hpp"

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::World;
using namespace tree_test;

namespace {

PrefabLibrary* g_lib = nullptr;
World* g_w = nullptr;
Prefab g_p{};
Entity g_e{}, g_parent{};
void spawn_g() { g_e = g_lib->spawn(*g_w, g_p); }

/// A three-entity tree: root (Node, Body) → a (Node, Coin) → b (Node, Bob).
Prefab define(PrefabLibrary& lib) {
    const Prefab b = must_add(lib, "b", {}, Node{}, Bob{});
    const Prefab a = must_add(lib, "a", {.children = {nnh::child(b)}}, Node{}, Coin{});
    return must_add(lib, "root", {.children = {nnh::child(a)}}, Node{}, Body{});
}

std::uint32_t world_hash(World& w) { return hierarchy_test::state_hash(w); }

} // namespace

NNH_TEST(tree_spawn_on_a_full_world_creates_nothing) {
    PrefabLibrary lib = make_tree_library(32, nnh::WhenFull::Miss);
    const Prefab root = define(lib);
    World w = make_tree_world(4, 4);
    w.spawn();
    w.spawn(); // two free entity slots for a three-entity tree
    const std::uint32_t before = world_hash(w);
    CHECK(!lib.spawn(w, root));
    CHECK(world_hash(w) == before && w.count() == 2 && lib.stats().misses == 1 && w.stats().spawn_misses == 1);
}

NNH_TEST(tree_spawn_on_a_full_kind_creates_nothing) {
    PrefabLibrary lib = make_tree_library(32, nnh::WhenFull::Miss);
    const Prefab root = define(lib);
    const nnh::ComponentDesc kinds[] = {nnh::node_component(16), nnh::component<Body>(16), nnh::component<Coin>(1),
                                        nnh::component<Bob>(16), nnh::component<Big>(1)};
    World w = static_cast<World&&>(World::create({.max_entities = 16}, kinds).value());
    w.add<Coin>(w.spawn()); // the child's kind is full
    const std::uint32_t before = world_hash(w);
    CHECK(!lib.spawn(w, root) && world_hash(w) == before && w.storage<Coin>().stats().misses == 1);
    w.destroy(w.storage<Coin>().owner(0));
    w.flush();
    w.add<Big>(w.spawn()); // an override-only kind full
    CHECK(!lib.spawn_with(w, root, Big{}) && w.storage<Big>().stats().misses == 1 && lib.stats().misses == 2);
    CHECK(lib.spawn(w, root) && w.storage<Node>().count() == 3);
}

NNH_TEST(tree_spawn_when_full_asserts_by_default) {
    PrefabLibrary lib = make_tree_library();
    g_p = define(lib);
    World w = make_tree_world(2, 2);
    g_lib = &lib;
    g_w = &w;
    CHECK(asserts_or_is_ignored(spawn_g) && !g_e && w.count() == 0);
    g_lib = nullptr;
    g_w = nullptr;
}

NNH_TEST(tree_spawn_too_deep_under_a_parent_is_a_miss) {
    PrefabLibrary lib = make_tree_library(32, nnh::WhenFull::Miss);
    const Prefab root = define(lib); // height 2
    World w = make_tree_world(64, 64);
    Hierarchy h(w);
    Entity chain = lib.spawn(w, must_add(lib, "plain", {}, Node{}));
    for (int i = 1; i < 14; ++i) chain = lib.spawn_under(w, lib.find("plain"), chain); // depth 13
    CHECK(h.depth(chain) == 13);
    const std::uint32_t before = world_hash(w);
    const int misses = lib.stats().misses;
    CHECK(!lib.spawn_under(w, root, chain)); // its deepest node would be at depth 13 + 1 + 2 = 16 (15 is the deepest)...
    CHECK(world_hash(w) == before && lib.stats().misses == misses + 1);
    CHECK(lib.spawn_under(w, root, h.parent(chain))); // ...under depth 12 it fits (13, 14, 15)
}

NNH_TEST(tree_spawn_inside_clear_is_null_and_not_counted) {
    static PrefabLibrary* lib_ptr = nullptr;
    static Prefab tree{};
    static Entity during{};
    PrefabLibrary lib = make_tree_library();
    lib_ptr = &lib;
    tree = define(lib);
    const nnh::ComponentDesc kinds[] = {nnh::node_component(16), nnh::component<Body>(16), nnh::component<Coin>(16),
                                        nnh::component<Bob>(16), nnh::component<Mover>(16, [](World& w, Entity, Mover&) {
                                            if (w.clearing()) during = lib_ptr->spawn(w, tree);
                                        })};
    World w = static_cast<World&&>(World::create({.max_entities = 16}, kinds).value());
    w.add<Mover>(w.spawn());
    w.clear();
    CHECK(!during && lib.stats().misses == 0 && w.count() == 0);
    lib_ptr = nullptr;
}
