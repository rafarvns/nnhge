# pool — object pools with generational handles

Header: `<nnh/pool.hpp>` · Spec: [020](../../specs/020-entities/spec.md) · Plan: [020](../../specs/020-entities/plan.md)
· Decision: [D35](../architecture/decisions.md) · Example: [`examples/entities`](../../examples/entities/arena.cpp)
(the ship's bullets) · In a game: [`examples/platformer`](../../examples/platformer/turrets.cpp) (the turrets'
bullets) · Worlds of entities: [entities.md](entities.md)

A pool keeps up to a fixed number of the game's own objects — bullets, particles, pickups — and hands out the
**lowest free slot**, with a 32-bit **handle** that tells a live object from a released one. It replaces the
hand-written "find the first inactive slot" array: nothing allocates while playing, a full pool is normal play (a
null handle and a miss in the stats), and a handle kept after its object was released reads as **stale** instead of
naming whatever took the slot. Released slots are **quarantined** until the next flush, so nothing acquired in the
same frame takes their place while loops and views of that frame are running.

The module is optional (constitution XI): a game that doesn't include the header links none of it. It doesn't include
`<nnh/entities.hpp>` (worlds have their own header) or any other engine module.

```cpp
struct Bullet { nnh::Vec2 position{}; nnh::Vec2 velocity{}; };

class Level final : public nnh::Scene {
    void update() override {
        if (nnh::input::held(nnh::Button::A)) bullets_.acquire({ship_, {0, -3}}); // full: nothing happens
        for (auto [h, b] : bullets_.items()) {
            b.position = b.position + b.velocity;
            if (b.position.y < 0) bullets_.release(h); // safe inside the loop
        }
    }
    nnh::Pool<Bullet, 32> bullets_{*this}; // bound: flushed before each update, emptied when the level is left
    nnh::Vec2 ship_{200, 220};
};
```

## Handles

`nnh::Handle<T>` is one `std::uint32_t`: the slot in bits 0–15 and a **generation** in bits 16–31. All zero is the
null handle (`Handle<T>{}`, `if (h)`), and generation 0 is never issued. Handles are trivially copyable: keep them in
your structs and events. `slot()`, `generation()` and `bits()` give the parts (for logs and checksums);
`Handle<T>::from_bits(word)` makes one back. A handle says nothing about liveness by itself — ask the pool:
`pool.alive(h)`, `pool.get(h)` (null when stale).

Every release raises the slot's generation (65 535 wraps to 1), so every handle issued before is stale from that
moment: `get` answers null, `release` answers false and changes nothing (releasing twice in a frame — two hits on the
same bullet — is normal). A handle naming a free slot with its next generation is refused too.

**The wrap limit:** a handle kept while its slot is reused exactly 65 535 times reads as live again. A slot is
reused at most once per flush, so with one flush per frame that is 65 535 frames (18 min 12 s at 60 Hz) of keeping
one stale handle while its slot is reused every single frame. A game that flushes several times per frame divides
that. Handles don't record which pool they came from: a handle used with another pool of the same type is caught
only by its generation.

## The two forms

| Form | Storage | Movable | Use |
|------|---------|---------|-----|
| `nnh::Pool<T, N>` | inside the pool object: no allocation at all | no | a scene member with a fixed capacity |
| `nnh::Pool<T>` | one block of tagged memory from `Pool<T>::create(n, tag)` | while unbound | a capacity from data or settings |

`T` is any default-constructible type whose destructor doesn't throw; objects never move, so it doesn't need to be
copyable or movable. Both forms behave the same. `create()` returns a `Result`: `InvalidArgument` for a capacity
outside 1..65 535, `OutOfMemory`; the default memory tag is `pool`. A default-constructed or moved-from run-time pool
is empty: commands assert, queries answer "nothing".

Memory: ≈ 56 B + 4·N + 12·⌈N/32⌉ + (N + 1)·sizeof(T) — the extra `T` is the checked accessor's scratch. A
`Pool<Bullet, 32>` of 16-byte bullets is about 0.6 KB.

## Acquire, release, quarantine and flush

| Call | What |
|------|------|
| `acquire()` | the lowest free slot's `T{}` and its handle: `Acquired<T>{handle, object}`; full: `{null, nullptr}` and a miss |
| `acquire(value)` | the same, the slot's object copied or moved from `value` |
| `release(h)` | destroys the object and leaves a `T{}` in its slot (a pointer you kept reads that, never freed memory); the handle is stale at once; false for a stale or null handle |
| `get(h)` / `alive(h)` | the object or null / whether `h` is live; constant time |
| `at(h)` | checked access: asserts on a stale handle (Debug/Development); Release returns a scratch `T{}`, never another object |
| `flush()` | quarantined slots become free |
| `clear()` | releases every object (ascending slots) and flushes: the next acquires start at slot 0 again; stats reset |

Quarantine is what keeps a frame consistent: a bullet released during this frame's update leaves its slot empty
(`T{}`) until the next flush, so a bullet fired later in the same frame goes elsewhere and a pointer taken earlier in
the frame never points at a newcomer. An unbound pool is flushed by the game, once per frame (typically at the start
of its update); a bound pool by its scene.

## Iteration and raw access

`for (auto [h, object] : pool.items())` and `pool.each([](nnh::Handle<T> h, T& object) { ... })` visit the live
objects in **ascending slot order**. Releasing any object inside is safe (it isn't visited afterwards); objects
acquired inside are **not** visited by that iteration (also when they land in a hole above the cursor); `break` is
fine. `flush()` or `clear()` inside one of the pool's own iterations asserts and is ignored.

Other modules can read the storage in place: `data()` (slot 0), `extent()` (one past the highest slot used since the
last `clear`), `stride()` (`sizeof(T)`), `is_live(slot)`, `handle_at(slot)` (null for a non-live slot). Every slot
that isn't live holds `T{}`. That is how a [collision box view](collision.md) reads bullets without copying them:
give the bullet's layers member a `kNoLayers` default and released slots are invisible to every query.

`count()`, `capacity()` and `stats()` (`PoolStats{live, peak, capacity, misses}`; peak and misses since creation or
the last `clear`) make capacities easy to tune.

## Scene binding

`nnh::Pool<T, N> pool{*this};` in a scene, or `pool.bind(scene)` (run-time pools: after `create`), attaches the pool
to the scene through the scene attachments of [decision D30](../architecture/decisions.md):

- right **before the scene's `update()`**, only while it is the top scene, the pool is **flushed** (a pause menu on
  top freezes it);
- right **after the scene's `on_exit()`**, it is **cleared**, so a level entered again starts empty.

Flushing a bound pool by hand is harmless. A bound pool must not outlive its scene (destroying the scene with the pool
still attached stops the game) and a bound run-time pool doesn't move (asserts, nothing moves); destroying a bound
pool detaches it. Binding twice or inside an iteration asserts and is ignored.

## Costs

Everything after creation is integer code inline in the game's own code: no allocation, no `virtual`, no call per
object. `get`/`alive`/`release`'s check: one bound check, one load, one compare. `acquire`: a scan of the free bitmap
from a hint word (every word below it is full), at most `capacity / 32` words. Iterations: proportional to the
extent. Old 3DS (Development, `examples/entities` bench): 1000 acquire/release pairs on a `Pool<Item, 1024>` plus a
flush take 0.44 ms, about 0.44 µs per pair.

## Misuse

Asserts in Debug/Development, harmless in Release (logged, nothing happens or nothing is answered): a command on an
empty or moved-from run-time pool, `at` on a stale handle, `flush`/`clear` inside an iteration, `bind` twice, moving
a bound pool, destroying a run-time pool inside its own iteration. A full pool is not misuse.
