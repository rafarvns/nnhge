#pragma once
// Prefab libraries (spec 021, D36): the block's structs and the helpers the module's sources share. A block is
// {PrefabBlock header, hashes[max_prefabs], table[table_size] (u16), records[max_prefabs], names (Debug/Development),
// entries[max_entries], values[value_bytes]}, laid out by library_layout().

#include <cstddef>
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
    std::uint8_t child_count = 0; ///< spec 022: child entries (0: spawned exactly as spec 021's)
    Tag tags[kMaxPrefabTags] = {};
    SpawnHook hook{};             ///< its own hook (empty when none)
};

// ---- Spec 022: prefab hierarchies (plan 022 "Prefab additions") ----

struct PrefabTreeOps;
/// One child entry: the child prefab's index and an optional local transform for its node. 24 bytes.
struct PrefabChildEntry {
    std::uint16_t prefab = 0;
    std::uint8_t has_local = 0;
    std::uint8_t spare = 0;
    Transform2D local{};
};
/// What a spawn of the prefab makes (itself and its children's trees). 6 bytes per prefab when max_children > 0.
struct PrefabTreeFacts {
    std::uint16_t first_child = 0; ///< its first child entry
    std::uint8_t entities = 1;     ///< ≤ 32
    std::uint8_t components = 0;   ///< ≤ 128
    std::uint8_t height = 0;       ///< levels below it (a leaf: 0), ≤ 15
    std::uint8_t spare = 0;
};
inline constexpr int kMaxPrefabTreeLevels = 16; ///< kMaxHierarchyDepth, without the hierarchy header

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
    // Spec 022 (at the end: no earlier field moves). Null and 0 when max_children is 0.
    PrefabChildEntry* children = nullptr;
    PrefabTreeFacts* facts = nullptr; ///< per prefab
    const PrefabTreeOps* tree = nullptr; ///< set by the first prefab registered with children
    int max_children = 0;
    int children_used = 0;
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
/// FNV-1a 32 of a NUL-terminated string in one pass, and of `size` bytes (the run-time forms of detail::fnv1a32: one
/// `mul` per byte, see library_find.cpp).
std::uint32_t hash_string(const char* text);
std::uint32_t hash_bytes(const char* text, std::size_t size);

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

/// The tree code's entry points (prefab_tree.cpp): the node kind's type id (checked at registration) and the spawn of
/// a hierarchy (`parent`: spawn_under's, else null).
struct PrefabTreeOps {
    TypeId node_kind = nullptr;
    Entity (*spawn)(PrefabBlock* b, World& world, Prefab prefab, const SpawnSource* source,
                    const ComponentValue* overrides, int count, Entity parent) = nullptr;
};

/// The spawn (FR-5..FR-8): checks, kinds, overrides, the world's reservation, copies, placement, hooks — or, for a
/// prefab with children (spec 022), the tree code through the block's pointer.
Entity prefab_spawn(PrefabBlock* b, World& world, Prefab prefab, const SpawnSource* source,
                    const ComponentValue* overrides, int count);

/// A map call's name → prefab memo (FR-9) for types and `prefab` values, direct-mapped by the string's pointer: a map
/// stores equal strings once, so one lookup per distinct name (types that alternate included); equal text at another
/// pointer is only a second lookup, never a wrong answer. Forgotten when the library grows (a hook may register), so
/// it answers what find() would answer at that moment.
struct NameMemo {
    static constexpr int kSlots = 16;
    const char* text[kSlots] = {};
    Prefab prefab[kSlots] = {};
    int prefabs = -1; ///< the library's count when the entries were found
    Prefab find(const PrefabLibrary& library, const char* name) {
        const PrefabBlock* b = PrefabAccess::block(library);
        const int now = b != nullptr ? b->prefabs : 0;
        if (now != prefabs) {
            for (const char*& t : text) t = nullptr;
            prefabs = now;
        }
        const auto bits = reinterpret_cast<std::uintptr_t>(name);
        const auto slot = static_cast<int>((bits ^ (bits >> 4)) & (kSlots - 1));
        if (text[slot] != name || name == nullptr) {
            text[slot] = name;
            prefab[slot] = library.find(name);
        }
        return prefab[slot];
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
    // Spec 022: children (the base's, then the options'), and the tree's facts.
    int child_total = 0;
    std::uint16_t child_prefab[kMaxPrefabChildren] = {};
    bool child_has_local[kMaxPrefabChildren] = {};
    Transform2D child_local[kMaxPrefabChildren] = {};
    const PrefabTreeOps* tree = nullptr;
    int tree_entities = 1, tree_components = 0, height = 0;
};

/// Flattens `values` over the options' base (FR-4): entries, tags, placement, hook chain. Returns false with `error`
/// (InvalidArgument or LimitReached) and leaves the block untouched.
bool flatten(const PrefabBlock& b, const PrefabOptions& options, const ComponentValue* values, int count,
             PrefabPlan& plan, Error& error);

/// Spec 022: the children of a flattened plan (prefab_tree_plan.cpp): checks and the tree's facts. False with `error`.
bool plan_children(const PrefabBlock& b, const PrefabOptions& options, const ComponentValue* values, PrefabPlan& plan,
                   Error& error);
/// Writes the plan's child entries and facts for prefab `index` (nothing when the library keeps no facts).
void commit_children(PrefabBlock& b, int index, const PrefabPlan& plan);

} // namespace detail
} // namespace nnh
