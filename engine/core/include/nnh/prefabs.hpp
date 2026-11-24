#pragma once
/// @file nnh/prefabs.hpp
/// Prefabs (spec 021, decision D36): named prototypes of entities — component values, tags, an optional placement
/// member and an optional spawn hook, optionally based on another prefab — registered in a library the game owns and
/// spawned into a spec 020 World, each copy bit for bit what spawn() + add<T>(e, value) per component + add_tag per tag
/// would make. A Tiled object names a prefab by its `prefab` property or its type. A prefab may list child prefabs
/// (spec 022): one spawn then makes the whole hierarchy of nodes, all or nothing. See docs/api/prefabs.md.
///
/// Optional (constitution XI): a game that doesn't include this header links none of it; a game that spawns only from
/// code links no tilemap code (the map calls live in their own object file), and one whose prefabs have no children
/// links no hierarchy code. Light: <nnh/entities.hpp>, <nnh/math.hpp> and <nnh/transform.hpp>; the tilemap types are
/// only forward-declared.
///
/// @code
/// struct Body { nnh::Rect box; nnh::collision::Layers layers = nnh::collision::kNoLayers; };
/// struct Coin { int value = 1; };
/// auto made = nnh::PrefabLibrary::create({.max_prefabs = 8, .max_entries = 16, .value_bytes = 256});
/// nnh::PrefabLibrary prefabs = std::move(made.value());            // check it in real code
/// const nnh::Prefab coin = prefabs.add("coin", {.tags = {kCoin}, .place = nnh::place(&Body::box)},
///                                      Body{{0, 0, 12, 12}, kCoinLayer}, Coin{1}).value();
/// nnh::Entity e = prefabs.spawn(world, coin, {64, 32});            // a Body at (64, 32), a Coin, the tag
/// prefabs.spawn_with(world, coin, {80, 32}, Coin{5});              // a gold coin: Coin overridden for this copy
/// prefabs.spawn_map_objects(world, map);                           // every map object naming a prefab
/// @endcode

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "nnh/detail/prefab_values.hpp"
#include "nnh/entities.hpp"
#include "nnh/math.hpp"
#include "nnh/result.hpp"
#include "nnh/transform.hpp"

namespace nnh {

class Tilemap;   // <nnh/tilemap.hpp>
struct MapObject; // <nnh/tilemap.hpp>
struct MemTag;   // <nnh/memory.hpp>
class PrefabLibrary;

namespace detail {
struct PrefabBlock;
struct PrefabTreeOps; // src/prefabs/prefab_internal.hpp
/// The prefab-tree code (src/prefabs/prefab_tree.cpp), named only by nnh::child(): a game whose prefabs have no
/// children links none of it (nor any hierarchy code).
extern const PrefabTreeOps kPrefabTreeOps;
} // namespace detail

/// A registered prefab: its index (bits 0–15) and its library's stamp (bits 16–31); 0 is the null prefab. Trivially
/// copyable, 4 bytes. A prefab of another library is refused (asserts) in all but 1 case in 65 535.
using Prefab = Handle<PrefabLibrary>;

inline constexpr int kMaxPrefabs = 1024;     ///< prefabs per library
inline constexpr int kMaxPrefabEntries = 32; ///< component entries per prefab (as spec 020's 32 kinds)
inline constexpr int kMaxPrefabTags = 8;     ///< tags per prefab (as spec 020's 8 tags per entity)
inline constexpr int kMaxPrefabHooks = 8;    ///< a prefab's hook plus its bases' hooks

inline constexpr int kMaxPrefabChildren = 8;        ///< child entries per prefab (a variant's inherited ones count)
inline constexpr int kMaxPrefabTree = 32;           ///< entities one spawn of a prefab hierarchy makes
inline constexpr int kMaxPrefabTreeComponents = 128; ///< components one spawn of a prefab hierarchy makes

/// A child entry of a prefab (spec 022 FR-16): made by nnh::child().
struct PrefabChild {
    Prefab prefab{};
    Transform2D local{};    ///< replaces the child prefab's own nnh::Node local in this slot of the hierarchy
    bool has_local = false; ///< else the child prefab's own Node local
    const detail::PrefabTreeOps* tree = nullptr; ///< set by child(): links the tree code only for games that use it
};
/// `.children = {nnh::child(gun, {.position = {12, -4}, .rotation = 0.1f})}`: `prefab` (registered earlier in the same
/// library, with the nnh::Node kind) spawned as a child, in entry order, with its own local or `local`.
inline PrefabChild child(Prefab prefab) { return {prefab, {}, false, &detail::kPrefabTreeOps}; }
inline PrefabChild child(Prefab prefab, const Transform2D& local) {
    return {prefab, local, true, &detail::kPrefabTreeOps};
}

/// A prefab's name as its FNV-1a 32 hash (the hash of tags, data paths and sprite names): the same bits from a literal
/// at compile time and from a string at run time.
enum class PrefabName : std::uint32_t {};
constexpr PrefabName prefab_name(const char* name) { return PrefabName{detail::fnv1a32(name)}; }
constexpr PrefabName prefab_name(const char* text, std::size_t size) {
    return PrefabName{detail::fnv1a32(text, size)};
}

/// What a spawn hook knows about the copy it runs for.
struct SpawnInfo {
    bool has_position = false;
    Vec2 position{};                   ///< the position given to the spawn (a map object's x, y)
    const MapObject* object = nullptr; ///< copies made from a map: the object, valid during the hook only
    int object_index = -1;             ///< its index in the map (Tilemap::object(i))
};

/// A spawn hook: `[this](nnh::World& w, nnh::Entity e, const nnh::SpawnInfo& info) { ... }` (captures ≤ 16 bytes,
/// trivially copyable, as spec 020's remove hooks). Called once per copy, after its components, placement and tags,
/// before spawn returns. It may spawn, destroy (the new entity too), add and remove: spec 020's rules apply.
using SpawnHook = detail::SmallFunction<void(World&, Entity, const SpawnInfo&)>;

/// How a prefab is registered, besides its component values.
struct PrefabOptions {
    Prefab base{};                 ///< a variant of this prefab (registered earlier in the same library)
    Tag tags[kMaxPrefabTags] = {}; ///< `.tags = {kEnemy, kFlying}`; the first kNoTag ends the list
    Placement place{};             ///< `.place = nnh::place(&Body::box)`; a variant inherits its base's
    SpawnHook on_spawn{};          ///< runs after the base's hooks
    /// Spec 022: child prefabs spawned with this one as a hierarchy (the first null prefab ends the list). The prefab
    /// and every child need the nnh::Node kind; a variant inherits its base's children and appends these.
    /// No `= {}` here: every PrefabChild member has its own default, and with one GCC 13 and 14 reject a braced
    /// `{.tags = ...}` argument for add()'s options (an array of default-initialised aggregates in a template call).
    PrefabChild children[kMaxPrefabChildren];
};

/// What a spawn does when a capacity it needs is full (FR-8). Either way nothing is created and the library counts a
/// miss (and the world its own: its spawn misses, or the full kind's).
enum class WhenFull : std::uint8_t {
    Assert, ///< logs an error naming the prefab and asserts in Debug/Development, as a full world (the default)
    Miss,   ///< quiet: budgeted spawns (touch spawns, effects)
};

/// A library's capacities, fixed at creation.
struct PrefabLibraryDesc {
    int max_prefabs = 0;                ///< 1..1024
    int max_entries = 0;                ///< component entries over all prefabs, 0..65535 (a variant's count)
    int value_bytes = 0;                ///< stored values with their alignment, 0..1 MiB (variants: their own only)
    WhenFull when_full = WhenFull::Assert;
    MemTag* memory_tag = nullptr;       ///< default: the engine's `prefabs`
    int max_children = 0;               ///< child entries over all prefabs, 0..65535 (0: no hierarchies, spec 021's layout)
};

/// Counts since creation, and what is used of each capacity.
struct PrefabStats {
    int prefabs = 0, capacity = 0;
    int entries = 0, entry_capacity = 0;
    int value_bytes = 0, value_capacity = 0;
    int spawns = 0, misses = 0, map_skipped = 0, map_unknown = 0;
};

/// spawn_map_objects() / check_map_objects() options: one object layer, or −1 = every layer.
struct MapSpawnOptions {
    int object_layer = -1;
};
/// What a map call did: objects spawned; refused by a full capacity (FR-8); whose type names no prefab (skipped:
/// doors, spawn points, empty types); whose explicit `prefab` property names no prefab (level errors) and the first
/// such object's index.
struct MapSpawnSummary {
    int spawned = 0, misses = 0, skipped = 0, unknown = 0, first_unknown = -1;
};

/// Prefabs in one block of tagged memory sized at creation (FR-1): nothing allocates afterwards, nothing is
/// unregistered or edited. Movable, not copyable. Empty (default-constructed or moved-from) it asserts on commands and
/// answers nothing to queries. Memory (3DS): ≈ 64 B + per prefab 80 B (+ 32 B for its name in Debug/Development), per
/// entry 16 B, plus the values. No virtual call, no allocation, no event (FR-12, FR-14).
class PrefabLibrary {
public:
    PrefabLibrary() = default;
    /// One block for `desc`'s capacities. Errors: InvalidArgument (a capacity out of range), OutOfMemory.
    static Result<PrefabLibrary> create(const PrefabLibraryDesc& desc);
    /// Destroys the stored values and frees the block. Inside one of its own spawn hooks: asserts (Release: freed when
    /// the outermost spawn returns).
    ~PrefabLibrary();
    PrefabLibrary(PrefabLibrary&& other) noexcept;
    PrefabLibrary& operator=(PrefabLibrary&& other) noexcept;
    PrefabLibrary(const PrefabLibrary&) = delete;
    PrefabLibrary& operator=(const PrefabLibrary&) = delete;

    // ---- Registration (load time; FR-2..FR-4, FR-10) ----

    /// Registers `name` with `values` (component structs, in entry order), copied into the block:
    /// `lib.add("coin", {.tags = {kCoin}, .place = nnh::place(&Body::box)}, Body{{0, 0, 12, 12}, kCoinLayer}, Coin{1})`.
    /// A variant (`.base`) starts with its base's entries, tags, placement and hooks; its values replace the base's
    /// of the same kind in place or are appended. Errors, with nothing registered: InvalidArgument (null or empty name,
    /// a name whose hash is registered, a kind twice among the values, a base not of this library, a placement kind
    /// not among the entries or a member past its end, kNoTag before another tag), LimitReached (32 entries, 8 tags
    /// or 8 hooks per prefab; a library capacity). Each value type: a class, copy-constructible, alignof ≤ 16 (compile-
    /// time checks). Load time: copies, never allocates.
    template <typename... T>
        requires((std::is_class_v<T> && !std::is_same_v<T, ComponentValue>) && ...)
    Result<Prefab> add(const char* name, const PrefabOptions& options, const T&... values) {
        if constexpr (sizeof...(T) == 0) {
            return add(name, options, static_cast<const ComponentValue*>(nullptr), 0);
        } else {
            const ComponentValue list[] = {component_value(values)...};
            return add(name, options, list, static_cast<int>(sizeof...(T)));
        }
    }
    /// The array form: `count` values made by component_value().
    Result<Prefab> add(const char* name, const PrefabOptions& options, const ComponentValue* values, int count);

    /// The prefab registered under a name, or null. No allocation; a hash and a table probe.
    Prefab find(PrefabName name) const;
    Prefab find(const char* name) const; ///< hashes in one pass (the same bits as prefab_name)
    Prefab find(const char* text, std::size_t size) const;

    // ---- Spawning (FR-5..FR-8) ----

    /// One entity, bit for bit what spawn() + add<T>(e, value) per entry + add_tag per tag would make, then the hooks
    /// (base first). Null when refused (FR-8: see WhenFull; inside world.clear(): null, no assert, no miss). May return
    /// a stale handle (a hook destroyed the copy). No allocation, no virtual call; per-frame safe.
    Entity spawn(World& world, Prefab prefab);
    /// The same with `at` written into the placement member (asserts without one, and spawns without the position).
    Entity spawn(World& world, Prefab prefab, Vec2 at);
    /// With component overrides (FR-7): each replaces the prefab's value of its kind for this copy (constructed once,
    /// from the override) or adds a kind the prefab doesn't have (after the prefab's entries). Each override type at
    /// most once, and never a bare nnh::Vec2 (compile-time checks).
    template <typename T, typename... More>
        requires(!std::is_same_v<T, Vec2> && std::is_class_v<T> && (std::is_class_v<More> && ...))
    Entity spawn_with(World& world, Prefab prefab, const T& override_value, const More&... more) {
        check_overrides<T, More...>();
        const ComponentValue list[] = {component_value(override_value), component_value(more)...};
        return spawn_with(world, prefab, static_cast<const Vec2*>(nullptr), list, 1 + static_cast<int>(sizeof...(More)));
    }
    template <typename T, typename... More>
        requires(std::is_class_v<T> && (std::is_class_v<More> && ...))
    Entity spawn_with(World& world, Prefab prefab, Vec2 at, const T& override_value, const More&... more) {
        check_overrides<T, More...>();
        const ComponentValue list[] = {component_value(override_value), component_value(more)...};
        return spawn_with(world, prefab, &at, list, 1 + static_cast<int>(sizeof...(More)));
    }
    /// The array form (`at` may be null). A kind given twice asserts; the second is ignored.
    Entity spawn_with(World& world, Prefab prefab, const Vec2* at, const ComponentValue* overrides, int count);

    // ---- Hierarchies (spec 022 FR-17, FR-18; the hierarchy code is linked by these and by nnh::child()) ----

    /// The prefab (with or without children) spawned as the last child of node `parent` of the same world, attached
    /// keep-local; `at` (when given) is written into the placement member and is local to `parent`. All or nothing; a
    /// hierarchy that would be deeper than kMaxHierarchyDepth under `parent` is refused as a full capacity (WhenFull).
    Entity spawn_under(World& world, Prefab prefab, Entity parent);
    Entity spawn_under(World& world, Prefab prefab, Entity parent, Vec2 at);

    // ---- Map objects (FR-9) ----

    /// Every object (or one layer's) in map order: its string property `prefab` names the prefab, else its type;
    /// spawned at its bounds through the placement member (a Rect member also gets a rectangle object's w, h unless
    /// the placement keeps its size), with the object in SpawnInfo. An explicit name that isn't registered is a level
    /// error: logged (`object 12 "coin" (layer things): no prefab "coinn"`) in every flavour, asserted in
    /// Debug/Development. Load time (one call per map object; nothing allocates).
    MapSpawnSummary spawn_map_objects(World& world, const Tilemap& map, const MapSpawnOptions& options = {});
    /// The same summary without spawning, logging or asserting (`spawned` = what would be spawned, `misses` = 0).
    MapSpawnSummary check_map_objects(const Tilemap& map, const MapSpawnOptions& options = {}) const;

    // ---- Queries (FR-11) ----

    int count() const;
    int capacity() const;
    /// Registered here (not null, in range, this library's stamp).
    bool valid(Prefab prefab) const;
    /// Debug/Development: the registered name (≤ 31 bytes); Release (no names stored) and invalid prefabs: "".
    const char* name(Prefab prefab) const;
    /// Writes up to `capacity` kinds in entry order and returns how many the prefab has.
    int kinds(Prefab prefab, KindId* out, int capacity) const;
    /// Writes up to `capacity` tags in order and returns how many the prefab has.
    int tags(Prefab prefab, Tag* out, int capacity) const;
    /// Writes up to `capacity` child prefabs in entry order (a variant's inherited ones first) and returns how many.
    int children(Prefab prefab, Prefab* out, int capacity) const;
    template <typename T>
    bool has(Prefab prefab) const {
        return stored_value(prefab, kind_id<T>()) != nullptr;
    }
    /// The prefab's stored value of kind T (a variant may read its base's), read-only; null when it has none.
    template <typename T>
    const T* value(Prefab prefab) const {
        return static_cast<const T*>(stored_value(prefab, kind_id<T>()));
    }
    PrefabStats stats() const;

private:
    friend struct PrefabAccess;

    template <typename... T>
    static constexpr void check_overrides() {
        static_assert(((!std::is_same_v<T, Vec2>) && ...), "nnh::PrefabLibrary::spawn_with: a bare nnh::Vec2 "
                                                            "override reads as a position: wrap it in a component");
        static_assert(detail::distinct_types<T...>(), "nnh::PrefabLibrary::spawn_with: the same override type twice");
    }
    const void* stored_value(Prefab prefab, KindId kind) const;

    detail::PrefabBlock* block_ = nullptr;
};

} // namespace nnh
