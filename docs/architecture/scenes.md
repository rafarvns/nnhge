# Scenes

> Status: **implemented** ([spec 001](../../specs/001-event-bus-and-scenes/spec.md)). API reference:
> [docs/api/scene.md](../api/scene.md).

A game is a set of **scenes**: title screen, level, pause menu, game over. Each scene is one class with
its own state and behaviour. This is the **State pattern**: the game's behaviour changes by switching
the active scene instead of growing `if (mode == ...)` chains.

## The Scene contract

```cpp
class Level final : public nnh::Scene {
public:
    void on_enter() override;            // acquire resources, subscribe to events
    void on_exit() override;             // release what on_enter acquired
    void update() override;              // game logic for one frame (60 Hz)
    void draw() override;                // drawing only, no game logic
private:
    nnh::Subscription coin_sub_;         // released automatically with the scene
    int score_ = 0;
};
```

- `update()` changes state; `draw()` only reads it. This keeps drawing free of side effects.
- A scene talks to other scenes **only through events** (e.g. `LevelCompleted`) or through scene-stack
  requests. It never holds a pointer to another scene.
- `virtual` is used here on purpose: there are few scenes and few calls per frame, so the cost is
  negligible (see [../process/solid.md](../process/solid.md)).

## The scene stack

| Operation | Effect | Typical use |
|-----------|--------|-------------|
| `push(scene)` | New scene on top; the one below is paused (no update) and may still draw | Pause menu over a level |
| `pop()` | Remove the top scene; the one below resumes | Close pause menu |
| `replace(scene)` | Swap the top scene | Title → Level |

Stack changes requested during a frame take effect **at the end of that frame** (the start of the next
`nnh::frame()`), so a scene never leaves the stack in the middle of its own `update()`.

The **game creates and owns** its scene objects; the stack only holds pointers. Destroying a scene that is
still on the stack is caught at runtime ([api/scene.md](../api/scene.md#ownership-the-game-owns-its-scenes)).

## Frame flow with scenes

```
frame():  present previous frame → apply scene changes → input → input actions (D32) → deliver events (input last)
game:     top scene update() → draw scenes bottom-to-top          (scenes::update_and_draw(), nnh::run())
```

## Scene attachments (D30)

Some engine objects belong to a scene and must follow its life: a **tween set bound to a scene**
([api/tween.md](../api/tween.md#scene-bound-sets)) is updated right before that scene's `update()`, only while the
scene is on top, and cleared right after its `on_exit()`. The scene module offers a generic, allocation-free hook
for this ([decision D30](decisions.md)), so scenes never depend on the modules that use it:

- `Scene` holds one private pointer, the head of an intrusive list of `detail::SceneAttachment` nodes. A node is
  `{next, notify function pointer, epoch}` and lives inside the attaching object (for a bound `TweenSet`, inside
  its heap block, so the set itself can move).
- The scene stack notifies the top scene's nodes `BeforeUpdate` just before its `update()`, and a scene's nodes
  `AfterExit` just after its `on_exit()` — on `pop`, `replace`, the end of `run()` and `shutdown()`, which all go
  through the same exit path. Clearing after `on_exit()` also discards what `on_exit()` itself started.
- Nodes run in attach order. A node attached during a pass waits for the next pass (its epoch is that pass);
  detaching during a pass is safe (the pass keeps a cursor). One pass runs at a time.
- The `Scene` destructor checks that nothing is still attached (a bound set must not outlive its scene: make it a
  member of the scene).

A second user is input actions ([api/actions.md](../api/actions.md#contexts-and-scenes)): a context bound with
`ActionSet::bind_context(ctx, scene)` carries one node that only listens for `AfterExit`, which ends the binding, so a
binding lasts one stay on the stack. Whether the context is on is decided when the set is evaluated, right after input
(`scenes::top() == scene`): `BeforeUpdate` comes after that evaluation and would be one frame late (decision D32).

> Status: design ([spec 020](../../specs/020-entities/spec.md), decision D35). Two more users will bind through the same
> notifications: a **pool or world bound to a scene** is flushed on `BeforeUpdate` and cleared (remove hooks run,
> spawns refused) on `AfterExit`. [Plan 020](../../specs/020-entities/plan.md#scene-binding-and-the-d30-proof-fr-18)
> shows why that clear can't call into a scene that has left.

Internal API: `engine/core/src/scene/scene_attachments.hpp` (`scene_attach`, `scene_detach`, `scene_notify`).
Cost for a scene with nothing attached: one pointer and a null check per frame and per exit; a game with scenes but
no bound set links only `scene_notify()` (+176 bytes of code on the 3DS, measured in
[plan 015](../../specs/015-tweens-and-timers/plan.md)).

## Rules

- One scene per file pair (`level_scene.hpp/.cpp`), in the game's `scenes/` folder.
- Scenes own their resources and subscriptions as members (RAII). Because the game keeps scene objects
  alive after they leave the stack, subscribe in `on_enter` and reset in `on_exit`.
- Shared game data (save data, settings) lives in plain structs passed to scenes, not in globals.

## Further reading

- Robert Nystrom, *Game Programming Patterns*: [State](https://gameprogrammingpatterns.com/state.html)
  (including pushdown automata — our scene stack).
