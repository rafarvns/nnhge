// Shared world test helpers (spec 020): recovery after an expected assert.

#include "world_fixture.hpp"

#include "entities/world_internal.hpp"

namespace world_test {

void recover(nnh::World& world) {
    nnh::detail::WorldHot* w = nnh::WorldAccess::hot(world);
    if (w == nullptr) return;
    w->iterating = 0;
    w->cold->hook_depth = 0;
    w->cold->clearing = false;
    w->cold->queue_head = w->cold->queue_tail = -1;
    for (int i = 0; i < nnh::detail::words_for(w->entities.capacity); ++i) w->cold->dying[i] = 0;
    for (int k = 0; k < w->kind_count; ++k)
        for (int i = 0; i < nnh::detail::words_for(w->kinds[k].slots.capacity); ++i) w->kinds[k].removing[i] = 0;
    if (nnh::build::kAssertsEnabled) w->pending = static_cast<std::uint32_t>(w->pending & ~nnh::detail::kPendingDoomed);
    nnh::detail::world_end_iteration(*w);
}

} // namespace world_test
