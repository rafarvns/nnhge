# SOLID in NNHGE

SOLID keeps modules small and replaceable. On a 268 MHz ARM11 we apply it **without paying for it at
runtime**: an *interface* means hiding the implementation, not necessarily a `virtual` function.

## S — Single Responsibility

A module, a class, a file: one reason to change.

```cpp
// Don't: app.cpp handles the loop AND clear colors AND button edges.
// Do:    app/app.cpp (loop), gfx/gfx.cpp (clear colors), input/input.cpp (buttons).
```

Rule of thumb: if describing a file needs "and", split it. File limits in
[../architecture/project-structure.md](../architecture/project-structure.md).

## O — Open/Closed

Add behaviour by adding code, not by editing a central switch.

```cpp
// Don't: every new reaction edits the publisher.
void collect_coin() { score += 1; play_coin_sound(); update_hud(); check_achievement(); }

// Do: publish a fact; each reaction subscribes in its own place.
void collect_coin() { nnh::events::publish(CoinCollected{1}); }
```

New game modes are new scenes, not new `if (mode == ...)` branches.

## L — Liskov Substitution

Anything implementing a contract must honour all of it. Every `nnh::Scene` must work wherever a scene is
expected: `on_exit` undoes `on_enter`, `draw()` has no side effects, `update()` doesn't destroy the scene
directly (it requests a stack change). Every platform backend must satisfy the same
`platform.hpp` behaviour (documented per function), checked by running the same examples on each.

## I — Interface Segregation

Small interfaces; include only what you use.

```cpp
// Don't: #include <nnh/nnh.hpp> in engine code (pulls in everything, slows builds).
// Do:    #include <nnh/input.hpp>
```

One public header per module, each small. Internal headers expose only what other modules need.

## D — Dependency Inversion

High-level code depends on abstractions, not on platform details.

```cpp
// engine/src/input/input.cpp depends on the abstraction...
g_input.update(platform::read_buttons());
// ...and the build links exactly one implementation: input_3ds.cpp, input_pc.cpp or input_fake.cpp.
```

This is a **link-time** interface (the Bridge pattern from [patterns.md](../architecture/patterns.md)):
zero runtime cost, and tests link the fake backend.

## When `virtual` is fine

| OK | Avoid |
|----|-------|
| Per scene / per system: a handful of calls per frame | Per object: `virtual update()` on hundreds of entities |
| Plugin-like boundaries chosen at runtime | Hot loops over arrays of mixed types |

For many objects, keep data in arrays of plain structs and process them in one function (data-oriented),
called from a scene.

## Further reading

- Brooke Hodgman, [OOP is dead, long live OOP](https://gamedev.net/blogs/entry/2265481-oop-is-dead-long-live-oop/) — implementation hiding without virtuals.
- Richard Fabian, [*Data-Oriented Design*](https://www.dataorienteddesign.com/dodbook/node12.html) — the cost of virtual-heavy designs.
