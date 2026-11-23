#include "roster.hpp"

#include "components.hpp"
#include "data_types.hpp"
#include "tags.hpp"

#include <nnh/log.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

namespace room {

int register_roster(nnh::PrefabLibrary& library, nnh::Prefab* out, int capacity) {
    int registered = 0;
    const nnh::Placement box = nnh::place(&Body::box);
    for (int i = 0; i < nnh::data::count<EnemyData>() && registered < capacity; ++i) {
        const char* name = nnh::data::name<EnemyData>(i);
        EnemyData enemy;
        if (auto loaded = nnh::data::load(name, enemy); !loaded) {
            NNH_LOG_ERROR(example, "roster: %s", loaded.error().message);
            continue;
        }
        const float side = enemy.size;
        auto added = library.add(name, {.tags = {kEnemy, kRoster}, .place = box}, Body{{0, 0, side, side}, enemy.color},
                                 Walker{enemy.speed}, Stats{enemy.hp});
        if (!added) {
            NNH_LOG_ERROR(example, "roster: %s: %s", name, added.error().message);
            continue;
        }
        out[registered++] = added.value();
    }
    return registered;
}

} // namespace room
