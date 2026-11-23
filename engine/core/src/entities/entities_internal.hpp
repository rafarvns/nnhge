#pragma once
// What another module needs from a world (spec 021 prefabs, plan 021 "Entities-internal additions"): a kind by type
// id, an all-or-nothing reservation of an entity with its components and tags, and a component slot's address. No
// public change to <nnh/entities.hpp>: spec 020's API and behaviour stay as they are.

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
/// KindFull, when given) after
/// counting the refusal as spawn()/add() would (the world's spawn misses, or that kind's misses; nothing while
/// clearing or for an empty world). Logs and asserts nothing. Allowed where spawn() is (iterations, remove hooks).
Entity world_reserve(World& world, const int* kinds, int count, void** components, const Tag* tags, int tag_count,
                     ReserveRefusal& why, int* full_kind);

/// The address of kind `kind`'s component slot `slot` (a reserved one).
void* world_component(World& world, int kind, int slot);

} // namespace nnh::detail
