#pragma once
/// @file nnh/pool.hpp
/// Object pools (spec 020, decision D35): up to a fixed number of the game's own objects, handed out from the lowest
/// free slot and taken back, with generational handles that tell a stale reference from a live one. See
/// docs/api/pool.md.
///
/// Optional (constitution XI): a game that doesn't include this header links none of it. Light: no standard
/// containers, no <functional>; it doesn't include <nnh/entities.hpp> (worlds have their own header).
///
/// @code
/// nnh::Pool<Bullet, 32> bullets_{*this};            // a scene member, bound: flushed and cleared by the scene
/// if (auto [h, b] = bullets_.acquire(); h) b->velocity = {2, 0};   // full: null handle, a miss in stats()
/// for (auto [h, b] : bullets_.items()) if (b.gone) bullets_.release(h); // safe inside the loop
/// @endcode

#include <cstddef>
#include <cstdint>
#include <new>

#include "nnh/detail/handle.hpp"
#include "nnh/detail/pool_core.hpp"
#include "nnh/detail/slot_bits.hpp"
#include "nnh/result.hpp"

namespace nnh {

class Scene;  // <nnh/scene.hpp>
struct MemTag; // <nnh/memory.hpp>

inline constexpr int kMaxPoolCapacity = 65535;
inline constexpr int kRuntimeCapacity = 0; ///< Pool<T>: the capacity is chosen by create()

/// A pool's counters: live objects, the highest live count and the full-pool acquires since creation or clear().
struct PoolStats {
    int live = 0;
    int peak = 0;
    int capacity = 0;
    int misses = 0;
};

/// What acquire() returns: `if (auto [h, b] = pool.acquire(); h) ...`. Both null when the pool is full.
template <typename T>
struct Acquired {
    Handle<T> handle{};
    T* object = nullptr;
};

namespace detail {

/// The pool interface both forms share, over their PoolCore (CRTP: `D::core()` returns it, or null when empty).
template <typename T, typename D>
class PoolApi {
public:
    /// Takes the lowest free slot (which holds a T{}) and returns its handle and object. A full pool returns
    /// {null, nullptr} and counts a miss (no assert: running out is normal play). Allocates nothing. Inside an
    /// iteration the new object is not visited by it.
    Acquired<T> acquire() {
        PoolCore* c = live_core();
        if (c == nullptr) return {};
        const int slot = PoolOps<T>::take(*c);
        if (slot < 0) return {};
        return {PoolOps<T>::handle(*c, slot), PoolOps<T>::data(*c) + slot};
    }
    /// The same, with the slot's object copied from `value`.
    Acquired<T> acquire(const T& value) {
        Acquired<T> a = acquire();
        if (a.object != nullptr) {
            a.object->~T();
            ::new (static_cast<void*>(a.object)) T(value);
        }
        return a;
    }
    /// The same, with the slot's object moved from `value`.
    Acquired<T> acquire(T&& value) {
        Acquired<T> a = acquire();
        if (a.object != nullptr) {
            a.object->~T();
            ::new (static_cast<void*>(a.object)) T(static_cast<T&&>(value));
        }
        return a;
    }
    /// Destroys the object, leaves a T{} in its slot (a pointer you kept reads that, never freed memory) and makes
    /// every handle to it stale at once. The slot is quarantined: not reused until the next flush(). False (nothing
    /// changes) for a stale or null handle — releasing twice in a frame is normal play.
    bool release(Handle<T> h) {
        PoolCore* c = self().core();
        return c != nullptr && PoolOps<T>::release(*c, h);
    }
    /// The object, or null when `h` is stale or null. Constant time.
    T* get(Handle<T> h) {
        PoolCore* c = self().core();
        return c != nullptr ? PoolOps<T>::get(*c, h) : nullptr;
    }
    const T* get(Handle<T> h) const { return const_cast<PoolApi*>(this)->get(h); }
    bool alive(Handle<T> h) const { return get(h) != nullptr; }
    /// Checked access for a handle you know is live: a stale one asserts in Debug/Development; in Release it returns
    /// a scratch T{} (re-made on each such call), never another object.
    T& at(Handle<T> h) {
        if (T* object = get(h)) return *object;
        pool_misuse("Pool::at() with a stale or null handle");
        PoolCore* c = self().core();
        if (c == nullptr) {
            static T empty{};
            return empty;
        }
        return PoolOps<T>::scratch(*c);
    }
    /// Quarantined slots become free. A bound pool is flushed by its scene; an unbound one by the game, once per
    /// frame (typically at the start of its update). Inside one of the pool's iterations: asserts, ignored.
    void flush() {
        if (PoolCore* c = live_core()) pool_flush(*c);
    }
    /// Releases every object (ascending slots) and flushes: the next acquires start at slot 0 again (a level
    /// restart gets the slots of its first load). Stats are reset. Inside an iteration: asserts, ignored.
    void clear() {
        if (PoolCore* c = live_core()) PoolOps<T>::clear(*c);
    }

    /// Binds the pool to `scene` (D30): flushed right before the scene's update() (only while it is the top scene),
    /// cleared right after its on_exit(). Keep it a member of that scene. Twice, or while iterating: asserts.
    void bind(Scene& scene) {
        if (PoolCore* c = live_core()) pool_bind(*c, scene);
    }
    bool bound() const {
        const PoolCore* c = const_cast<PoolApi*>(this)->self().core();
        return c != nullptr && c->scene != nullptr;
    }

    /// Every live object in ascending slots: `for (auto [h, object] : pool.items())`. Releasing any object inside
    /// is safe (it isn't visited afterwards); objects acquired inside are not visited. `break` is fine.
    PoolRange<T> items() { return PoolRange<T>(live_core()); }
    /// fn(Handle<T>, T&) for every live object, ascending, with items()' rules; the callable is inlined.
    template <typename F>
    void each(F&& fn) {
        if (PoolCore* c = live_core()) PoolOps<T>::each(*c, fn);
    }

    /// The storage as a plain array for other modules (spec 019 box views): slot 0, one past the highest slot used
    /// since the last clear(), the element stride. Non-live slots hold T{}.
    T* data() {
        PoolCore* c = self().core();
        return c != nullptr ? PoolOps<T>::data(*c) : nullptr;
    }
    int extent() const { return field(&SlotTable::extent); }
    static constexpr int stride() { return static_cast<int>(sizeof(T)); }
    bool is_live(int slot) const {
        const PoolCore* c = const_cast<PoolApi*>(this)->self().core();
        return c != nullptr && slot >= 0 && slot < c->slots.capacity && c->slots.is_live(slot);
    }
    /// The handle of a live slot; null for a non-live or out-of-range slot.
    Handle<T> handle_at(int slot) const {
        if (!is_live(slot)) return {};
        return PoolOps<T>::handle(*const_cast<PoolApi*>(this)->self().core(), slot);
    }

    int count() const { return field(&SlotTable::count); }
    int capacity() const { return field(&SlotTable::capacity); }
    PoolStats stats() const {
        return {field(&SlotTable::count), field(&SlotTable::peak), field(&SlotTable::capacity),
                field(&SlotTable::misses)};
    }

protected:
    D& self() { return static_cast<D&>(*this); }
    PoolCore* live_core() {
        PoolCore* c = self().core();
        if (c == nullptr) pool_misuse("use of an empty or moved-from nnh::Pool");
        return c;
    }
    int field(int SlotTable::*member) const {
        const PoolCore* c = const_cast<PoolApi*>(this)->self().core();
        return c != nullptr ? c->slots.*member : 0;
    }
};

std::size_t pool_block_bytes(int capacity, std::size_t size, std::size_t align);
PoolCore* pool_create_block(int capacity, std::size_t size, std::size_t align, MemTag* tag, Error& error);
void pool_free_block(PoolCore* core);
void pool_init_inline(PoolCore& core, int capacity, std::uint32_t* meta, std::uint32_t* bitmaps, void* data);
bool pool_move_allowed(const PoolCore* core);

} // namespace detail

/// Up to N objects of T stored inside the pool object itself (no allocation at all): make it a scene member. T is any
/// default-constructible type whose destructor doesn't throw; objects never move. Not copyable, not movable.
/// Memory: ≈ 56 B + 4·N + 12·⌈N/32⌉ + (N + 1)·sizeof(T) (one extra T for the checked accessor).
template <typename T, int N = kRuntimeCapacity>
class Pool : public detail::PoolApi<T, Pool<T, N>> {
    static_assert(N >= 1 && N <= kMaxPoolCapacity, "nnh::Pool<T, N>: N must be 1..65535");

public:
    /// Unbound: call flush() once per frame and clear() to restart.
    Pool() {
        detail::pool_init_inline(core_, N, meta_, bitmaps_, storage_);
        detail::PoolOps<T>::construct_all(core_);
    }
    /// Bound to `scene` (D30): flushed before its update(), cleared after its on_exit().
    explicit Pool(Scene& scene) : Pool() { this->bind(scene); }
    /// Destroys every object (T's destructor) and detaches from the scene.
    ~Pool() {
        if (core_.scene != nullptr) detail::pool_unbind(core_);
        detail::PoolOps<T>::destroy_all(core_);
    }
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

private:
    friend class detail::PoolApi<T, Pool>;
    friend struct detail::PoolAccess;
    detail::PoolCore* core() { return &core_; }

    static constexpr int kWords = detail::words_for(N);
    detail::PoolCore core_;
    std::uint32_t meta_[N];
    std::uint32_t bitmaps_[3 * kWords];
    alignas(T) unsigned char storage_[(static_cast<std::size_t>(N) + 1) * sizeof(T)];
};

/// The same pool with its capacity chosen at run time (from a data asset, from settings): one block of tagged memory.
/// Movable while unbound; empty (default-constructed or moved-from) it asserts on use and answers nothing.
template <typename T>
class Pool<T, kRuntimeCapacity> : public detail::PoolApi<T, Pool<T, kRuntimeCapacity>> {
public:
    Pool() = default;
    /// One block for `capacity` (1..65535) objects, tagged `tag` (default: the engine's `pool`). Errors:
    /// InvalidArgument (capacity), OutOfMemory. The only allocation of the pool's life.
    static Result<Pool> create(int capacity, MemTag* tag = nullptr) {
        Error error{};
        detail::PoolCore* c = detail::pool_create_block(capacity, sizeof(T), alignof(T), tag, error);
        if (c == nullptr) return error;
        detail::PoolOps<T>::construct_all(*c);
        Pool pool;
        pool.core_ = c;
        return Result<Pool>(static_cast<Pool&&>(pool));
    }
    ~Pool() { reset(); }
    /// Moving a bound pool asserts and moves nothing.
    Pool(Pool&& other) noexcept {
        if (!detail::pool_move_allowed(other.core_)) return;
        core_ = other.core_;
        other.core_ = nullptr;
    }
    Pool& operator=(Pool&& other) noexcept {
        if (this == &other || !detail::pool_move_allowed(other.core_)) return *this;
        reset();
        core_ = other.core_;
        other.core_ = nullptr;
        return *this;
    }
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

private:
    friend class detail::PoolApi<T, Pool>;
    friend struct detail::PoolAccess;
    detail::PoolCore* core() { return core_; }
    void reset() {
        if (core_ == nullptr) return;
        if (core_->scene != nullptr) detail::pool_unbind(*core_);
        detail::PoolOps<T>::destroy_all(*core_);
        detail::pool_free_block(core_);
        core_ = nullptr;
    }

    detail::PoolCore* core_ = nullptr;
};

namespace detail {
/// Tests and engine code: a pool's core (null when empty).
struct PoolAccess {
    template <typename P>
    static PoolCore* core(P& pool) {
        return pool.core();
    }
};
} // namespace detail

} // namespace nnh
