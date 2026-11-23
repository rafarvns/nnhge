// Flush and clear (spec 020 FR-14): quarantined entity and component slots become free; clear() destroys every
// entity in ascending slots (hooks run, spawns refused meanwhile) and flushes, so the next spawns and adds get the
// slots of the first load.

#include "entities/world_internal.hpp"

namespace nnh {
namespace detail {
namespace {

bool idle_or_report(const WorldHot& w, const char* what) {
    if (!world_busy(w)) return true;
    world_misuse(what);
    return false;
}

void flush_slots(WorldHot& w) {
    w.entities.flush();
    for (int k = 0; k < w.kind_count; ++k) {
        KindHot& kind = w.kinds[k];
        if (!kind.dirty) continue;
        kind.slots.flush();
        kind.dirty = false;
    }
}

} // namespace

void world_clear(World& world, WorldHot& w) {
    w.cold->clearing = true;
    const int words = words_for(w.entities.extent);
    for (int word = 0; word < words; ++word) {
        for (std::uint32_t bits = w.entities.live[word]; bits != 0; bits &= w.entities.live[word]) {
            const int slot = (word << 5) + lowest_bit(bits);
            bits &= bits - 1;
            world_destroy(world, w, static_cast<std::uint32_t>(slot));
            if ((w.pending & kPendingDoomed) != 0) return;
        }
    }
    flush_slots(w);
    w.entities.reset();
    for (int k = 0; k < w.kind_count; ++k) w.kinds[k].slots.reset();
    w.cold->clearing = false;
}

} // namespace detail

void World::flush() {
    if (hot_ == nullptr) {
        detail::world_misuse("use of an empty or moved-from nnh::World");
        return;
    }
    if (!detail::idle_or_report(*hot_, "World::flush() inside an iteration or a remove hook (ignored)")) return;
    detail::flush_slots(*hot_);
}

void World::clear() {
    detail::WorldHot* w = hot_;
    if (w == nullptr) {
        detail::world_misuse("use of an empty or moved-from nnh::World");
        return;
    }
    if (w->cold->clearing ||
        !detail::idle_or_report(*w, "World::clear() inside an iteration or a remove hook (ignored)"))
        return;
    detail::world_clear(*this, *w);
    detail::world_finish(*w);
}

bool World::clearing() const { return hot_ != nullptr && hot_->cold->clearing; }

} // namespace nnh
