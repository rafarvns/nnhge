# Spec 001: Event bus and scenes

Status: draft
Author: NNHGE maintainers · Date: 2026-11-17
Depends on: [spec 002 — core foundation](../002-core-foundation/spec.md) (logging, asserts, platform
contract), which is implemented first.

## Problem and goal

Today a game is one `while (nnh::frame())` loop in `main()`. As soon as a game has a title screen, a
level and a pause menu, everything ends up in one function full of mode flags, and parts that react to
each other (score, sound, HUD) call each other directly. Games need a way to split into **scenes** and to
let parts **react to events** without knowing each other, as described in
[docs/architecture/events.md](../../docs/architecture/events.md) and [scenes.md](../../docs/architecture/scenes.md).

## User stories

- As a game developer, I want to publish an event (e.g. `CoinCollected`) and have any number of
  listeners react, so that adding a reaction doesn't require changing the code that collects coins.
- As a game developer, I want subscriptions to end automatically when the owner is destroyed, so that I
  never get callbacks into a scene that no longer exists.
- As a game developer, I want to write each game mode as a scene and switch, push and pop scenes, so that
  my game is organized and a pause menu can sit on top of a level.
- As a game developer, I want engine facts like button presses delivered as events too, so that I can
  react to them the same way as to my own events.

## Functional requirements

- **FR-1** Games can define any trivially copyable struct as an event type, without registering it.
- **FR-2** `subscribe<T>(handler)` registers a handler and returns an RAII subscription; destroying or
  resetting it unsubscribes.
- **FR-3** `publish<T>(event)` queues a copy of the event; queued events are delivered once per frame at
  a fixed point (after input is read, before the active scene updates), in publish order.
- **FR-4** Re-entrancy follows the rules in `events.md`: events published during dispatch are delivered in
  the same dispatch after earlier ones; a handler removed during dispatch is not called again; a handler
  added during dispatch only receives later publishes; a depth limit stops feedback loops.
- **FR-5** No RTTI and no heap allocation per publish/dispatch; capacities (queue bytes, subscribers) are
  fixed at `init()` with documented defaults.
- **FR-6** Overflowing a capacity asserts in debug builds and drops the event with a log line in release.
- **FR-7** The input module publishes `ButtonPressed` and `ButtonReleased` events.
- **FR-8** `nnh::Scene` defines `on_enter`, `on_exit`, `update`, `draw`. The engine runs a scene stack with
  `push`, `pop`, `replace`; changes requested during a frame take effect at the end of that frame.
- **FR-9** The top scene updates; scenes draw bottom to top (so a pause menu draws over its level).
- **FR-10** A game can run entirely through scenes: `nnh::run(first_scene)` (or equivalent) replaces the
  hand-written loop, while the plain `while (nnh::frame())` loop keeps working.

## Acceptance criteria

- **AC-1** Unit tests (PC) cover FR-1 to FR-6 and FR-8/FR-9 using the fake platform backend, including
  each re-entrancy rule.
- **AC-2** An example `examples/scenes/` has a title scene and a level scene with a pause scene pushed on
  START; the score updates via an event. Screenshots from the simulator show each scene; it runs on a 3DS.
- **AC-3** The `hello` example still works unchanged.
- **AC-4** `docs/api/events.md` and `docs/api/scene.md` exist; `docs/architecture/events.md` and
  `scenes.md` no longer say "design".
- **AC-5** Build budgets still met (constitution III).

## Non-goals

- Multithreaded publishing.
- An entity-component system.
- Scene transitions/animations (fades) — a later spec.
- Serialization of events (replays, networking).

## Constraints

- Constitution II and V: fixed capacities, no RTTI, no per-frame heap. Default capacities must fit
  comfortably in Old 3DS memory (target: < 64 KB total for the bus).
- Constitution VII: bus, subscription, queue and scene stack in separate files.

## Open questions

- [ ] Handler type: `std::function` (may allocate) or a small fixed-size callable (no heap)? Leaning to a
  fixed-size, non-allocating callable with a size limit.
- [ ] Should scenes be created by the game (`std::unique_ptr`) or by the engine from a type (`push<Level>()`)?
- [ ] Default capacities: queue 16 KB, 256 subscribers? To be confirmed in the plan.
