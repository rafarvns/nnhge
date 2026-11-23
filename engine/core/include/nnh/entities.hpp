#pragma once
/// @file nnh/entities.hpp
/// Entities, components and tags (spec 020, decision D35): a World of entities whose components are the game's own
/// plain structs, stored kind by kind in fixed arrays with stable slots, found by generational handles and by tags
/// hashed at compile time. See docs/api/entities.md.
///
/// Optional (constitution XI): a game that doesn't include this header links none of it. Light: no standard
/// containers, no <functional>; it doesn't include <nnh/pool.hpp>, collision, tweens, animation or graphics (they meet
/// through the data: a storage view is a plain array a collision::box_view reads in place).
///
/// @code
/// struct Body { nnh::Rect box; nnh::collision::Layers layers = nnh::collision::kNoLayers; };
/// struct Velocity { nnh::Vec2 v; };
/// const nnh::ComponentDesc kinds[] = {nnh::component<Body>(256), nnh::component<Velocity>(256)};
/// nnh::World world = std::move(nnh::World::create({.max_entities = 256}, kinds).value()); // check it in real code
/// nnh::Entity e = world.spawn();
/// world.add<Body>(e, {{10, 10, 16, 16}, kEnemy});
/// world.add<Velocity>(e, {{1, 0}});
/// world.add_tag(e, nnh::tag("enemy"));
/// world.each<Body, Velocity>([](nnh::Entity, Body& b, Velocity& v) { b.box.x += v.v.x; });
/// @endcode

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "nnh/detail/handle.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/detail/slot_bits.hpp"
#include "nnh/detail/small_function.hpp"
#include "nnh/detail/type_id.hpp"
#include "nnh/detail/world_layout.hpp"
#include "nnh/detail/world_ranges.hpp"
#include "nnh/result.hpp"

namespace nnh {

class Scene;  // <nnh/scene.hpp>
struct MemTag; // <nnh/memory.hpp>

// ---- Tags (FR-16) ----

/// A tag: the FNV-1a 32 hash of its name (the hash of data paths and sprite names). Comparing tags is comparing
/// integers; the engine stores no names. Tag::None (0) is never a tag.
enum class Tag : std::uint32_t { None = 0 };

/// The tag of a name, at compile time from a literal (`constexpr nnh::Tag kEnemy = nnh::tag("enemy");`, usable as a
/// `case` label) or at run time from a string you read (a Tiled object's type): the same bits either way.
constexpr Tag tag(const char* name) { return Tag{detail::fnv1a32(name)}; }
/// The tag of the first `size` bytes of `text` (not NUL-terminated buffers).
constexpr Tag tag(const char* text, std::size_t size) { return Tag{detail::fnv1a32(text, size)}; }

/// True when every tag differs from the others and from Tag::None — the way to rule out a hash collision among your
/// tags: `static_assert(nnh::tags_distinct(kEnemy, kCoin, kFlying));`.
template <typename... Tags>
constexpr bool tags_distinct(Tags... tags) {
    const Tag list[] = {Tag::None, tags...};
    constexpr int n = static_cast<int>(sizeof...(Tags)) + 1;
    for (int i = 1; i < n; ++i)
        for (int j = 0; j < i; ++j)
            if (list[i] == list[j]) return false;
    return true;
}

// ---- Component kinds (FR-10) ----

/// A component kind's registration: filled by component<T>(). Its fields are what a future prefab needs to build a
/// component by kind (size, alignment, construct, destroy).
struct ComponentDesc {
    detail::TypeId id = nullptr;
    int size = 0;
    int align = 0;
    int capacity = 0;
    void (*construct)(void*) = nullptr; ///< placement-new of a T{}
    void (*destroy)(void*) = nullptr;   ///< ~T(); null when T is trivially destructible
    detail::SmallFunction<void(World&, Entity, void*)> on_remove{};
};

namespace detail {
template <typename T>
void construct_component(void* p) {
    ::new (p) T{};
}
template <typename T>
void destroy_component(void* p) {
    static_cast<T*>(p)->~T();
}
} // namespace detail

/// Registers kind T with room for `capacity` components (1..max_entities). T: any default-constructible type with a
/// non-throwing destructor; components never move (no copy or move needed). Free slots hold a T{}: give a layers
/// member the default `collision::kNoLayers` and free slots are invisible to box views.
template <typename T>
ComponentDesc component(int capacity) {
    static_assert(std::is_default_constructible_v<T>, "nnh::component<T>: T must be default-constructible");
    static_assert(std::is_nothrow_destructible_v<T>, "nnh::component<T>: T's destructor must not throw");
    ComponentDesc d;
    d.id = detail::type_id<T>();
    d.size = static_cast<int>(sizeof(T));
    d.align = static_cast<int>(alignof(T));
    d.capacity = capacity;
    d.construct = &detail::construct_component<T>;
    d.destroy = std::is_trivially_destructible_v<T> ? nullptr : &detail::destroy_component<T>;
    return d;
}

/// The same with a remove hook, called when a T component goes (remove<T>, destroy, clear) while it is still
/// attached and readable: `nnh::component<Body>(256, [this](nnh::World& w, nnh::Entity e, Body& b) { ... })`.
/// Captures ≤ 16 bytes, trivially copyable (`[this]`), as event handlers. Called once per component removed.
template <typename T, typename F>
ComponentDesc component(int capacity, F&& on_remove) {
    using Fn = std::decay_t<F>;
    static_assert(std::is_invocable_v<const Fn&, World&, Entity, T&>,
                  "nnh::component<T>(capacity, hook): the hook takes (nnh::World&, nnh::Entity, T&)");
    ComponentDesc d = component<T>(capacity);
    d.on_remove = [fn = Fn(static_cast<F&&>(on_remove))](World& w, Entity e, void* p) { fn(w, e, *static_cast<T*>(p)); };
    return d;
}

/// A world's capacities.
struct WorldDesc {
    int max_entities = 0;          ///< 1..65535
    int tags_per_entity = 4;       ///< 0..8 (16 bytes per entity at 4)
    MemTag* memory_tag = nullptr;  ///< default: the engine's `entities`
};

/// Entities live, highest live count, capacity; spawns and adds refused by full capacities (since creation).
struct WorldStats {
    int entities = 0;
    int peak = 0;
    int capacity = 0;
    int spawn_misses = 0;
    int add_misses = 0;
};
/// One kind's live components, highest count, capacity and adds refused (since creation).
struct KindStats {
    int live = 0;
    int peak = 0;
    int capacity = 0;
    int misses = 0;
};

namespace detail {

inline constexpr int kAddedExisting = 0x10000; ///< world_add(): the entity already had the kind

int world_add(WorldHot& w, int kind, Entity e); ///< a slot, kAddedExisting | slot, or −1
bool world_remove(World& world, int kind, Entity e);
bool world_has_tag(const WorldHot& w, Entity e, Tag t);

} // namespace detail

/// A view of one kind's storage (FR-15), with the kind lookup done once: a plain array other modules read in place
/// (`collision::box_view(bodies.data(), bodies.extent(), &Body::box, &Body::layers)`; a result's box index is a slot,
/// `owner(index)` its entity). Non-live slots hold T{}. Valid for the world's life; cheap to copy. Its get() is the
/// fastest lookup by handle: it keeps the world's record layout in the view (no kind scan, no header loads).
template <typename T>
class Storage {
public:
    Storage() = default;
    Storage(detail::WorldHot* w, int kind)
        : w_(w), kind_(kind), records_(w->records), record_words_(static_cast<std::uint32_t>(w->record_words)),
          entities_(static_cast<std::uint32_t>(w->entities.capacity)),
          slot_field_(static_cast<std::uint32_t>(2 * w->slots_word + kind)), data_(static_cast<T*>(w->kinds[kind].data)) {}

    T* data() const { return w_ != nullptr ? static_cast<T*>(k().data) : nullptr; }
    /// One past the highest slot used since the last clear().
    int extent() const { return w_ != nullptr ? k().slots.extent : 0; }
    static constexpr int stride() { return static_cast<int>(sizeof(T)); }
    bool is_live(int slot) const {
        return w_ != nullptr && slot >= 0 && slot < k().slots.capacity && k().slots.is_live(slot);
    }
    /// The entity owning a live slot; null for a non-live or out-of-range slot.
    Entity owner(int slot) const { return is_live(slot) ? detail::entity_at(*w_, k().owner[slot]) : Entity{}; }
    /// The slot of `e`'s component, or −1 when `e` is stale or lacks T.
    int slot_of(Entity e) const {
        const std::uint32_t slot = e.bits() & 0xFFFFu; // an empty view has entities_ 0: every handle is out of range
        if (slot >= entities_) return -1;
        const std::uint32_t* r = records_ + slot * record_words_;
        if (r[0] != ((e.bits() >> 16) | detail::kEntityLive)) return -1;
        const std::uint16_t s = reinterpret_cast<const std::uint16_t*>(r)[slot_field_];
        return s == detail::kNoSlot ? -1 : s;
    }
    /// As World::get<T>, without the kind lookup or a load of the world's header.
    T* get(Entity e) const {
        const int s = slot_of(e);
        return s < 0 ? nullptr : data_ + s;
    }
    /// Every live component in ascending slots: `for (auto [entity, component] : view.items())` (FR-15's rules).
    detail::StorageRange<T> items() const { return detail::StorageRange<T>(w_, kind_); }

    int count() const { return w_ != nullptr ? k().slots.count : 0; }
    int capacity() const { return w_ != nullptr ? k().slots.capacity : 0; }
    KindStats stats() const {
        if (w_ == nullptr) return {};
        return {k().slots.count, k().slots.peak, k().slots.capacity, k().slots.misses};
    }

private:
    const detail::KindHot& k() const { return w_->kinds[kind_]; }
    detail::WorldHot* w_ = nullptr;
    int kind_ = 0;
    // The world's record layout, fixed for its life (the block never moves): what get() reads.
    const std::uint32_t* records_ = nullptr;
    std::uint32_t record_words_ = 0;
    std::uint32_t entities_ = 0;
    std::uint32_t slot_field_ = 0; ///< index of this kind's u16 slot field from the record's start
    T* data_ = nullptr;
};

/// Entities with components and tags, in one block of tagged memory sized at creation (FR-9). Nothing allocates
/// afterwards; no virtual calls; the same calls give the same handles and visiting orders on every system (FR-21).
/// Movable while unbound, not copyable. Empty (default-constructed or moved-from) it asserts on commands and answers
/// nothing to queries. Memory: ≈ 64 + 52·kinds B, per entity 8 + 4·tags + 2·kinds (rounded to 4) + 2.5 B, per
/// component slot sizeof(T) + 2.5 B (+ one scratch T per kind).
class World {
public:
    World() = default;
    /// One block for `desc` and `kinds` (an array: `const nnh::ComponentDesc kinds[] = {...}`). Errors:
    /// InvalidArgument (capacities, tags, 0 or more than 32 kinds, a kind twice, a kind capacity outside
    /// 1..max_entities), OutOfMemory. Constructs a T{} in every component slot.
    template <int K>
    static Result<World> create(const WorldDesc& desc, const ComponentDesc (&kinds)[K]) {
        return create(desc, kinds, K);
    }
    static Result<World> create(const WorldDesc& desc, const ComponentDesc* kinds, int count);
    /// Destroys every entity (remove hooks run, ascending slots), detaches from its scene, frees the block.
    ~World();
    World(World&& other) noexcept;
    World& operator=(World&& other) noexcept; ///< moving a bound world asserts and moves nothing
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    /// Binds the world to `scene` (D30): flushed right before the scene's update() (only while it is the top scene),
    /// cleared right after its on_exit() (hooks run; test clearing() in them to skip effects). Bind it before the
    /// tween sets its hooks use. Keep the world a member of that scene.
    void bind(Scene& scene);
    bool bound() const;

    // ---- Entities (FR-11, FR-13) ----

    /// A new entity in the lowest free slot, with no components and no tags. A full world returns the null handle
    /// and asserts (Debug/Development): the capacity is the game's budget. Inside clear(): null, no assert.
    Entity spawn();
    /// Runs the remove hooks of `e`'s components (registration order, data intact), then destroys its components
    /// (slots hold T{}, quarantined until the flush), removes its tags and makes `e` stale. Safe anywhere, including
    /// iterations and hooks (from a hook it is queued and finished before the outermost call returns). False when
    /// `e` is stale or already being destroyed.
    bool destroy(Entity e);
    /// True while `e` names a live entity (also during its own remove hooks).
    bool alive(Entity e) const { return hot_ != nullptr && detail::live_record(*hot_, e) != nullptr; }

    // ---- Components (FR-12) ----

    /// Adds a T (the slot's T{}) to live entity `e` and returns it. Null when `e` is stale; asserts and returns null
    /// for a full kind or an unregistered T; asserts and returns the existing component when `e` already has a T.
    template <typename T>
    T* add(Entity e) {
        int k = -1;
        const int s = add_slot<T>(e, k);
        return s < 0 ? nullptr : static_cast<T*>(hot_->kinds[k].data) + (s & 0xFFFF);
    }
    /// The same, with the component copied from `value` (an existing component is left unchanged).
    template <typename T>
    T* add(Entity e, const T& value) {
        int k = -1;
        const int s = add_slot<T>(e, k);
        if (s < 0) return nullptr;
        T* p = static_cast<T*>(hot_->kinds[k].data) + (s & 0xFFFF);
        if (s < detail::kAddedExisting) {
            p->~T();
            ::new (static_cast<void*>(p)) T(value);
        }
        return p;
    }
    /// `e`'s T, or null (stale entity, no T). Constant time: no search. The address never changes while it exists.
    template <typename T>
    T* get(Entity e) {
        const detail::WorldHot* w = hot_;
        if (w == nullptr) return nullptr;
        // Straight-line and call-free in Release (the assert's call is [[noreturn]]): every header field is read
        // before a branch, so a loop of lookups keeps them in registers (plan 020, ARM pre-check). An unknown kind
        // reads the records' spare slot field (always kNoSlot): null without a branch of its own.
        int k = detail::kind_index<T>(*w);
        NNH_ASSERT(k >= 0, "nnh::World::get<T>(): a component kind the world doesn't store");
        if (k < 0) k = w->kind_count;
        T* const data = static_cast<T*>(w->kind_data[k]);
        const int slots_word = w->slots_word;
        std::uint32_t* r = detail::live_record(*w, e);
        if (r == nullptr) return nullptr;
        const std::uint16_t s = reinterpret_cast<const std::uint16_t*>(r + slots_word)[k];
        return s == detail::kNoSlot ? nullptr : data + s;
    }
    template <typename T>
    const T* get(Entity e) const {
        return const_cast<World*>(this)->get<T>(e);
    }
    template <typename T>
    bool has(Entity e) const {
        return get<T>(e) != nullptr;
    }
    /// Runs T's remove hook, then destroys the component (its slot holds T{}, quarantined until the flush). False
    /// when `e` is stale, lacks T, is being destroyed, or T's hook is already running for it.
    template <typename T>
    bool remove(Entity e) {
        if (hot_ == nullptr) return false;
        const int k = kind<T>();
        return k >= 0 && detail::world_remove(*this, k, e);
    }
    /// Checked access for an entity you know has a T: asserts otherwise (Debug/Development); in Release returns the
    /// kind's scratch T{} (re-made each time), never another component.
    template <typename T>
    T& at(Entity e) {
        if (T* p = get<T>(e)) return *p;
        detail::world_misuse("World::at<T>(): the entity is stale or has no such component");
        const int k = hot_ != nullptr ? kind<T>() : -1;
        if (k < 0) {
            static T empty{};
            return empty;
        }
        return *static_cast<T*>(detail::world_scratch(*hot_, k));
    }
    /// A view of T's storage, the kind lookup done once (FR-15).
    template <typename T>
    Storage<T> storage() {
        if (hot_ == nullptr) {
            detail::world_misuse("use of an empty or moved-from nnh::World");
            return {};
        }
        const int k = kind<T>();
        return k < 0 ? Storage<T>() : Storage<T>(hot_, k);
    }

    // ---- Iteration (FR-15) ----

    /// fn(Entity, A&, B&, ...) for every live entity that has all the listed kinds, in ascending slots of A's
    /// storage (list the rarest kind first). Destroying, removing, spawning and adding inside are safe: what is
    /// destroyed or removed is not visited afterwards, what is created is not visited. Inlined; allocates nothing.
    template <typename A, typename... B, typename F>
    void each(F&& fn) {
        each_in<A, B...>(fn, std::index_sequence_for<B...>{});
    }
    /// Every live entity in ascending slots (the same rules): `for (nnh::Entity e : world.entities())`.
    detail::EntityRange entities() {
        if (hot_ == nullptr) detail::world_misuse("use of an empty or moved-from nnh::World");
        return detail::EntityRange(hot_);
    }

    // ---- Tags (FR-17) ----

    /// Adds `t` to live `e`. False when `e` is stale or already has it; asserts and returns false when `e` already
    /// has the world's tags_per_entity tags, or for Tag::None.
    bool add_tag(Entity e, Tag t);
    bool remove_tag(Entity e, Tag t);
    bool has_tag(Entity e, Tag t) const { return hot_ != nullptr && detail::world_has_tag(*hot_, e, t); }
    /// Writes the live entities tagged `t` in ascending slots into `out` (up to `capacity`) and returns how many
    /// there are (may exceed `capacity`; a null `out` with capacity 0 just counts).
    int find(Tag t, Entity* out, int capacity) const;
    /// The lowest-slot live entity tagged `t`, or null.
    Entity first(Tag t) const;
    /// fn(Entity) for every live entity tagged `t`, ascending, with each()'s rules — Godot's "call group".
    template <typename F>
    void each_tagged(Tag t, F&& fn) {
        if (hot_ == nullptr) {
            detail::world_misuse("use of an empty or moved-from nnh::World");
            return;
        }
        detail::WorldHot& w = *hot_;
        detail::IterationGuard guard(&w);
        for (detail::BitCursor c(&w, &w.entities, detail::words_for(w.entities.extent)); c.slot() >= 0; c.next()) {
            const std::uint32_t* r = detail::record_at(w, static_cast<std::uint32_t>(c.slot()));
            bool tagged = false;
            for (int i = 0; i < w.tags; ++i) tagged = tagged || r[2 + i] == static_cast<std::uint32_t>(t);
            if (tagged) fn(detail::entity_at(w, c.slot()));
        }
    }

    // ---- Frames (FR-14) ----

    /// Quarantined entity and component slots become free. Bound worlds are flushed by their scene; unbound ones by
    /// the game once per frame. Inside an iteration or a hook: asserts, ignored.
    void flush();
    /// Destroys every entity (hooks run, ascending slots; spawns refused meanwhile) and flushes: a level rebuilt
    /// after clear() gets the slots of its first load. Inside an iteration or a hook: asserts, ignored.
    void clear();
    /// True inside clear() (also the scene's): remove hooks test it to skip effects.
    bool clearing() const;

    WorldStats stats() const;
    int count() const { return hot_ != nullptr ? hot_->entities.count : 0; }
    int capacity() const { return hot_ != nullptr ? hot_->entities.capacity : 0; }

private:
    friend struct WorldAccess;

    template <typename T>
    int kind() const {
        const int k = detail::kind_index<T>(*hot_);
        if (k < 0) detail::world_misuse("nnh::World: a component kind it doesn't store (register it in create())");
        return k;
    }
    template <typename T>
    int add_slot(Entity e, int& k) {
        if (hot_ == nullptr) {
            detail::world_misuse("use of an empty or moved-from nnh::World");
            return -1;
        }
        k = kind<T>();
        return k < 0 ? -1 : detail::world_add(*hot_, k, e);
    }

    template <typename A, typename... B, typename F, std::size_t... I>
    void each_in(F& fn, std::index_sequence<I...>) {
        if (hot_ == nullptr) {
            detail::world_misuse("use of an empty or moved-from nnh::World");
            return;
        }
        const int ka = kind<A>();
        const int kb[] = {kind<B>()..., 0};
        if (ka < 0) return;
        for (int i = 0; i < static_cast<int>(sizeof...(B)); ++i)
            if (kb[i] < 0) return;
        detail::each_with<A, B...>(*hot_, ka, kb, fn, std::index_sequence<I...>{});
    }

    detail::WorldHot* hot_ = nullptr;
};

} // namespace nnh
