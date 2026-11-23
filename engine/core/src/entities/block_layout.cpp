// A world's block layout (spec 020 FR-9): header, cold part, kind tables, ids, entity records, entity bitmaps, the
// destroy queue, then per kind its objects, owners and bitmaps. Every part aligned to its type.

#include "entities/block_layout.hpp"
#include "entities/world_internal.hpp"

namespace nnh::detail {
namespace {

std::size_t align_up(std::size_t value, std::size_t align) { return (value + align - 1) / align * align; }

} // namespace

WorldLayout world_layout(const WorldDesc& desc, const ComponentDesc* kinds, int count) {
    WorldLayout l{};
    const std::size_t entities = static_cast<std::size_t>(desc.max_entities);
    l.slots_word = 2 + desc.tags_per_entity;
    l.record_words = l.slots_word + (count + 2) / 2; // + a spare slot field, always kNoSlot (get<T> of an unknown kind)
    l.entity_words = words_for(desc.max_entities);
    l.align = alignof(KindCold) > alignof(WorldHot) ? alignof(KindCold) : alignof(WorldHot);
    std::size_t at = 0;
    l.hot = at;
    at = l.cold = align_up(at + sizeof(WorldHot), alignof(WorldCold));
    at = l.kind_hot = align_up(at + sizeof(WorldCold), alignof(KindHot));
    at = l.kind_cold = align_up(at + sizeof(KindHot) * static_cast<std::size_t>(count), alignof(KindCold));
    at = l.kind_data = align_up(at + sizeof(KindCold) * static_cast<std::size_t>(count), alignof(void*));
    at = l.records = align_up(at + sizeof(void*) * static_cast<std::size_t>(count + 1), 4);
    at = l.entity_bitmaps = at + 4 * entities * static_cast<std::size_t>(l.record_words);
    at = l.queue = at + 4 * 4 * static_cast<std::size_t>(l.entity_words);
    at += 2 * entities;
    for (int k = 0; k < count; ++k) {
        const ComponentDesc& d = kinds[k];
        const std::size_t cap = static_cast<std::size_t>(d.capacity);
        KindLayout& kl = l.kinds[k];
        kl.words = words_for(d.capacity);
        kl.data = align_up(at, static_cast<std::size_t>(d.align));
        kl.owner = align_up(kl.data + (cap + 1) * static_cast<std::size_t>(d.size), 2);
        kl.bitmaps = align_up(kl.owner + 2 * cap, 4);
        at = kl.bitmaps + 4 * 4 * static_cast<std::size_t>(kl.words);
        if (static_cast<std::size_t>(d.align) > l.align) l.align = static_cast<std::size_t>(d.align);
    }
    l.total = at;
    return l;
}

} // namespace nnh::detail
