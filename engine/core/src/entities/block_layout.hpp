#pragma once
// A world's block layout (spec 020 FR-9, D35): a pure function from the description to byte offsets and the total.
// Shared by World::create() and the tests (the block's size is checked against it).

#include "nnh/entities.hpp"

#include <cstddef>

namespace nnh::detail {

struct KindLayout {
    std::size_t data = 0;    ///< T[capacity + 1], aligned to alignof(T)
    std::size_t owner = 0;   ///< u16[capacity]
    std::size_t bitmaps = 0; ///< 4 × words: live, quarantine, fresh, removing
    int words = 0;
};

struct WorldLayout {
    std::size_t hot = 0, cold = 0, kind_hot = 0, kind_cold = 0;
    std::size_t kind_data = 0;      ///< kinds + 1 data pointers (the last null: the spare kind of get<T>)
    std::size_t records = 0;        ///< max_entities × record_words words
    std::size_t entity_bitmaps = 0; ///< 4 × words: live, quarantine, fresh, dying
    std::size_t queue = 0;          ///< u16 per entity: the destroy queue's links
    KindLayout kinds[kMaxKinds];
    std::size_t total = 0;
    std::size_t align = 0;
    int record_words = 0;
    int slots_word = 0;
    int entity_words = 0;
};

/// The layout of a valid description (validated by the caller).
WorldLayout world_layout(const WorldDesc& desc, const ComponentDesc* kinds, int count);

} // namespace nnh::detail
