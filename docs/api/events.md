# events — publish and subscribe

Header: `<nnh/events.hpp>` · Design and rules: [architecture/events.md](../architecture/events.md)

Parts of a game react to each other through events: whoever sees something happen **publishes** a fact,
every interested part **subscribes** to it. The publisher doesn't know who listens.

```cpp
struct CoinCollected { int value; };            // any trivially copyable struct, no registration

class Hud {
public:
    Hud() : coin_sub_(nnh::events::subscribe<&Hud::on_coin>(this)) {}
private:
    void on_coin(const CoinCollected& e) { score_ += e.value; }
    nnh::Subscription coin_sub_;                  // unsubscribes when the Hud is destroyed
    int score_ = 0;
};

nnh::events::publish(CoinCollected{10});         // delivered in the next nnh::frame()
```

| API | Purpose |
|-----|---------|
| `Subscription events::subscribe<T>(handler)` | Calls `handler(const T&)` for each `T` event. Lambda, function pointer or functor. |
| `Subscription events::subscribe<&Class::method>(object)` | Same, with a member function `void method(const T&)` (const or not). |
| `void events::publish(const T& event)` | Queues a copy of `event`. |
| `events::Stats events::stats()` | Capacities, active subscriptions and dropped events (for tuning). |
| `Subscription::reset()` | Unsubscribes now. Also happens when the `Subscription` is destroyed. |
| `bool Subscription::active() const` | True while subscribed. |

`subscribe` is `[[nodiscard]]`: a discarded `Subscription` unsubscribes on the spot. Keep it as a member
of the object whose handler it is. `Subscription` is move-only; moving transfers the subscription and
move-assigning ends the one it replaces. Handles from before `nnh::shutdown()` are harmless afterwards.

## When handlers run

- `publish` only queues. Queued events are delivered once per frame, inside `nnh::frame()`: after input is
  read and the new frame is open, before `frame()` returns to your update code.
- Order is publish order, across all event types. Subscribers of one type are called in subscription
  order (don't depend on it).
- A handler may publish: the new event is delivered in the same frame, after those already queued.
  Chains deeper than `events::kMaxDepth` (8) are cut: the event is dropped with a warning (a feedback loop).
- A handler may unsubscribe itself or others: removed handlers are not called again, even for events
  already queued.
- A handler subscribed **while events are being delivered** only receives events published after it.
  Subscribed **outside** delivery, it also receives the events still waiting in the queue.
- `subscribe` and `publish` are for the game's thread, between `init()` and `shutdown()`.

## Handlers never allocate

Handlers are stored inline in 16 bytes. Capture `this` or pointers, not copies of big data:

```cpp
auto ok  = nnh::events::subscribe<CoinCollected>([this](const CoinCollected& e) { add(e.value); });
auto bad = nnh::events::subscribe<CoinCollected>([level_copy](const CoinCollected&) {}); // compile error:
// "nnh: event handler capture too large (max 16 bytes): capture `this` or a pointer to your data instead of copies"
```

Captures must be trivially copyable (pointers, numbers, `this`); `std::string` or containers are refused
at compile time. `mutable` lambdas keep their state between events.

## Event types

- Trivially copyable structs (checked at compile time), at most `events::kMaxEventSize` (256) bytes;
  aim for ≤ 32. Send an id instead of big data.
- Past tense for facts (`CoinCollected`), imperative for requests (`PlaySound`). See
  [architecture/events.md](../architecture/events.md#events-vs-requests).

## Capacities

Fixed at `nnh::init()` through `AppConfig::events`, allocated once (memory tag `events`):

| Field | Default | Minimum | Cost |
|-------|---------|---------|------|
| `queue_bytes` | 16 KB | 1024 | each queued event takes 16 bytes + its size rounded up to 8 |
| `max_subscribers` | 256 | 8 | about 48 bytes each on 3DS (64 on 64-bit PC) |

```cpp
nnh::init({.title = "My Game", .events = {.queue_bytes = 32 * 1024, .max_subscribers = 512}});
```

Example: [`examples/scenes`](../../examples/scenes/main.cpp) (`CoinCollected` published by the level,
counted by `Score`).

Running out is a bug: in Debug and Development a full queue or subscriber table **asserts** (crash screen
with the message). In Release the event is dropped (or `subscribe` returns an empty `Subscription`) and
one warning per frame is logged; `events::stats().dropped_events` counts drops. Use `stats()` while
developing to size the capacities.
