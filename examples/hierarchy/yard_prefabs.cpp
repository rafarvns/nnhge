// The yard's prefabs (spec 022 FR-16): hierarchies declared once — ship (gun → muzzle, hand), walker (hat), cart
// (rider), squash (a turned block under a (2, ½) scale: shear), crate (no children). Each root is placed through its
// node's local (`place(&Node::local)`); bodies are local boxes the yard maps to world bounds after each pass.

#include "yard.hpp"

namespace yard {
namespace {

using nnh::Node;

constexpr nnh::collision::Layers kPickable = 1;

Look look(int x, int y, int w, int h) { return {{x, y, w, h}, {w * 0.5f, h * 0.5f}}; }
Body body(float w, float h) { return {{-w / 2, -h / 2, w, h}, {}, kPickable}; }

template <typename... T>
bool add(nnh::PrefabLibrary& lib, nnh::Prefab& out, const char* name, const nnh::PrefabOptions& o, const T&... v) {
    auto r = lib.add(name, o, v...);
    if (r) out = r.value();
    return static_cast<bool>(r);
}

} // namespace

bool register_prefabs(nnh::PrefabLibrary& lib, Prefabs& p) {
    const nnh::Placement at = nnh::place(&Node::local);
    nnh::Prefab muzzle, gun, hand, hat, rider, block;
    bool ok = add(lib, muzzle, "muzzle", {}, Node({.position = {12, 0}}), Name{"muzzle"});
    ok = ok && add(lib, gun, "gun", {.children = {nnh::child(muzzle)}}, Node({.position = {8, 0}}),
                   Look{{24, 0, 14, 4}, {2, 2}}, Body{{-2, -2, 14, 4}, {}, kPickable}, Name{"gun"});
    ok = ok && add(lib, hand, "hand", {}, Node({.position = {0, 12}}), Look{{4, 16, 4, 4}, {2, 2}}, Name{"hand"});
    ok = ok && add(lib, p.ship, "ship", {.place = at, .children = {nnh::child(gun), nnh::child(hand)}}, Node(),
                   look(0, 0, 24, 16), body(24, 16), Name{"ship"});
    ok = ok && add(lib, hat, "hat", {}, Node({.position = {0, -11}}), look(52, 0, 10, 6), body(10, 6), Name{"hat"});
    ok = ok && add(lib, p.walker, "walker", {.place = at, .children = {nnh::child(hat)}}, Node(), look(40, 0, 12, 16),
                   body(12, 16), Name{"walker"});
    ok = ok && add(lib, rider, "rider", {}, Node({.position = {0, -13}}), look(88, 0, 10, 14), body(10, 14),
                   Name{"rider"});
    ok = ok && add(lib, p.cart, "cart", {.place = at, .children = {nnh::child(rider)}}, Node(), look(64, 0, 24, 12),
                   body(24, 12), Name{"cart"});
    ok = ok && add(lib, block, "block", {}, Node({.position = {10, 0}, .rotation = nnh::kQuarterTurn * 0.5f}),
                   look(112, 0, 16, 16), body(16, 16), Name{"block"});
    ok = ok && add(lib, p.squash, "squash", {.place = at, .children = {nnh::child(block)}},
                   Node({.scale = {2, 0.5f}}), look(112, 0, 16, 16), body(16, 16), Name{"squash"});
    ok = ok && add(lib, p.crate, "crate", {.place = at}, Node(), look(100, 0, 12, 12), body(12, 12), Name{"crate"});
    return ok;
}

} // namespace yard
