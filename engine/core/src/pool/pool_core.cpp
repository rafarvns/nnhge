// Pools (spec 020 FR-6, FR-7, FR-23): the untyped operations, out of line because they run once per operation or
// per frame, never per slot: flush, the end of an iteration with fresh slots, misuse reports, inline-form setup.

#include "nnh/assert.hpp"
#include "pool/pool_internal.hpp"

NNH_LOG_CHANNEL(pool);

namespace nnh::detail {

void pool_misuse(const char* what) {
    NNH_LOG_ERROR(pool, "%s", what);
    NNH_ASSERT(false, "%s", what);
}

void pool_flush(PoolCore& core) {
    if (core.iterating > 0) {
        pool_misuse("Pool::flush() inside one of the pool's iterations (ignored)");
        return;
    }
    core.slots.flush();
}

bool pool_clear_allowed(const PoolCore& core) {
    if (core.iterating == 0) return true;
    pool_misuse("Pool::clear() inside one of the pool's iterations (ignored)");
    return false;
}

void pool_end_iteration(PoolCore& core) {
    core.slots.end_iteration();
    core.any_fresh = false;
}

void pool_init_inline(PoolCore& core, int capacity, std::uint32_t* meta, std::uint32_t* bitmaps, void* data) {
    const int words = words_for(capacity);
    for (int i = 0; i < capacity; ++i) meta[i] = 1; // generation 1, not live
    for (int i = 0; i < 3 * words; ++i) bitmaps[i] = 0;
    core.meta = meta;
    core.slots.live = bitmaps;
    core.slots.quarantine = bitmaps + words;
    core.slots.fresh = bitmaps + 2 * words;
    core.slots.capacity = capacity;
    core.data = data;
}

} // namespace nnh::detail
