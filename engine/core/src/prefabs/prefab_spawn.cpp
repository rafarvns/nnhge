// Spawning a prefab (spec 021 FR-5..FR-8, plan 021 "Spawn"): the kinds of its entries in one call, the copy's
// overrides matched to them, one all-or-nothing reservation in the world, then the copies in entry order (the slot's
// T{} destroyed, the stored value or the override copy-constructed), the placement member, the hook chain base first.
// The bytes are those of spawn() + add<T>(e, value) per entry + add_tag per tag: no code runs between the reservation
// and the copies, so the order in which distinct kinds took their slots can't change one.

#include "entities/entities_internal.hpp"
#include "nnh/assert.hpp"
#include "prefabs/prefab_internal.hpp"

#include <cstdio>

namespace nnh {
namespace detail {
namespace {

/// What a full capacity does (FR-8): a miss, and with WhenFull::Assert an error naming the prefab and an assert.
void refuse_full(PrefabBlock& b, int index, ReserveRefusal why) {
    ++b.counts.misses;
    if (b.when_full == WhenFull::Miss) return;
    char label[16];
    std::snprintf(label, sizeof label, "#%d", index);
    const char* name = b.names != nullptr ? b.names + index * kNameBytes : label;
    const char* what = why == ReserveRefusal::WorldFull ? "the world is full (raise WorldDesc::max_entities)"
                                                        : "a component kind is full (raise its capacity)";
    NNH_LOG_ERROR(prefabs, "spawn of prefab \"%s\": %s", name, what);
    NNH_ASSERT(false, "PrefabLibrary::spawn(\"%s\"): %s", name, what);
}

/// A kind the world doesn't store: always a bug (asserts whatever WhenFull says), counted as a miss.
Entity refuse_kind(PrefabBlock& b) {
    ++b.counts.misses;
    prefab_misuse("PrefabLibrary::spawn(): a component kind the world doesn't store");
    return {};
}

/// Overrides matched to the entries (`by_entry[i]`: the override for entry i, or −1); the others are appended to
/// `kinds` after the `n` entries (`extra`). False when one names a kind the world doesn't store.
bool match_overrides(World& world, const PrefabEntry* entries, int n, const ComponentValue* overrides, int count,
                     std::int8_t* by_entry, int* kinds, const ComponentValue** extra, int& extras) {
    for (int i = 0; i < n; ++i) by_entry[i] = -1;
    for (int j = 0; j < count; ++j) {
        const ComponentValue& v = overrides[j];
        int at = -1;
        for (int i = 0; i < n && at < 0; ++i)
            if (entries[i].id == v.id) at = i;
        bool twice = at >= 0 && by_entry[at] >= 0;
        for (int k = 0; k < extras && !twice; ++k) twice = extra[k]->id == v.id;
        if (twice) {
            prefab_misuse("PrefabLibrary::spawn_with(): a kind overridden twice (the second is ignored)");
            continue;
        }
        if (at >= 0) {
            by_entry[at] = static_cast<std::int8_t>(j);
            continue;
        }
        const int kind = world_kind_of(world, v.id);
        if (kind < 0 || v.copy == nullptr || n + extras >= kMaxPrefabEntries) return false;
        kinds[n + extras] = kind;
        extra[extras++] = &v;
    }
    return true;
}

void place(const PrefabRecord& r, void* const* at, const SpawnSource& s) {
    unsigned char* member = static_cast<unsigned char*>(at[r.place_entry]) + r.place_offset;
    if ((r.place_flags & kPlaceRect) == 0) {
        *reinterpret_cast<Vec2*>(member) = s.position;
        return;
    }
    Rect* rect = reinterpret_cast<Rect*>(member);
    rect->x = s.position.x;
    rect->y = s.position.y;
    if (s.rect_size && (r.place_flags & kPlaceKeepSize) == 0) {
        rect->w = s.size.x;
        rect->h = s.size.y;
    }
}

/// The chain, base first: this prefab's own hook last. A hook that destroys the library ends the chain; the block is
/// freed when the outermost spawn returns.
void run_hooks(PrefabBlock& b, int index, World& world, Entity e, const SpawnSource* source) {
    std::uint16_t chain[kMaxPrefabHooks];
    int count = 0;
    const PrefabRecord& r = b.records[index];
    if (r.hook) chain[count++] = static_cast<std::uint16_t>(index);
    for (std::uint16_t p = r.hook_base; p != kNoHookBase && count < kMaxPrefabHooks; p = b.records[p].hook_base)
        chain[count++] = p;
    SpawnInfo info{};
    if (source != nullptr) {
        info.has_position = source->has_position;
        info.position = source->position;
        info.object = source->object;
        info.object_index = source->object_index;
    }
    ++b.spawn_depth;
    for (int i = count - 1; i >= 0 && !b.doomed; --i) b.records[chain[i]].hook(world, e, info);
    --b.spawn_depth;
}

} // namespace

Entity prefab_spawn(PrefabBlock* b, World& world, Prefab prefab, const SpawnSource* source,
                    const ComponentValue* overrides, int count) {
    if (b == nullptr) {
        prefab_misuse("PrefabLibrary::spawn() on an empty or moved-from library");
        return {};
    }
    const PrefabRecord* r = record_of(b, prefab);
    if (r == nullptr) {
        prefab_misuse("PrefabLibrary::spawn(): a null prefab or a prefab of another library");
        return {};
    }
    if (world.capacity() == 0) {
        prefab_misuse("PrefabLibrary::spawn() into an empty or moved-from nnh::World");
        return {};
    }
    const PrefabEntry* entries = b->entries + r->first;
    const int n = r->entries;
    int kinds[kMaxPrefabEntries];
    if (world_kinds_of(world, &entries[0].id, sizeof(PrefabEntry), n, kinds) >= 0) return refuse_kind(*b);
    std::int8_t by_entry[kMaxPrefabEntries];
    const ComponentValue* extra[kMaxPrefabEntries];
    int extras = 0;
    if (count > 0 && !match_overrides(world, entries, n, overrides, count, by_entry, kinds, extra, extras))
        return refuse_kind(*b);
    bool placed = source != nullptr && source->has_position;
    if (placed && r->place_entry == kNoPlacement) { // from a map: normal (the hook still gets the position)
        if (source->object == nullptr)
            prefab_misuse("PrefabLibrary::spawn(): a position for a prefab without a placement member (ignored)");
        placed = false;
    }
    void* at[kMaxPrefabEntries];
    ReserveRefusal why = ReserveRefusal::None;
    const Entity e = world_reserve(world, kinds, n + extras, at, r->tags, r->tag_count, why, nullptr);
    if (!e) {
        if (why != ReserveRefusal::Clearing) refuse_full(*b, prefab.slot(), why);
        return {};
    }
    if (why == ReserveRefusal::TagsDropped)
        prefab_misuse("PrefabLibrary::spawn(): more prefab tags than the world's tags_per_entity (the first kept)");
    for (int i = 0; i < n; ++i) {
        const PrefabEntry& entry = entries[i];
        const void* from = count > 0 && by_entry[i] >= 0 ? overrides[by_entry[i]].value
                                                         : b->values + (entry.value & kValueOffset);
        if (entry.destroy != nullptr) entry.destroy(at[i]); // the slot's T{}, as add<T>(e, value)
        entry.copy(at[i], from);
    }
    for (int k = 0; k < extras; ++k) {
        if (extra[k]->destroy != nullptr) extra[k]->destroy(at[n + k]);
        extra[k]->copy(at[n + k], extra[k]->value);
    }
    if (placed) place(*r, at, *source);
    ++b->counts.spawns;
    if (r->hooks > 0) {
        run_hooks(*b, prefab.slot(), world, e, source);
        if (b->doomed && b->spawn_depth == 0) library_teardown(b);
    }
    return e;
}

} // namespace detail

Entity PrefabLibrary::spawn(World& world, Prefab prefab) {
    return detail::prefab_spawn(block_, world, prefab, nullptr, nullptr, 0);
}

Entity PrefabLibrary::spawn(World& world, Prefab prefab, Vec2 at) {
    detail::SpawnSource source;
    source.has_position = true;
    source.position = at;
    return detail::prefab_spawn(block_, world, prefab, &source, nullptr, 0);
}

Entity PrefabLibrary::spawn_with(World& world, Prefab prefab, const Vec2* at, const ComponentValue* overrides,
                                 int count) {
    if (count < 0 || (count > 0 && overrides == nullptr)) {
        detail::prefab_misuse("PrefabLibrary::spawn_with(): no overrides behind a positive count");
        count = 0;
    }
    if (at == nullptr) return detail::prefab_spawn(block_, world, prefab, nullptr, overrides, count);
    detail::SpawnSource source;
    source.has_position = true;
    source.position = *at;
    return detail::prefab_spawn(block_, world, prefab, &source, overrides, count);
}

} // namespace nnh
