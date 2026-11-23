// Spawning (spec 020 FR-11): the lowest free entity slot. Its record already says "no kind, no tag" (create() and
// every destroy leave it so); spawning only marks it live.

#include "entities/world_internal.hpp"

namespace nnh {

Entity World::spawn() {
    detail::WorldHot* w = hot_;
    if (w == nullptr) {
        detail::world_misuse("use of an empty or moved-from nnh::World");
        return {};
    }
    if (w->cold->clearing) return {}; // nothing may survive a clear (D30 proof, plan 020)
    const int slot = w->entities.find_free();
    if (slot < 0) {
        ++w->entities.misses;
        detail::world_misuse("World::spawn(): the world is full (raise WorldDesc::max_entities)");
        return {};
    }
    return detail::take_entity(*w, slot);
}

} // namespace nnh
