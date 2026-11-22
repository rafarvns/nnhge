# scene — scenes and the scene stack

Header: `<nnh/scene.hpp>` · Design: [architecture/scenes.md](../architecture/scenes.md)

A scene is one game mode (title, level, pause menu). The engine keeps a **stack** of scenes: the top one
updates, all of them draw from bottom to top, so a pause menu pushed over a level draws on top of it while
the level stays frozen.

```cpp
class Pause final : public nnh::Scene {
    void update() override { if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop(); }
    void draw() override { nnh::debug::print(nnh::Screen::Bottom, 1, 1, "PAUSED"); }
};

class Level final : public nnh::Scene {
public:
    explicit Level(Pause& pause) : pause_(pause) {}
private:
    void on_enter() override { coin_sub_ = nnh::events::subscribe<&Level::on_coin>(this); }
    void on_exit() override { coin_sub_.reset(); }
    void update() override { if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::push(pause_); }
    void draw() override { /* draw the level */ }
    void on_coin(const CoinCollected& e) { score_ += e.value; }
    Pause& pause_;
    nnh::Subscription coin_sub_;
    int score_ = 0;
};

int main() {
    if (!nnh::init({.title = "My Game"})) return 1;
    Pause pause;                 // the game creates and owns its scenes
    Level level(pause);
    nnh::run(level);             // until the stack is empty or the system asks to quit
    nnh::shutdown();
}
```

## nnh::Scene

| Override | Called |
|----------|--------|
| `on_enter()` | when the scene enters the stack (`push`, `replace`): acquire resources, subscribe to events |
| `on_exit()` | when it leaves (`pop`, `replace`, end of `run()`, `shutdown()`): release what `on_enter` took |
| `update()` | once per frame, **only for the top scene**: game logic |
| `draw()` | once per frame for **every scene on the stack**, bottom to top: drawing only |

All four are optional. `on_stack()` tells whether the scene is between `on_enter` and `on_exit`. Scenes
are not copyable. A scene that stays alive after leaving the stack keeps its members: reset subscriptions
in `on_exit` if it shouldn't react while off the stack (or while paused under another scene, if needed).

## The stack

| Function | Effect |
|----------|--------|
| `scenes::push(scene)` | `scene` goes on top; the one below stops updating but still draws |
| `scenes::pop()` | removes the top scene; the one below updates again |
| `scenes::replace(scene)` | pop + push in one step (on an empty stack: push) |
| `scenes::top()` / `scenes::size()` | the top scene (or nullptr) / how many scenes are on the stack |
| `scenes::update_and_draw()` | top `update()`, then every `draw()`; called by `run()` |
| `nnh::run(first)` | pushes `first` and loops `frame()` + `update_and_draw()` until the stack is empty or `frame()` returns false; exits every scene left |

- **Changes are deferred**: they take effect at the start of the next `nnh::frame()` (the end of the
  current frame), in request order, before input is read and events are delivered. A scene can
  `pop()` itself inside its own `update()` safely; it still draws in that frame.
- Changes requested by `on_enter`/`on_exit` are applied right after, in the same frame.
- Limits: `scenes::kMaxDepth` (8) scenes on the stack, `scenes::kMaxPending` (8) changes per frame.

## Ownership (the game owns its scenes)

The engine never creates or deletes scenes; it keeps pointers to the scenes you give it. A scene must stay
alive while it is on the stack or waiting to be pushed. These mistakes stop the game with a clear message
on the crash screen, in every build flavor:

- destroying a scene that is on the stack or waiting to be pushed;
- pushing a scene that is already on the stack;
- `pop()` on an empty stack, more than 8 scenes, more than 8 changes in a frame, or changes that keep
  requesting more changes (a scene that pops itself in `on_enter` and pushes itself in `on_exit`).

`nnh::shutdown()` exits every scene still on the stack (top first), so a hand-written loop can just call
`shutdown()` before its scenes go out of scope.

## Tweens and timers bound to a scene

A `nnh::TweenSet` member created with the scene (`nnh::TweenSet tweens_{*this, {.tweens = 32}};`) is updated
right before the scene's `update()`, only while the scene is on top (a pause menu freezes the level's
animations), and cleared silently right after its `on_exit()`. Nothing to update or clear by hand; see
[api/tween.md](tween.md#scene-bound-sets) and the attachment hook in
[architecture/scenes.md](../architecture/scenes.md#scene-attachments-d30). Destroying a scene while such a set is
still attached to it stops the game with an error, like destroying a scene on the stack.

## Input contexts bound to a scene

`controls.bind_context(Ctx::Menu, *this)` in a scene's `on_enter()` enables that [action](actions.md) context exactly
while the scene is on top and ends the binding when it leaves the stack: a pushed pause menu turns the level's
controls off and its own on, and controls held at the switch don't leak into the other context
([api/actions.md](actions.md#contexts-and-scenes)).

## Without run()

`run()` is optional. A manual loop works the same and can mix scenes with other code:

```cpp
nnh::scenes::push(title);
while (nnh::frame() && nnh::scenes::size() > 0) {
    nnh::scenes::update_and_draw();
    // anything else, e.g. a debug HUD
}
```

A game that doesn't use scenes at all keeps its plain `while (nnh::frame())` loop.

## Example

[`examples/scenes`](../../examples/scenes/main.cpp): title → level → pause menu pushed over the level. The
level publishes `CoinCollected` and a separate `Score` object counts it; buttons arrive both as
`ButtonPressed` events and as `input::pressed()` queries. One scene per file pair.

```sh
build/pc-debug/examples/scenes/scenes.exe
SCENES_DEMO_START=pause NNH_EXIT_AFTER_FRAMES=90 NNH_SCREENSHOT=build/pause.png build/pc-debug/examples/scenes/scenes.exe
```
