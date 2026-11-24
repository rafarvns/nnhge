// Map objects naming prefab hierarchies (spec 022 FR-18, T5.4): tests/data/maps/good_prefab_trees.tmj — whole
// hierarchies in map order, each root placed by its object, every SpawnInfo in a tree carrying the root's object (the
// children's without a position), the summary.

#include "map_fixture.hpp"
#include "tree_fixture.hpp"

#include <cstdio>
#include <string>

using nnh::Entity;
using nnh::Hierarchy;
using nnh::Node;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::Vec2;
using nnh::World;
using namespace tree_test;

namespace {

struct Seen {
    int ids[32];
    bool positions[32];
    int n = 0;
};
Seen g_seen;

nnh::Tilemap load_tree_map() {
    std::string bytes;
    std::FILE* f = std::fopen(NNH_TEST_MAPS_DIR "/good_prefab_trees.nnhmap", "rb");
    CHECK(f != nullptr);
    if (f == nullptr) return {};
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0) bytes.append(buffer, got);
    std::fclose(f);
    auto made = nnh::detail::tilemap_from_file(bytes.data(), bytes.size(), {}, "good_prefab_trees");
    CHECK(made.ok());
    return made ? std::move(made.value()) : nnh::Tilemap{};
}

void seen(World&, Entity, const nnh::SpawnInfo& info) {
    if (g_seen.n < 32) {
        g_seen.ids[g_seen.n] = info.object != nullptr ? info.object->id : -1;
        g_seen.positions[g_seen.n] = info.has_position;
        ++g_seen.n;
    }
}

} // namespace

NNH_TEST(map_objects_spawn_whole_hierarchies) {
    PrefabLibrary lib = make_tree_library();
    const Prefab muzzle = must_add(lib, "muzzle", {.on_spawn = seen}, node_at(6, 0));
    const Prefab gun = must_add(lib, "gun", {.on_spawn = seen, .children = {nnh::child(muzzle)}}, node_at(10, 0));
    const Prefab shield = must_add(lib, "shield", {.on_spawn = seen}, node_at(0, -8));
    const Prefab ship = must_add(lib, "ship",
                                 {.place = nnh::place(&Node::local), .on_spawn = seen, .children = {nnh::child(gun)}},
                                 Node{}, Body{});
    must_add(lib, "ship_shielded", {.base = ship, .children = {nnh::child(shield)}});
    must_add(lib, "crate", {.place = nnh::place(&Node::local), .on_spawn = seen}, Node{}, Coin{});
    World w = make_tree_world();
    Hierarchy h(w);
    const nnh::Tilemap map = load_tree_map();
    g_seen = {};
    const nnh::MapSpawnSummary s = lib.spawn_map_objects(w, map);
    CHECK(s.spawned == 4 && s.skipped == 1 && s.misses == 0 && s.unknown == 0);
    CHECK(w.count() == 3 + 1 + 4 + 3 && h.stats().nodes == 11);
    // Hooks per tree, post-order: muzzle, gun, ship (object 1); crate (2); muzzle, gun, shield, ship (3); ... (5).
    const int ids[] = {1, 1, 1, 2, 3, 3, 3, 3, 5, 5, 5};
    const bool roots[] = {false, false, true, true, false, false, false, true, false, false, true};
    CHECK(g_seen.n == 11);
    for (int i = 0; i < 11 && i < g_seen.n; ++i) CHECK(g_seen.ids[i] == ids[i] && g_seen.positions[i] == roots[i]);
    const Entity s1 = nnh::detail::entity_at(*nnh::WorldAccess::hot(w), 0);
    CHECK(h.world_transform(s1)->position() == (Vec2{32, 16}) && h.child_count(s1) == 1);
    CHECK(h.world_transform(h.first_child(h.first_child(s1)))->position() == (Vec2{48, 16}));
    const nnh::MapSpawnSummary check = lib.check_map_objects(map);
    CHECK(check.spawned == 4 && check.skipped == 1);
}
