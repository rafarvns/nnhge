// World statistics (spec 020 FR-24): entities live/peak/capacity and the misses of spawns and adds since creation.

#include "entities/world_internal.hpp"

namespace nnh {

WorldStats World::stats() const {
    if (hot_ == nullptr) return {};
    WorldStats s{hot_->entities.count, hot_->entities.peak, hot_->entities.capacity, hot_->entities.misses, 0};
    for (int k = 0; k < hot_->kind_count; ++k) s.add_misses += hot_->kinds[k].slots.misses;
    return s;
}

} // namespace nnh
