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
    const bool iterating = w->iterating > 0;
    w->entities.take(slot, iterating);
    if (iterating) w->pending = static_cast<std::uint8_t>(w->pending | detail::kPendingFresh);
    std::uint32_t* r = detail::record_at(*w, static_cast<std::uint32_t>(slot));
    r[0] |= detail::kEntityLive;
    return detail::make_handle<World>(static_cast<std::uint32_t>(slot), r[0] & 0xFFFFu);
}

} // namespace nnh
