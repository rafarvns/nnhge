#pragma once
// What another module needs from a world (spec 021 prefabs, plan 021 "Entities-internal additions"): a kind by type
// id, an all-or-nothing reservation of an entity with its components and tags, and a component slot's address; spec
// 022 (the hierarchy, prefab trees) adds per-world module words, the dying and busy queries and a group reservation.
// No public change to <nnh/entities.hpp>: spec 020's API and behaviour stay as they are.

#include <cstddef>
#include <cstdint>

#include "nnh/entities.hpp"

namespace nnh::detail {

/// The registration index of the kind with type id `id` in `world`, or −1 (an empty world, a null id: −1). The first
/// four kinds by straight compares, as kind_index<T>.
int world_kind_of(const World& world, TypeId id);

/// The kinds of `count` type ids read `stride` bytes apart from `first_id` (a prefab's entries), written into `out`;
/// one call per spawn. Returns -1 when every id is a kind of `world`, else the index of the first that isn't.
int world_kinds_of(const World& world, const TypeId* first_id, std::size_t stride, int count, int* out);

/// What world_reserve() did: None (reserved), TagsDropped (reserved, but tags past the world's tags_per_entity were
/// dropped), or why it took nothing.
enum class ReserveRefusal : std::uint8_t { None, TagsDropped, Clearing, WorldFull, KindFull, Empty };

/// All or nothing (spec 021 FR-8): the lowest free entity slot and, for each of `count` distinct registered kinds in
/// `kinds`, that kind's lowest free slot, taken with spawn()'s and add()'s bookkeeping (live, fresh during an
/// iteration, owners, the record's slot fields and kinds mask); `tags` (distinct, not kNoTag; at most the world's tags
/// per entity, extra ones dropped: `why` TagsDropped) written into the new record in order. Returns the entity and
/// `components[i]` = the address of kind kinds[i]'s slot (holding its T{}), or null with `why` (and `*full_kind` for
/// KindFull, when given) after counting the refusal as spawn()/add() would (the world's spawn misses, or that kind's
/// misses; nothing while clearing or for an empty world). Logs and asserts nothing. Allowed where spawn() is
/// (iterations, remove hooks).
Entity world_reserve(World& world, const int* kinds, int count, void** components, const Tag* tags, int tag_count,
                     ReserveRefusal& why, int* full_kind);

/// The address of kind `kind`'s component slot `slot` (a reserved one).
void* world_component(World& world, int kind, int slot);

// ---- Spec 022 (plan 022 "Entities-internal additions"): what the hierarchy and prefab trees need ----

/// Per-world words for modules built over worlds (the hierarchy: its active walks and stats). Zeroed at creation, kept
/// by clear(); at the END of the world's cold part, so no existing field moves.
struct WorldModuleState {
    void* walks = nullptr;
    std::uint32_t words[5] = {};
};
/// `world`'s module words (an empty world: asserts nothing, returns a scratch that is never kept — callers check
/// `world.capacity()` first).
WorldModuleState& world_module_state(World& world);
/// True when destroy() started for live `e` (its hooks running or queued): it is about to go.
bool world_dying(const World& world, Entity e);
/// True inside an iteration of `world` or one of its remove hooks.
bool world_busy(const World& world);

/// One entity of a group reservation: its kinds (indices `first_kind`.. into the call's `kinds`; the entities' ranges
/// follow each other from 0) and tags.
struct ReserveEntity {
    int first_kind = 0;
    int kind_count = 0;
    const Tag* tags = nullptr;
    int tag_count = 0;
};
/// All or nothing for `count` entities (spec 022 FR-17): exactly what `count` world_reserve() calls in a row would take
/// — the lowest free entity slots and, per kind, its lowest free slots, in list order — or nothing, counting the
/// refusal (`why`, `*full_kind`) as the first failing world_reserve() would and changing nothing else. `out[i]` is
/// entity i; `components` receives every reserved component's address in list order (sum of kind_count). Room is
/// counted first (free bits from each table's hint, no state change). Returns out[0], or null. Allowed where spawn()
/// is (iterations: fresh; remove hooks).
Entity world_reserve_many(World& world, const ReserveEntity* list, int count, const int* kinds, Entity* out,
                          void** components, ReserveRefusal& why, int* full_kind);

} // namespace nnh::detail
