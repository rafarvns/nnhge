// Creating a world (spec 020 FR-9, FR-10): validation (Result errors, never asserts), one block of tagged memory laid
// out by world_layout(), the kind tables, the entity records (generation 1, no kind, no tag) and a T{} in every
// component slot.

#include "entities/block_layout.hpp"
#include "entities/world_internal.hpp"

#include <new>

NNH_MEM_TAG(entities);

namespace nnh {
namespace detail {
namespace {

bool invalid(Error& error, const char* what, int value) {
    error = Error::make(ErrorCode::InvalidArgument, "World::create: %s (%d)", what, value);
    return false;
}

bool validate(const WorldDesc& desc, const ComponentDesc* kinds, int count, Error& error) {
    if (desc.max_entities < 1 || desc.max_entities > 65535)
        return invalid(error, "max_entities outside 1..65535", desc.max_entities);
    if (desc.tags_per_entity < 0 || desc.tags_per_entity > kMaxTagsPerEntity)
        return invalid(error, "tags_per_entity outside 0..8", desc.tags_per_entity);
    if (kinds == nullptr || count < 1 || count > kMaxKinds) return invalid(error, "kinds: 1..32 needed", count);
    for (int k = 0; k < count; ++k) {
        const ComponentDesc& d = kinds[k];
        if (d.id == nullptr || d.construct == nullptr || d.size < 1 || d.align < 1)
            return invalid(error, "kind not made by nnh::component<T>()", k);
        if (d.capacity < 1 || d.capacity > desc.max_entities)
            return invalid(error, "a kind's capacity outside 1..max_entities", d.capacity);
        for (int j = 0; j < k; ++j)
            if (kinds[j].id == d.id) return invalid(error, "a kind listed twice, at index", k);
    }
    return true;
}

void init_records(WorldHot& w) {
    const int entities = w.entities.capacity;
    for (int e = 0; e < entities; ++e) {
        std::uint32_t* r = record_at(w, static_cast<std::uint32_t>(e));
        r[0] = 1; // generation 1, not live
        for (int i = 1; i < w.slots_word; ++i) r[i] = 0;
        std::uint16_t* slots = record_slots(w, r);
        for (int i = 0; i < 2 * (w.record_words - w.slots_word); ++i) slots[i] = kNoSlot;
    }
}

std::uint32_t* zeroed_words(unsigned char* at, int words) {
    auto* p = reinterpret_cast<std::uint32_t*>(at);
    for (int i = 0; i < words; ++i) p[i] = 0;
    return p;
}

void init_kind(WorldHot& w, const WorldLayout& l, unsigned char* bytes, int k, const ComponentDesc& d) {
    const KindLayout& kl = l.kinds[k];
    KindHot& kh = w.kinds[k];
    KindCold& kc = w.cold->kinds[k];
    kh.data = bytes + kl.data;
    kh.owner = reinterpret_cast<std::uint16_t*>(bytes + kl.owner);
    std::uint32_t* bits = zeroed_words(bytes + kl.bitmaps, 4 * kl.words);
    kh.slots.live = bits;
    kh.slots.quarantine = bits + kl.words;
    kh.slots.fresh = bits + 2 * kl.words;
    kh.removing = bits + 3 * kl.words;
    kh.slots.capacity = d.capacity;
    kh.slots.init_padding();
    kh.size = d.size;
    kc.construct = d.construct;
    kc.destroy = d.destroy;
    kc.hook = d.on_remove;
    if (kc.hook) w.cold->hooked |= 1u << k;
    for (int s = 0; s < d.capacity; ++s) kh.owner[s] = kNoSlot;
    for (int s = 0; s <= d.capacity; ++s) d.construct(component_at(w, k, s));
}

WorldHot* build(const WorldDesc& desc, const ComponentDesc* kinds, int count, const WorldLayout& l,
                unsigned char* bytes) {
    auto* w = ::new (bytes + l.hot) WorldHot{};
    w->cold = ::new (bytes + l.cold) WorldCold{};
    w->kinds = reinterpret_cast<KindHot*>(bytes + l.kind_hot);
    w->cold->kinds = reinterpret_cast<KindCold*>(bytes + l.kind_cold);
    for (int k = 0; k < count; ++k) {
        ::new (static_cast<void*>(w->kinds + k)) KindHot{};
        ::new (static_cast<void*>(w->cold->kinds + k)) KindCold{};
    }
    for (int k = 0; k < count; ++k) w->ids[k] = kinds[k].id;
    auto* kind_data = reinterpret_cast<void**>(bytes + l.kind_data);
    kind_data[count] = nullptr;
    w->kind_data = kind_data;
    w->kind_count = count;
    w->records = reinterpret_cast<std::uint32_t*>(bytes + l.records);
    w->record_words = l.record_words;
    w->slots_word = l.slots_word;
    w->tags = desc.tags_per_entity;
    std::uint32_t* bits = zeroed_words(bytes + l.entity_bitmaps, 4 * l.entity_words);
    w->entities.live = bits;
    w->entities.quarantine = bits + l.entity_words;
    w->entities.fresh = bits + 2 * l.entity_words;
    w->cold->dying = bits + 3 * l.entity_words;
    w->entities.capacity = desc.max_entities;
    w->entities.init_padding();
    w->cold->queue_next = reinterpret_cast<std::uint16_t*>(bytes + l.queue);
    init_records(*w);
    for (int k = 0; k < count; ++k) {
        init_kind(*w, l, bytes, k, kinds[k]);
        kind_data[k] = w->kinds[k].data;
    }
    return w;
}

} // namespace

MemTag& entities_memory_tag() { return NNH_MEM_TAG_REF(entities); }

} // namespace detail

Result<World> World::create(const WorldDesc& desc, const ComponentDesc* kinds, int count) {
    Error error{};
    if (!detail::validate(desc, kinds, count, error)) return error;
    const detail::WorldLayout l = detail::world_layout(desc, kinds, count);
    MemTag& tag = desc.memory_tag != nullptr ? *desc.memory_tag : detail::entities_memory_tag();
    auto* bytes = static_cast<unsigned char*>(mem::alloc(MemRegion::Heap, tag, l.total, l.align));
    if (bytes == nullptr) {
        return Error::make(ErrorCode::OutOfMemory, "World::create: no memory for %zu bytes", l.total);
    }
    World world;
    WorldAccess::set(world, detail::build(desc, kinds, count, l, bytes));
    return Result<World>(static_cast<World&&>(world));
}

} // namespace nnh
