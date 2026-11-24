// The self-check's prefabs, registered (register_prefabs) and written out flattened by hand (hand_def). The two halves
// never read each other: a mistake in either shows as sides that differ.

#include "self_check_defs.hpp"

namespace hcheck {
namespace {

using nnh::Node;
using nnh::Transform2D;

constexpr Transform2D kMuzzleLocal{.position = {6, 0}};
constexpr Transform2D kGunLocal{.position = {10, 0}};
constexpr Transform2D kGunOnShip{.position = {12, -4}, .rotation = 0.1f};
constexpr Transform2D kShieldOnShip{.position = {0, 10}};
constexpr Transform2D kNodeALocal{.position = {2, 3}};
constexpr Transform2D kNodeAOnTower{.position = {4, 4}, .rotation = nnh::kHalfTurn, .scale = {2, 0.5f}};
constexpr Transform2D kTowerLocal{.rotation = nnh::kQuarterTurn};
constexpr Transform2D kSpawnerLocal{.scale = {-1, 1}};
constexpr Transform2D kShieldLocal{.position = {0, -8}};

Transform2D ring_slot(int k) { return {.position = {8, 0}, .rotation = static_cast<float>(k) * nnh::kQuarterTurn * 0.5f}; }

// ---- The library side ----

nnh::SpawnHook hook(const HookTarget* t, int id) {
    return [t, id](nnh::World& w, nnh::Entity e, const nnh::SpawnInfo&) { t->run(t->side, id, w, e); };
}

template <typename... T>
bool add(nnh::PrefabLibrary& lib, nnh::Prefab* out, int def, const char* name, const nnh::PrefabOptions& o,
         const T&... values) {
    auto r = lib.add(name, o, values...);
    if (!r) return false;
    out[def] = r.value();
    return true;
}

// ---- The hand side ----

HandEntity entity(int parent, const Transform2D& local, std::uint32_t serial) {
    HandEntity e;
    e.parent = parent;
    e.local = local;
    e.has[kNode] = true;
    e.has[kK] = true;
    e.k = {serial};
    return e;
}

/// Appends `sub` (a whole flattened tree) under entity `parent` of `def`, with the entry local `local` (or the sub's).
void append(HandDef& def, const HandDef& sub, int parent, const Transform2D* local) {
    const int base = def.count;
    for (int i = 0; i < sub.count; ++i) {
        HandEntity e = sub.e[i];
        e.parent = i == 0 ? parent : sub.e[i].parent + base;
        if (i == 0 && local != nullptr) e.local = *local;
        def.e[def.count++] = e;
    }
}

void finish(HandDef& d) {
    for (int k = 0; k < kKinds; ++k) d.need[k] = 0;
    int depth[32] = {};
    d.height = 0;
    for (int i = 0; i < d.count; ++i) {
        for (int k = 0; k < kKinds; ++k) d.need[k] += d.e[i].has[k] ? 1 : 0;
        depth[i] = d.e[i].parent < 0 ? 0 : depth[d.e[i].parent] + 1;
        d.height = depth[i] > d.height ? depth[i] : d.height;
    }
}

struct Table {
    HandDef defs[kPrefabs];
    Table() {
        HandDef& bare = defs[kBare];
        bare.e[bare.count++] = entity(-1, {}, 1);
        HandDef& node_a = defs[kNodeA];
        node_a.e[0] = entity(-1, kNodeALocal, 2);
        node_a.e[0].has[kA] = true;
        node_a.e[0].a = {{0, 0, 8, 8}, 1};
        node_a.e[0].tags[node_a.e[0].tag_count++] = kTags[0];
        node_a.count = 1;
        HandDef& muzzle = defs[kMuzzle];
        muzzle.e[0] = entity(-1, kMuzzleLocal, 3);
        muzzle.e[0].tags[muzzle.e[0].tag_count++] = kTags[1];
        muzzle.count = 1;
        HandDef& gun = defs[kGun];
        gun.e[0] = entity(-1, kGunLocal, 4);
        gun.e[0].has[kC] = true;
        gun.e[0].c.a = 4;
        gun.e[0].c.b = 5;
        gun.count = 1;
        append(gun, muzzle, 0, nullptr);
        HandDef& shield = defs[kShield];
        shield.e[0] = entity(-1, kShieldLocal, 5);
        shield.e[0].has[kB] = true;
        shield.e[0].b = {{1, 2, 3, 4}};
        shield.e[0].tags[shield.e[0].tag_count++] = kTags[3];
        shield.count = 1;
        HandDef& ship = defs[kShip];
        ship.e[0] = entity(-1, {}, 6);
        ship.e[0].has[kA] = ship.e[0].has[kB] = true;
        ship.e[0].a = {{0, 0, 16, 16}, 2};
        ship.e[0].b = {{9, 8, 7, 6}};
        ship.e[0].tags[0] = kTags[0];
        ship.e[0].tags[1] = kTags[2];
        ship.e[0].tag_count = 2;
        ship.e[0].hook = kShipHook;
        ship.count = 1;
        ship.placed = true;
        append(ship, gun, 0, &kGunOnShip);
        append(ship, gun, 0, nullptr);
        HandDef& shielded = defs[kShielded];
        shielded = ship;
        append(shielded, shield, 0, &kShieldOnShip);
        HandDef& tower = defs[kTower];
        tower.e[0] = entity(-1, kTowerLocal, 7);
        tower.e[0].has[kD] = true;
        tower.e[0].d = {3};
        tower.e[0].hook = kTowerHook;
        tower.count = 1;
        tower.placed = true;
        append(tower, gun, 0, nullptr);
        append(tower, node_a, 0, &kNodeAOnTower);
        HandDef& ring = defs[kRing];
        ring.e[0] = entity(-1, {}, 8);
        ring.count = 1;
        for (int k = 0; k < 8; ++k) {
            const Transform2D at = ring_slot(k);
            append(ring, bare, 0, &at);
        }
        HandDef& swarm = defs[kSwarm];
        swarm.e[0] = entity(-1, {}, 9);
        swarm.e[0].has[kD] = true;
        swarm.e[0].d = {4};
        swarm.count = 1;
        swarm.placed = true;
        for (int r = 0; r < 3; ++r) append(swarm, ring, 0, nullptr);
        for (int b = 0; b < 4; ++b) append(swarm, bare, 0, nullptr);
        HandDef& plain = defs[kPlain];
        plain.e[0].has[kA] = plain.e[0].has[kK] = true;
        plain.e[0].a = {{1, 1, 2, 2}, 3};
        plain.e[0].k = {10};
        plain.e[0].tags[plain.e[0].tag_count++] = kTags[2];
        plain.count = 1;
        HandDef& spawner = defs[kSpawner];
        spawner.e[0] = entity(-1, kSpawnerLocal, 11);
        spawner.e[0].has[kD] = true;
        spawner.e[0].d = {5};
        spawner.e[0].hook = kSpawnerHook;
        spawner.count = 1;
        spawner.placed = true;
        for (HandDef& d : defs) finish(d);
    }
};

} // namespace

const HandDef& hand_def(int def) {
    static const Table table;
    return table.defs[def];
}

bool register_prefabs(nnh::PrefabLibrary& lib, nnh::Prefab* p, const HookTarget* t) {
    const auto at = nnh::place(&Node::local);
    bool ok = add(lib, p, kBare, "bare", {}, Node(), K{1});
    ok = ok && add(lib, p, kNodeA, "node_a", {.tags = {kTags[0]}}, Node(kNodeALocal), A{{0, 0, 8, 8}, 1}, K{2});
    ok = ok && add(lib, p, kMuzzle, "muzzle", {.tags = {kTags[1]}}, Node(kMuzzleLocal), K{3});
    ok = ok && add(lib, p, kGun, "gun", {.children = {nnh::child(p[kMuzzle])}}, Node(kGunLocal), C{4, 5}, K{4});
    ok = ok && add(lib, p, kShield, "shield", {.tags = {kTags[3]}}, Node(kShieldLocal), B{{1, 2, 3, 4}}, K{5});
    ok = ok && add(lib, p, kShip, "ship",
                   {.tags = {kTags[0], kTags[2]}, .place = at, .on_spawn = hook(t, kShipHook),
                    .children = {nnh::child(p[kGun], kGunOnShip), nnh::child(p[kGun])}},
                   Node(), A{{0, 0, 16, 16}, 2}, B{{9, 8, 7, 6}}, K{6});
    ok = ok && add(lib, p, kShielded, "ship_shielded", {.base = p[kShip], .children = {nnh::child(p[kShield], kShieldOnShip)}});
    ok = ok && add(lib, p, kTower, "tower",
                   {.place = at, .on_spawn = hook(t, kTowerHook),
                    .children = {nnh::child(p[kGun]), nnh::child(p[kNodeA], kNodeAOnTower)}},
                   Node(kTowerLocal), D{3}, K{7});
    nnh::PrefabOptions ring{};
    for (int k = 0; k < 8; ++k) ring.children[k] = nnh::child(p[kBare], ring_slot(k));
    ok = ok && add(lib, p, kRing, "ring", ring, Node(), K{8});
    const nnh::PrefabChild r = nnh::child(p[kRing]), b = nnh::child(p[kBare]);
    ok = ok && add(lib, p, kSwarm, "swarm", {.place = at, .children = {r, r, r, b, b, b, b}}, Node(), D{4}, K{9});
    ok = ok && add(lib, p, kPlain, "plain", {.tags = {kTags[2]}}, A{{1, 1, 2, 2}, 3}, K{10});
    ok = ok && add(lib, p, kSpawner, "spawner", {.place = at, .on_spawn = hook(t, kSpawnerHook)}, Node(kSpawnerLocal), D{5},
                   K{11});
    return ok;
}

} // namespace hcheck
