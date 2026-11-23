// Prefab values from data assets (spec 021 T5.3, FR-10, AC-3): one prefab per Enemy asset of spec 014's test game,
// named by the asset's name and registered in a loop; spawned components equal what data::load() gave (and a struct
// the game converts from it); spawning reads no file.

#include "../data_assets/data_fixture.hpp"
#include "../data_assets/test_types.hpp"
#include "nnh/platform_fake.hpp"
#include "prefab_fixture.hpp"

#include <cstring>

using nnh::Entity;
using nnh::Prefab;
using nnh::PrefabLibrary;
using nnh::World;
using test_data::Enemy;

namespace {

/// What the game keeps per enemy, converted from the asset at load.
struct Health {
    std::int32_t hp = 0;
    std::int32_t max = 0;
};

} // namespace

NNH_TEST(one_prefab_per_data_asset_spawns_the_loaded_values) {
    data_test::start();
    {
        const int enemies = nnh::data::count<Enemy>();
        CHECK(enemies == 2);
        PrefabLibrary lib = prefab_test::make_library({.max_prefabs = 8, .max_entries = 16, .value_bytes = 4096});
        Enemy loaded[2];
        for (int i = 0; i < enemies && i < 2; ++i) {
            const char* name = nnh::data::name<Enemy>(i);
            CHECK(nnh::data::load(name, loaded[i]).ok());
            const Health health{loaded[i].stats.hp, loaded[i].stats.hp};
            CHECK(lib.add(name, {.tags = {prefab_test::kEnemy}}, loaded[i], health).ok());
        }
        const nnh::ComponentDesc kinds[] = {nnh::component<Enemy>(8), nnh::component<Health>(8)};
        World w = static_cast<World&&>(World::create({.max_entities = 8, .tags_per_entity = 1}, kinds).value());
        const int fs_before = nnh::platform_fake::state().fs_calls;
        const int reads_before = nnh::platform_fake::state().asset_reads;
        for (int i = 0; i < enemies && i < 2; ++i) {
            const Prefab p = lib.find(nnh::data::name<Enemy>(i));
            const Entity e = lib.spawn(w, p);
            CHECK(std::memcmp(w.get<Enemy>(e), &loaded[i], sizeof(Enemy)) == 0);
            CHECK(w.get<Health>(e)->hp == loaded[i].stats.hp && w.get<Health>(e)->max == loaded[i].stats.hp);
        }
        CHECK(nnh::platform_fake::state().fs_calls == fs_before);
        CHECK(nnh::platform_fake::state().asset_reads == reads_before);
        CHECK(lib.find("enemies/goblin") && lib.find("enemies/bat"));
    }
    data_test::stop();
}
