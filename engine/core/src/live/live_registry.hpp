#pragma once
// The live registry (spec 030 D51): created at the first registration with configure()'s capacities (tag `live`),
// freed by a shutdown hook. Entries are added and removed with their sorted indexes kept in name order, found by hash,
// identified by slot + generation. Internal to src/live/. Tests: tests/live/test_live_registry.cpp.

#include <cstdint>

#include "live/live_block.hpp"

namespace nnh::detail::live {

/// Table bits of an id's upper half: 0 tunables and actions, 1 registered watches.
inline constexpr std::uint32_t kWatchTable = 0x8000;

constexpr std::uint32_t make_id(std::uint32_t table, int slot, std::uint16_t generation) {
    return ((table | static_cast<std::uint32_t>(slot)) << 16) | generation;
}
constexpr bool id_is_watch(std::uint32_t id) { return ((id >> 16) & kWatchTable) != 0; }
constexpr int id_slot(std::uint32_t id) { return static_cast<int>((id >> 16) & 0x7FFF); }
constexpr std::uint16_t id_generation(std::uint32_t id) { return static_cast<std::uint16_t>(id); }

/// The registry, created (block allocated, ops pointer set, shutdown hook added) if needed. Never null in
/// Debug/Development (a failed allocation is a panic: the Development budget is fixed).
Registry& registry();

/// Capacities for the next creation; asserts when the registry exists.
void set_capacities(int max_tunables, int max_watches, int max_shown);

/// Frees the block (the shutdown hook; tests).
void destroy_registry();

/// Checks a name for registration: the rules, unused by any entry, no group clash. Asserts with the reason.
bool name_usable(const Registry& r, const char* text, int length);

/// Adds an entry with `name` (checked by name_usable first): a free slot, the name copied, a new generation, the
/// sorted index updated. Asserts on a full table. @return the slot, or -1.
int add_tunable(Registry& r, const char* name, int length, std::uint32_t hash);
int add_watch(Registry& r, const char* name, int length, std::uint32_t hash);
/// A per-frame watch's slot for `hash` (added with `name` when new); -1 when the table is full.
int find_or_add_shown(Registry& r, const char* name, int length, std::uint32_t hash, bool& added);

/// Removes an entry (slot freed, generation bumped, index updated, follows and pending state dropped).
void remove_tunable(Registry& r, int slot);
void remove_watch(Registry& r, int slot);
void remove_shown(Registry& r, int slot);

/// The entry of a live id, or null (stale, moved from, another registry).
Entry* tunable_of(Registry& r, std::uint32_t id);
WatchEntry* watch_of(Registry& r, std::uint32_t id);

/// Slots by hash (and name length: a hash alone never decides), or -1.
int find_tunable(const Registry& r, std::uint32_t hash);
int find_watch(const Registry& r, std::uint32_t hash);
int find_shown(const Registry& r, std::uint32_t hash);

/// Called when an entry is removed or added while a PC is attached (live_remote.cpp sets them).
extern void (*g_on_removed)(std::uint32_t hash);
extern void (*g_on_added)(int ref, bool watch);

} // namespace nnh::detail::live
