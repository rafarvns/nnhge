#include "level_world.hpp"

namespace level {

nnh::Result<nnh::World> create_world() {
    const nnh::ComponentDesc kinds[] = {
        nnh::component<Body>(kMaxEntities),
        nnh::component<Coin>(kMaxCoins),
        nnh::component<Turret>(kMaxTurrets),
    };
    return nnh::World::create({.max_entities = kMaxEntities, .tags_per_entity = 2}, kinds);
}

} // namespace level
