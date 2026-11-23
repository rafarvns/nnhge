#pragma once
// Prefab libraries (spec 021, D36): the block's structs and the helpers the module's sources share. A block is
// {PrefabBlock header, hashes[max_prefabs], table[table_size] (u16), records[max_prefabs], names (Debug/Development),
// entries[max_entries], values[value_bytes]}, laid out by library_layout().

#include <cstdint>

#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "nnh/prefabs.hpp"

NNH_LOG_CHANNEL_EXTERN(prefabs);

namespace nnh {

/// Engine code and tests: a library's block.
struct PrefabAccess {
    static detail::PrefabBlock* block(const PrefabLibrary& l) { return l.block_; }
};

namespace detail {

/// One component entry: the kind, where its value is in the value bytes (bit 31: this entry owns it, i.e. destroys
/// it with the library; a variant's inherited entries share their base's value), and the type's functions.
struct PrefabEntry {
    TypeId id = nullptr;
    std::uint32_t value = 0;
    void (*copy)(void* to, const void* from) = nullptr;
    void (*destroy)(void*) = nullptr;
};
inline constexpr std::uint32_t kOwnsValue = 1u << 31;
inline constexpr std::uint32_t kValueOffset = ~kOwnsValue;

inline constexpr std::uint8_t kNoPlacement = 0xFF;
inline constexpr std::uint16_t kNoHookBase = 0xFFFF;
inline constexpr std::uint8_t kPlaceRect = 1;
inline constexpr std::uint8_t kPlaceKeepSize = 2;

/// One prefab, flattened: its entry range, tags, placement and hooks. 72 bytes on the 3DS, 80 on 64-bit hosts.
struct PrefabRecord {
    std::uint16_t first = 0;      ///< its first entry
    std::uint8_t entries = 0;     ///< entry count, ≤ 32
    std::uint8_t tag_count = 0;   ///< ≤ 8
    std::uint8_t place_entry = kNoPlacement; ///< the entry (of this prefab) whose component holds the member
    std::uint8_t place_flags = 0; ///< kPlaceRect, kPlaceKeepSize
    std::uint16_t place_offset = 0;
    std::uint16_t hook_base = kNoHookBase; ///< the nearest base with its own hook
    std::uint8_t hooks = 0;       ///< hooks in the chain, its own included, ≤ 8
    std::uint8_t spare = 0;
    Tag tags[kMaxPrefabTags] = {};
    SpawnHook hook{};             ///< its own hook (empty when none)
};

/// The block's header.
struct PrefabBlock {
    std::uint32_t* hashes = nullptr; ///< per prefab: its name's hash (find never touches the records)
    std::uint16_t* table = nullptr;  ///< open addressing, linear probing: prefab index + 1, 0 = empty
    PrefabRecord* records = nullptr;
    char* names = nullptr;           ///< kNameBytes per prefab, Debug/Development only (null in Release)
    PrefabEntry* entries = nullptr;
    unsigned char* values = nullptr;
    std::uint32_t table_mask = 0;
    int max_prefabs = 0;
    int max_entries = 0;
    int value_capacity = 0;
    int prefabs = 0;
    int entries_used = 0;
    int value_used = 0;
    PrefabStats counts{};      ///< spawns, misses, map_skipped, map_unknown
    int spawn_depth = 0;       ///< spawns running (hooks may spawn)
    std::uint16_t stamp = 0;   ///< the handles' high half, 1..65535
    WhenFull when_full = WhenFull::Assert;
    bool doomed = false;       ///< destroyed inside its own hook: freed when the outermost spawn returns
};

inline constexpr int kNameBytes = 32; ///< 31 characters and the NUL

MemTag& prefabs_memory_tag();

/// The record of `p` when it is a prefab of `b` (stamp and range), else null.
inline const PrefabRecord* record_of(const PrefabBlock* b, Prefab p) {
    if (b == nullptr) return nullptr;
    const std::uint32_t index = p.bits() & 0xFFFFu;
    if ((p.bits() >> 16) != b->stamp || index >= static_cast<std::uint32_t>(b->prefabs)) return nullptr;
    return b->records + index;
}

inline Prefab prefab_at(const PrefabBlock& b, int index) {
    return make_handle<PrefabLibrary>(static_cast<std::uint32_t>(index), b.stamp);
}

/// Misuse: logs an error and asserts (Debug/Development); Release goes on harmlessly.
void prefab_misuse(const char* what);

/// The table slot holding `hash`, or the empty slot where it would go.
std::uint32_t table_probe(const PrefabBlock& b, std::uint32_t hash);
/// FNV-1a 32 of a NUL-terminated string in one pass.
std::uint32_t hash_string(const char* text);

/// Frees the block after destroying the values its entries own.
void library_teardown(PrefabBlock* b);

/// Where a copy comes from (prefab_spawn.cpp, prefab_map.cpp): a position, and for a map object its size and itself.
struct SpawnSource {
    bool has_position = false;
    bool rect_size = false; ///< a rectangle map object: a Rect member also gets `size` (unless keep_size)
    Vec2 position{};
    Vec2 size{};
    const MapObject* object = nullptr;
    int object_index = -1;
};

/// The spawn (FR-5..FR-8): checks, kinds, overrides, the world's reservation, copies, placement, hooks.
Entity prefab_spawn(PrefabBlock* b, World& world, Prefab prefab, const SpawnSource* source,
                    const ComponentValue* overrides, int count);

/// A map call's type → prefab memo (FR-9), keyed by the type string's pointer: a map stores equal strings once, so
/// one lookup per distinct type; equal text at another pointer is only a second lookup, never a wrong answer.
struct TypeCache {
    const char* text = nullptr;
    Prefab prefab{};
    Prefab find(const PrefabLibrary& library, const char* type) {
        if (type != text) {
            text = type;
            prefab = library.find(type);
        }
        return prefab;
    }
};

/// A registration worked out before anything is written (library_add.cpp, prefab_flatten.cpp).
struct PrefabPlan {
    std::uint32_t hash = 0;
    int base = -1;                         ///< the base prefab's index, or −1
    int entries = 0;                       ///< after flattening
    std::int8_t own[kMaxPrefabEntries] = {}; ///< per entry: the own value giving it, or −1 (the base's entry i)
    Tag tags[kMaxPrefabTags] = {};
    int tag_count = 0;
    std::uint8_t place_entry = kNoPlacement;
    std::uint8_t place_flags = 0;
    std::uint16_t place_offset = 0;
    std::uint16_t hook_base = kNoHookBase;
    std::uint8_t hooks = 0;
    std::uint32_t offsets[kMaxPrefabEntries] = {}; ///< per own value (call order): its offset in the value bytes
    int value_end = 0;
};

/// Flattens `values` over the options' base (FR-4): entries, tags, placement, hook chain. Returns false with `error`
/// (InvalidArgument or LimitReached) and leaves the block untouched.
bool flatten(const PrefabBlock& b, const PrefabOptions& options, const ComponentValue* values, int count,
             PrefabPlan& plan, Error& error);

} // namespace detail
} // namespace nnh
