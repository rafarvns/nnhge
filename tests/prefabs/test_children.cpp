// Registering prefabs with children (spec 022 FR-16, T5.1): entries in order, inherited and appended by variants, the
// node kind required on the parent and every child, every limit (9 entries, 33 entities, 129 components, height 16,
// max_children, a library without child capacity), a child of another library or not made by nnh::child() — each
// refusal leaving the whole block unchanged — and place(&Node::local).

#include "tree_fixture.hpp"

using nnh::ErrorCode;
using nnh::Node;
using nnh::Prefab;
using nnh::PrefabLibrary;
using namespace tree_test;

namespace {

/// A refusal with `code` that changes nothing in the block.
template <typename... T>
bool refused(PrefabLibrary& lib, ErrorCode code, const nnh::PrefabOptions& options, const T&... values) {
    const std::uint32_t before = whole_block_hash(lib);
    const int count = lib.count();
    const nnh::ErrorCode got = add_error(lib.add("refused", options, values...));
    return got == code && whole_block_hash(lib) == before && lib.count() == count;
}

} // namespace

NNH_TEST(children_are_registered_in_entry_order) {
    PrefabLibrary lib = make_tree_library();
    const Prefab muzzle = must_add(lib, "muzzle", {}, node_at(6, 0));
    const Prefab gun = must_add(lib, "gun", {.children = {nnh::child(muzzle)}}, node_at(10, 0), Coin{2});
    const Prefab ship = must_add(lib, "ship", {.children = {nnh::child(gun, {.position = {12, -4}}), nnh::child(gun)}},
                                 Node{}, Body{});
    Prefab out[8];
    CHECK(lib.children(ship, out, 8) == 2 && out[0] == gun && out[1] == gun);
    CHECK(lib.children(gun, out, 8) == 1 && out[0] == muzzle && lib.children(muzzle, out, 8) == 0);
    const nnh::detail::PrefabBlock* b = nnh::PrefabAccess::block(lib);
    CHECK(b->facts[ship.slot()].entities == 5 && b->facts[ship.slot()].components == 2 + 2 * 3);
    CHECK(b->facts[ship.slot()].height == 2 && b->children_used == 3);
}

NNH_TEST(children_variants_inherit_and_append) {
    PrefabLibrary lib = make_tree_library();
    const Prefab gun = must_add(lib, "gun", {}, Node{});
    const Prefab shield = must_add(lib, "shield", {}, Node{}, Bob{});
    const Prefab ship = must_add(lib, "ship", {.children = {nnh::child(gun)}}, Node{});
    const Prefab shielded = must_add(lib, "shielded", {.base = ship, .children = {nnh::child(shield)}});
    Prefab out[8];
    CHECK(lib.children(shielded, out, 8) == 2 && out[0] == gun && out[1] == shield);
    const Prefab plain_variant = must_add(lib, "plain_variant", {.base = ship});
    CHECK(lib.children(plain_variant, out, 8) == 1 && out[0] == gun);
}

NNH_TEST(children_refusals_change_nothing) {
    PrefabLibrary lib = make_tree_library(12);
    PrefabLibrary other = make_tree_library();
    const Prefab leaf = must_add(lib, "leaf", {}, Node{});
    const Prefab bare = must_add(lib, "bare", {}, Coin{});
    const Prefab foreign = must_add(other, "foreign", {}, Node{});
    CHECK(refused(lib, ErrorCode::InvalidArgument, {.children = {nnh::child(foreign)}}, Node{}));
    nnh::PrefabChild handmade{leaf};
    CHECK(refused(lib, ErrorCode::InvalidArgument, {.children = {handmade}}, Node{}));
    CHECK(refused(lib, ErrorCode::InvalidArgument, {.children = {nnh::child(leaf)}}, Coin{})); // parent without Node
    CHECK(refused(lib, ErrorCode::InvalidArgument, {.children = {nnh::child(bare)}}, Node{})); // child without Node
    const nnh::PrefabChild l = nnh::child(leaf);
    const Prefab eight = must_add(lib, "eight", {.children = {l, l, l, l, l, l, l, l}}, Node{});
    CHECK(refused(lib, ErrorCode::LimitReached, {.base = eight, .children = {l}})); // 9 with the inherited
    // The library's capacity: 12 entries, 8 used, 5 more refused, 4 accepted.
    CHECK(refused(lib, ErrorCode::LimitReached, {.children = {l, l, l, l, l}}, Node{}));
    must_add(lib, "four", {.children = {l, l, l, l}}, Node{});
    PrefabLibrary none = make_library(); // max_children 0
    const Prefab n = must_add(none, "n", {}, Node{});
    CHECK(refused(none, ErrorCode::LimitReached, {.children = {nnh::child(n)}}, Node{}));
}

NNH_TEST(children_tree_limits) {
    PrefabLibrary lib = make_tree_library(200);
    const nnh::PrefabChild leaf = nnh::child(must_add(lib, "leaf", {}, Node{}));
    const nnh::PrefabChild nine = nnh::child(must_add(lib, "nine", {.children = {leaf, leaf, leaf, leaf, leaf, leaf, leaf, leaf}}, Node{}));
    must_add(lib, "thirty_two", {.children = {nine, nine, nine, leaf, leaf, leaf, leaf}}, Node{});
    CHECK(refused(lib, ErrorCode::LimitReached, {.children = {nine, nine, nine, leaf, leaf, leaf, leaf, leaf}}, Node{}));
    // Components: leaves of 7 kinds, 8 under a middle (63), two middles under a root (133 > 128).
    const nnh::PrefabChild fat = nnh::child(must_add(lib, "fat", {}, Node{}, Body{}, Coin{}, Big{}, Mover{}, Bob{}, Counted{}));
    const nnh::PrefabChild mid = nnh::child(must_add(lib, "mid", {.children = {fat, fat, fat, fat, fat, fat, fat, fat}}, Node{},
                                                     Body{}, Coin{}, Big{}, Mover{}, Bob{}, Counted{}));
    CHECK(refused(lib, ErrorCode::LimitReached, {.children = {mid, mid}}, Node{}, Body{}, Coin{}, Big{}, Mover{}, Bob{},
                  Counted{}));
    // Height: a chain of 16 levels (height 15) is accepted, 17 levels refused.
    nnh::PrefabChild chain = leaf;
    char name[16] = "chain_00";
    for (int i = 1; i <= 15; ++i) {
        name[6] = static_cast<char>('0' + i / 10);
        name[7] = static_cast<char>('0' + i % 10);
        chain = nnh::child(must_add(lib, name, {.children = {chain}}, Node{}));
    }
    CHECK(nnh::PrefabAccess::block(lib)->facts[chain.prefab.slot()].height == 15);
    CHECK(refused(lib, ErrorCode::LimitReached, {.children = {chain}}, Node{}));
}

NNH_TEST(children_placement_through_the_node_local) {
    PrefabLibrary lib = make_tree_library();
    nnh::World w = make_tree_world();
    const Prefab p = must_add(lib, "placed", {.place = nnh::place(&Node::local)}, node_at(1, 2, 0.5f));
    const nnh::Entity e = lib.spawn(w, p, {30, 40});
    CHECK(w.get<Node>(e)->local.position == (nnh::Vec2{30, 40}) && w.get<Node>(e)->local.rotation == 0.5f);
    // A single-entity spawn keeps spec 021's path: the stored world is the stored value's until the next pass.
    CHECK(w.get<Node>(e)->world().position() == (nnh::Vec2{1, 2}));
}
