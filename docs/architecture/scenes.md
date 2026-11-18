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
frame():  present previous frame → apply scene changes → input → deliver events (input events last)
game:     top scene update() → draw scenes bottom-to-top          (scenes::update_and_draw(), nnh::run())
```

## Rules

- One scene per file pair (`level_scene.hpp/.cpp`), in the game's `scenes/` folder.
- Scenes own their resources and subscriptions as members (RAII). Because the game keeps scene objects
  alive after they leave the stack, subscribe in `on_enter` and reset in `on_exit`.
- Shared game data (save data, settings) lives in plain structs passed to scenes, not in globals.

## Further reading

- Robert Nystrom, *Game Programming Patterns*: [State](https://gameprogrammingpatterns.com/state.html)
  (including pushdown automata — our scene stack).
