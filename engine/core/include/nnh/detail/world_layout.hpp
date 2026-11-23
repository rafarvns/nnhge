#pragma once
/// @file nnh/detail/world_layout.hpp
/// What the inline code of <nnh/entities.hpp> reads in a world's block (spec 020, D35): the hot header, one KindHot
/// per component kind, the entity records and their bitmaps. The cold parts (hooks, the destroy queue, the scene
/// attachment) are in src/entities/world_internal.hpp. Engine-internal: use <nnh/entities.hpp>.

#include <cstdint>

#include "nnh/detail/handle.hpp"
#include "nnh/detail/slot_bits.hpp"
#include "nnh/detail/type_id.hpp"

namespace nnh {

class World;
/// An entity of a World: a generational handle (slot 16 bits, generation 16 bits; 0 = null). See Handle.
using Entity = Handle<World>;

namespace detail {

inline constexpr int kMaxKinds = 32;
inline constexpr int kMaxTagsPerEntity = 8;
/// Bit 16 of an entity's meta word: live. Bits 0–15: the slot's current generation.
inline constexpr std::uint32_t kEntityLive = 1u << 16;
/// A record's slot field for a kind the entity doesn't have.
inline constexpr std::uint16_t kNoSlot = 0xFFFFu;
/// WorldHot::pending bits: work for the end of the outermost iteration.
inline constexpr std::uint32_t kPendingFresh = 1;  ///< something was spawned or added during it
inline constexpr std::uint32_t kPendingDoomed = 2; ///< the world was destroyed during it (Release misuse path)

/// One component kind's storage: T[capacity + 1] (the last is the checked accessor's scratch), the owner entity slot
/// of each component slot, and its slot table (live, quarantine, fresh bitmaps).
struct KindHot {
    SlotTable slots;
    void* data = nullptr;
    std::uint16_t* owner = nullptr;
    std::uint32_t* removing = nullptr; ///< bitmap: the component's remove hook is running
    int size = 0;
    bool dirty = false; ///< a slot was quarantined since the last flush
};

struct WorldCold;

/// The start of a world's block. Entity records are entity-major: {meta, kinds mask, tags[tags], u16 slot[kinds]}.
struct WorldHot {
    std::uint32_t* records = nullptr;
    int record_words = 0;  ///< a record's stride in 32-bit words
    int slots_word = 0;    ///< word index of slot[] in a record (2 + tags)
    int tags = 0;          ///< tags per entity
    int kind_count = 0;
    KindHot* kinds = nullptr;
    void* const* kind_data = nullptr; ///< each kind's data, then null: index kind_count is the spare kind
    SlotTable entities;
    int iterating = 0;
    std::uint32_t pending = 0; ///< a word, not a byte: a byte store may alias any counter
    WorldCold* cold = nullptr;
    TypeId ids[kMaxKinds] = {}; ///< kind_count type ids, in registration order (in the header: no pointer to chase)
};

inline std::uint32_t* record_at(const WorldHot& w, std::uint32_t slot) {
    return w.records + slot * static_cast<std::uint32_t>(w.record_words);
}
inline std::uint16_t* record_slots(const WorldHot& w, std::uint32_t* record) {
    return reinterpret_cast<std::uint16_t*>(record + w.slots_word);
}

/// The record of `e` when it is live, else null: one bound check, one load and one compare. The header's fields are
/// read before any branch, so a loop of lookups keeps them in registers (plan 020, ARM pre-check).
inline std::uint32_t* live_record(const WorldHot& w, Entity e) {
    std::uint32_t* const records = w.records;
    const std::uint32_t stride = static_cast<std::uint32_t>(w.record_words);
    const std::uint32_t capacity = static_cast<std::uint32_t>(w.entities.capacity);
    const std::uint32_t slot = e.bits() & 0xFFFFu;
    std::uint32_t* const r = records + slot * stride; // only read below once the slot is known to be in range
    if (slot >= capacity) return nullptr;
    return r[0] == ((e.bits() >> 16) | kEntityLive) ? r : nullptr;
}

/// The registration index of kind T in `w`, or -1. The first four registered kinds are found by straight compares
/// (the ids past kind_count are null and no type id is: no count check); the others by a scan of at most 32 ids.
/// Register the kinds looked up most first; hoist the lookup out of loops with World::storage<T>().
template <typename T>
int kind_index(const WorldHot& w) {
    const TypeId id = type_id<T>();
    if (w.ids[0] == id) return 0;
    if (w.ids[1] == id) return 1;
    if (w.ids[2] == id) return 2;
    if (w.ids[3] == id) return 3;
    for (int k = 4; k < w.kind_count; ++k)
        if (w.ids[k] == id) return k;
    return -1;
}

/// What an empty Storage view reads: one record, never live.
inline constexpr std::uint32_t kNoRecord[1] = {0};

/// The slot of `e`'s component in the u16 field `field` of its record (kNoSlot: none, or `e` stale). The caller reads
/// the layout (records, stride, capacity, field) before any branch, so a loop of lookups keeps it in registers.
inline std::uint32_t slot_by_handle(const std::uint32_t* records, std::uint32_t stride, std::uint32_t capacity,
                                    std::uint32_t field, Entity e) {
    const std::uint32_t slot = e.bits() & 0xFFFFu;
    if (slot >= capacity) return kNoSlot;
    const std::uint32_t* r = records + slot * stride;
    if (r[0] != ((e.bits() >> 16) | kEntityLive)) return kNoSlot;
    return reinterpret_cast<const std::uint16_t*>(r)[field];
}

inline bool bit_set(const std::uint32_t* bits, int slot) { return ((bits[slot >> 5] >> (slot & 31)) & 1u) != 0; }

// Out of line (src/entities/): once per operation or per iteration end, never per visited element.
void world_misuse(const char* what);
void world_end_iteration(WorldHot& w); ///< the outermost iteration ended with pending work
void* world_scratch(WorldHot& w, int kind); ///< the kind's scratch slot, re-made T{} (checked accessor, Release)

/// The iteration guard: nested iterations count; the outermost end clears fresh marks (and frees a world destroyed
/// during it). Held by each() and by the ranges, so `break` is safe.
class IterationGuard {
public:
    explicit IterationGuard(WorldHot* w) : w_(w) {
        if (w_ != nullptr) ++w_->iterating;
    }
    ~IterationGuard() {
        if (w_ != nullptr && --w_->iterating == 0 && w_->pending != 0) world_end_iteration(*w_);
    }
    IterationGuard(const IterationGuard&) = delete;
    IterationGuard& operator=(const IterationGuard&) = delete;

private:
    WorldHot* w_;
};

/// Walks the set bits of a bitmap below `words`, skipping fresh ones and re-reading the live word after each visit.
class BitCursor {
public:
    BitCursor() = default;
    BitCursor(const WorldHot* w, const SlotTable* table, int words)
        : w_(w), table_(table), words_(words), word_(-1) {
        find();
    }
    int slot() const { return slot_; }
    void next() {
        bits_ &= table_->live[word_]; // gone during the visit: not visited
        find();
    }

private:
    void find() {
        while (bits_ == 0) {
            if (++word_ >= words_) {
                slot_ = -1;
                return;
            }
            bits_ = table_->live[word_];
            if ((w_->pending & kPendingFresh) != 0) bits_ &= ~table_->fresh[word_];
        }
        slot_ = (word_ << 5) + lowest_bit(bits_);
        bits_ &= bits_ - 1;
    }

    const WorldHot* w_ = nullptr;
    const SlotTable* table_ = nullptr;
    int words_ = 0;
    int word_ = 0;
    std::uint32_t bits_ = 0;
    int slot_ = -1;
};

} // namespace detail
} // namespace nnh
