#pragma once
/// @file nnh/events.hpp
/// The event bus: publish facts, subscribe to them. See docs/api/events.md and
/// docs/architecture/events.md.
///
/// @code
/// struct CoinCollected { int value; };                         // any trivially copyable struct
///
/// nnh::Subscription sub = nnh::events::subscribe<CoinCollected>(
///     [this](const CoinCollected& e) { score_ += e.value; });   // ends when `sub` is destroyed
/// nnh::Subscription hud = nnh::events::subscribe<&Hud::on_coin>(&hud_object);
///
/// nnh::events::publish(CoinCollected{10});                      // delivered in the next nnh::frame()
/// @endcode
/// Capacities are fixed at nnh::init() (AppConfig::events). Nothing here allocates memory.

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "nnh/detail/small_function.hpp"
#include "nnh/detail/type_id.hpp"

namespace nnh {

/// Keeps a handler subscribed while it lives (RAII). Move-only. Keep it as a member of the object
/// whose handler it is, so the subscription ends with the object.
class Subscription {
public:
    Subscription() = default;
    ~Subscription() { reset(); }
    Subscription(Subscription&& other) noexcept : slot_(other.slot_), generation_(other.generation_) {
        other.slot_ = -1;
    }
    Subscription& operator=(Subscription&& other) noexcept;
    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    /// Unsubscribes now (no-op if already ended, or after nnh::shutdown()).
    void reset();

    /// True while the handler is subscribed.
    bool active() const;

private:
    friend struct SubscriptionAccess;
    Subscription(int slot, std::uint32_t generation) : slot_(slot), generation_(generation) {}
    int slot_ = -1;
    std::uint32_t generation_ = 0;
};

namespace detail {

using EventHandler = SmallFunction<void(const void*)>;

Subscription events_subscribe(TypeId type, EventHandler handler);
void events_publish(TypeId type, const void* data, std::size_t size);

/// Turns a `void(const T&)` callable into the type-erased handler stored by the bus.
template <typename T, typename Fn>
struct EventAdapter {
    Fn fn;
    void operator()(const void* event) { fn(*static_cast<const T*>(event)); }
};

template <typename Method>
struct MethodEvent;
template <typename C, typename T>
struct MethodEvent<void (C::*)(const T&)> {
    using Class = C;
    using Event = T;
};
template <typename C, typename T>
struct MethodEvent<void (C::*)(const T&) const> {
    using Class = C;
    using Event = T;
};

} // namespace detail

namespace events {

/// Largest event type, in bytes. Big data belongs elsewhere: send an id or index instead.
inline constexpr std::size_t kMaxEventSize = 256;
/// Events published by handlers of events published by handlers... stop at this depth (feedback loops).
inline constexpr int kMaxDepth = 8;

/// Subscribes `handler` (callable as `void(const T&)`) to events of type T. Handlers are stored inline
/// (no allocation): capture `this` or pointers, at most detail::kSmallFunctionCapacity bytes.
/// A handler added while events are being delivered only receives events published after it.
/// @return the subscription; discarding it unsubscribes immediately. An empty subscription if the
/// subscriber table is full (asserts in Debug/Development).
template <typename T, typename F>
[[nodiscard]] Subscription subscribe(F&& handler) {
    static_assert(std::is_trivially_copyable_v<T>, "nnh: event types must be trivially copyable structs");
    static_assert(std::is_invocable_v<std::decay_t<F>&, const T&>,
                  "nnh: the handler must be callable as void(const Event&)");
    using Fn = std::decay_t<F>;
    return detail::events_subscribe(detail::type_id<T>(),
                                    detail::EventHandler{detail::EventAdapter<T, Fn>{static_cast<F&&>(handler)}});
}

/// Subscribes a member function `void Class::method(const Event&)` of `object`:
/// `subscribe<&Hud::on_coin>(this)`.
template <auto Method, typename Obj>
[[nodiscard]] Subscription subscribe(Obj* object) {
    using Traits = detail::MethodEvent<decltype(Method)>;
    using T = typename Traits::Event;
    static_assert(std::is_base_of_v<typename Traits::Class, Obj>, "nnh: the method does not belong to this object");
    return subscribe<T>([object](const T& event) { (object->*Method)(event); });
}

/// Queues a copy of `event`. Subscribers receive it in the next nnh::frame() (or later in the current
/// delivery, when published by a handler), in publish order. Drops it if the queue is full
/// (asserts in Debug/Development).
template <typename T>
void publish(const T& event) {
    static_assert(std::is_trivially_copyable_v<T>, "nnh: event types must be trivially copyable structs");
    static_assert(sizeof(T) <= kMaxEventSize, "nnh: event too large (max 256 bytes): send an id instead of the data");
    detail::events_publish(detail::type_id<T>(), &event, sizeof(T));
}

/// Current capacities and usage (for tuning AppConfig::events).
struct Stats {
    std::size_t queue_bytes = 0; ///< ring buffer capacity
    int max_subscribers = 0;
    int subscribers = 0;     ///< active subscriptions
    int dropped_events = 0;  ///< since init(): queue full or depth limit
};
Stats stats();

} // namespace events
} // namespace nnh
