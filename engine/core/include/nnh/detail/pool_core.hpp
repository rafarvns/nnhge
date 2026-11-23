#pragma once
/// @file nnh/detail/pool_core.hpp
/// The untyped state of a pool (spec 020, D35) and its typed inline operations, shared by the inline form
/// Pool<T, N> (state inside the pool object) and the run-time form Pool<T> (state at the start of its block).
/// Engine-internal: use <nnh/pool.hpp>.

#include <cstddef>
#include <cstdint>
#include <new>

#include "nnh/detail/handle.hpp"
#include "nnh/detail/slot_bits.hpp"

namespace nnh {

class Scene;

template <typename T>
struct Acquired;

namespace detail {

/// Bit 16 of a slot's meta word: the slot holds a live object. Bits 0–15: the slot's current generation.
inline constexpr std::uint32_t kSlotLive = 1u << 16;

/// Everything a pool knows besides its element type. Standard layout: the scene attachment finds it by offset.
struct PoolCore {
    SlotTable slots;
    std::uint32_t* meta = nullptr; ///< per slot: generation | kSlotLive
    void* data = nullptr;          ///< T[capacity + 1]; the last slot is the checked accessor's scratch
    int iterating = 0;             ///< nesting depth of running iterations
    bool any_fresh = false;        ///< something was acquired during the running iteration
    Scene* scene = nullptr;        ///< bound scene (D30), or null
    void (*clear_fn)(PoolCore&) = nullptr; ///< the typed clear(), for the scene's AfterExit
    alignas(void*) unsigned char node[3 * sizeof(void*)] = {}; ///< the scene attachment (pool_scene.cpp)
};

// Out of line (src/pool/): per operation, never per slot.
void pool_flush(PoolCore& core);             ///< asserts (and does nothing) inside an iteration
void pool_end_iteration(PoolCore& core);     ///< the outermost iteration ended with fresh slots
void pool_misuse(const char* what);          ///< asserts in Debug/Development, logs an error in Release
bool pool_clear_allowed(const PoolCore& core); ///< false (after an assert) inside an iteration
void pool_bind(PoolCore& core, Scene& scene);
void pool_unbind(PoolCore& core);

struct PoolAccess; // tests: reach a pool's core

/// The typed operations, inline: one load and one compare per lookup, a bitmap scan per acquire.
template <typename T>
struct PoolOps {
    static T* data(PoolCore& c) { return static_cast<T*>(c.data); }

    static T* get(PoolCore& c, Handle<T> h) {
        const std::uint32_t slot = h.bits() & 0xFFFFu;
        if (slot >= static_cast<std::uint32_t>(c.slots.capacity) || c.meta[slot] != ((h.bits() >> 16) | kSlotLive))
            return nullptr;
        return data(c) + slot;
    }

    static int take(PoolCore& c) {
        const int slot = c.slots.find_free();
        if (slot < 0) {
            ++c.slots.misses;
            return -1;
        }
        const bool iterating = c.iterating > 0;
        c.slots.take(slot, iterating);
        if (iterating) c.any_fresh = true; // no store in the common case
        c.meta[slot] |= kSlotLive;
        return slot;
    }

    static Handle<T> handle(const PoolCore& c, int slot) {
        return make_handle<T>(static_cast<std::uint32_t>(slot), c.meta[slot] & 0xFFFFu);
    }

    static bool release(PoolCore& c, Handle<T> h) {
        T* object = get(c, h);
        if (object == nullptr) return false;
        const int slot = h.slot();
        c.meta[slot] = next_generation(c.meta[slot] & 0xFFFFu); // stale from now on
        c.slots.retire(slot);
        object->~T();
        ::new (static_cast<void*>(object)) T{};
        return true;
    }

    static T& scratch(PoolCore& c) {
        T* s = data(c) + c.slots.capacity;
        s->~T();
        ::new (static_cast<void*>(s)) T{};
        return *s;
    }

    static void clear(PoolCore& c) {
        if (!pool_clear_allowed(c)) return;
        const int words = words_for(c.slots.extent);
        for (int w = 0; w < words; ++w) {
            for (std::uint32_t bits = c.slots.live[w]; bits != 0; bits &= bits - 1) {
                const int slot = (w << 5) + lowest_bit(bits);
                release(c, handle(c, slot));
            }
        }
        c.slots.reset();
        c.slots.peak = 0;
        c.slots.misses = 0;
    }
    static void clear_erased(PoolCore& c) { clear(c); }

    static void construct_all(PoolCore& c) {
        for (int i = 0; i <= c.slots.capacity; ++i) ::new (static_cast<void*>(data(c) + i)) T{};
        c.clear_fn = &clear_erased;
    }
    static void destroy_all(PoolCore& c) {
        for (int i = 0; i <= c.slots.capacity; ++i) data(c)[i].~T();
    }

    template <typename F>
    static void each(PoolCore& c, F& fn) {
        ++c.iterating;
        const int words = words_for(c.slots.extent);
        T* objects = data(c);
        for (int w = 0; w < words; ++w) {
            std::uint32_t bits = c.slots.live[w];
            if (c.any_fresh) bits &= ~c.slots.fresh[w];
            while (bits != 0) {
                const int slot = (w << 5) + lowest_bit(bits);
                bits &= bits - 1;
                fn(handle(c, slot), objects[slot]);
                bits &= c.slots.live[w]; // released by the callback: not visited
            }
        }
        if (--c.iterating == 0 && c.any_fresh) pool_end_iteration(c);
    }
};

/// One live object of an iteration: `for (auto [handle, bullet] : pool.items())`.
template <typename T>
struct PoolItem {
    Handle<T> handle;
    T& object;
};

/// The range of items(): ascending slots, FR-7's rules. Holds the iteration guard (break is safe).
template <typename T>
class PoolRange {
public:
    explicit PoolRange(PoolCore* core) : core_(core) {
        if (core_ == nullptr) return;
        ++core_->iterating;
        words_ = words_for(core_->slots.extent);
    }
    ~PoolRange() {
        if (core_ != nullptr && --core_->iterating == 0 && core_->any_fresh) pool_end_iteration(*core_);
    }
    PoolRange(const PoolRange&) = delete;
    PoolRange& operator=(const PoolRange&) = delete;

    class iterator {
    public:
        PoolItem<T> operator*() const {
            return {PoolOps<T>::handle(*core_, slot_), PoolOps<T>::data(*core_)[slot_]};
        }
        iterator& operator++() {
            bits_ &= core_->slots.live[word_]; // released by the loop's body: not visited
            find();
            return *this;
        }
        bool operator!=(const iterator& other) const { return slot_ != other.slot_; }

    private:
        friend class PoolRange;
        iterator() = default;
        iterator(PoolCore* core, int words) : core_(core), words_(words), word_(-1) { find(); }
        void find() {
            while (bits_ == 0) {
                if (++word_ >= words_) {
                    slot_ = -1;
                    return;
                }
                bits_ = core_->slots.live[word_];
                if (core_->any_fresh) bits_ &= ~core_->slots.fresh[word_];
            }
            slot_ = (word_ << 5) + lowest_bit(bits_);
            bits_ &= bits_ - 1;
        }

        PoolCore* core_ = nullptr;
        int words_ = 0;
        int word_ = 0;
        std::uint32_t bits_ = 0;
        int slot_ = -1;
    };

    iterator begin() const { return core_ != nullptr ? iterator(core_, words_) : iterator(); }
    iterator end() const { return iterator(); }

private:
    PoolCore* core_ = nullptr;
    int words_ = 0;
};

} // namespace detail
} // namespace nnh
