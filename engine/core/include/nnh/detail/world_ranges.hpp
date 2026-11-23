#pragma once
/// @file nnh/detail/world_ranges.hpp
/// The ranges of a world's iterations (spec 020 FR-15): Storage<T>::items() and World::entities(). Each holds the
/// iteration guard, so `break` out of a range-for is safe. Engine-internal: use <nnh/entities.hpp>.

#include <cstddef>
#include <cstdint>
#include <utility>

#include "nnh/detail/world_layout.hpp"

namespace nnh::detail {

/// One component of an iteration over a kind: `for (auto [entity, body] : bodies.items())`.
template <typename T>
struct StorageItem {
    Entity entity;
    T& component;
};

inline Entity entity_at(const WorldHot& w, int slot) {
    return make_handle<World>(static_cast<std::uint32_t>(slot), record_at(w, static_cast<std::uint32_t>(slot))[0] & 0xFFFFu);
}

/// The range of Storage::items(): ascending component slots, FR-15's rules.
template <typename T>
class StorageRange {
public:
    StorageRange(WorldHot* w, int kind) : w_(w), kind_(kind), guard_(w) {}
    class iterator {
    public:
        StorageItem<T> operator*() const {
            const KindHot& k = w_->kinds[kind_];
            return {entity_at(*w_, k.owner[cursor_.slot()]), static_cast<T*>(k.data)[cursor_.slot()]};
        }
        iterator& operator++() {
            cursor_.next();
            return *this;
        }
        bool operator!=(const iterator& o) const { return cursor_.slot() != o.cursor_.slot(); }

    private:
        friend class StorageRange;
        iterator() = default;
        iterator(WorldHot* w, int kind)
            : w_(w), kind_(kind), cursor_(w, &w->kinds[kind].slots, words_for(w->kinds[kind].slots.extent)) {}
        WorldHot* w_ = nullptr;
        int kind_ = 0;
        BitCursor cursor_;
    };
    iterator begin() const { return w_ != nullptr ? iterator(w_, kind_) : iterator(); }
    iterator end() const { return iterator(); }

private:
    WorldHot* w_;
    int kind_;
    IterationGuard guard_;
};

/// The range of World::entities(): every live entity in ascending slots, FR-15's rules.
class EntityRange {
public:
    explicit EntityRange(WorldHot* w) : w_(w), guard_(w) {}
    class iterator {
    public:
        Entity operator*() const { return entity_at(*w_, cursor_.slot()); }
        iterator& operator++() {
            cursor_.next();
            return *this;
        }
        bool operator!=(const iterator& o) const { return cursor_.slot() != o.cursor_.slot(); }

    private:
        friend class EntityRange;
        iterator() = default;
        explicit iterator(WorldHot* w) : w_(w), cursor_(w, &w->entities, words_for(w->entities.extent)) {}
        WorldHot* w_ = nullptr;
        BitCursor cursor_;
    };
    iterator begin() const { return w_ != nullptr ? iterator(w_) : iterator(); }
    iterator end() const { return iterator(); }

private:
    WorldHot* w_;
    IterationGuard guard_;
};

/// World::each's loop (FR-15): ascending live slots of kind ka, the other kinds' slots from the owner's record, fresh
/// slots skipped, the live word re-read after each call. Inline in the game's code: no call per entity.
template <typename A, typename... B, typename F, std::size_t... I>
void each_with(WorldHot& w, int ka, const int* kb, F& fn, std::index_sequence<I...>) {
    const KindHot& a = w.kinds[ka];
    void* const bdata[] = {w.kinds[kb[I]].data..., nullptr};
    (void)bdata;
    A* const adata = static_cast<A*>(a.data);
    IterationGuard guard(&w);
    const int words = words_for(a.slots.extent);
    for (int word = 0; word < words; ++word) {
        std::uint32_t bits = a.slots.live[word];
        if ((w.pending & kPendingFresh) != 0) bits &= ~a.slots.fresh[word];
        while (bits != 0) {
            const int s = (word << 5) + lowest_bit(bits);
            bits &= bits - 1;
            const std::uint32_t owner = a.owner[s];
            std::uint32_t* r = record_at(w, owner);
            [[maybe_unused]] const std::uint16_t* slots = record_slots(w, r);
            [[maybe_unused]] const std::uint16_t bs[] = {slots[kb[I]]..., 0};
            bool ok = ((bs[I] != kNoSlot) && ... && true);
            if constexpr (sizeof...(B) > 0) {
                if (ok && (w.pending & kPendingFresh) != 0)
                    ok = (!bit_set(w.kinds[kb[I]].slots.fresh, bs[I]) && ...); // rare: loads here
            }
            if (ok) {
                fn(make_handle<World>(owner, r[0] & 0xFFFFu), adata[s], static_cast<B*>(bdata[I])[bs[I]]...);
            }
            bits &= a.slots.live[word]; // destroyed or removed by the callback: not visited
        }
    }
}

} // namespace nnh::detail
