# input — buttons

Header: `<nnh/input.hpp>`

Input is read once per `nnh::frame()`; every query in the same frame returns the same answer.

| Function | True when |
|----------|-----------|
| `nnh::input::held(Button)` | the button is down this frame |
| `nnh::input::pressed(Button)` | the button went down this frame (one frame only) |
| `nnh::input::released(Button)` | the button went up this frame (one frame only) |
| `nnh::input::held_mask()` | returns all held buttons as a bit mask |

```cpp
if (nnh::input::pressed(nnh::Button::A)) jump();
if (nnh::input::held(nnh::Button::Left)) player.x -= 1;
```

A button already held when the game starts reports `pressed` on the first frame.

## Events

The same changes are also published on the [event bus](events.md), for code that prefers to react
instead of polling:

| Event | Published | Payload |
|-------|-----------|---------|
| `nnh::ButtonPressed` | once per button that went down this frame | `Button button` |
| `nnh::ButtonReleased` | once per button that went up this frame | `Button button` |

```cpp
nnh::Subscription sub = nnh::events::subscribe<nnh::ButtonPressed>([this](const nnh::ButtonPressed& e) {
    if (e.button == nnh::Button::Start) paused_ = true;
});
```

They are delivered in the same `nnh::frame()` that read the input, after events the game published in
the previous frame. Within a frame: all presses first, then all releases, each in `Button` order
(A, B, X, Y, L, R, ZL, ZR, Start, Select, Up, Down, Left, Right). Inside a handler, `input::pressed()`
and `input::held()` agree with the event. Use queries for continuous state ("is Left held?") and events
for moments ("Start was pressed").

## Buttons and PC keyboard mapping

| `nnh::Button` | 3DS | PC key |
|---------------|-----|--------|
| `A` | A | X |
| `B` | B | Z |
| `X` | X | S |
| `Y` | Y | A |
| `L` | L | Q |
| `R` | R | W |
| `ZL` | ZL (New 3DS) | 1 |
| `ZR` | ZR (New 3DS) | 2 |
| `Start` | START | Enter |
| `Select` | SELECT | Backspace |
| `Up` `Down` `Left` `Right` | D-Pad | Arrow keys |

The keys follow the physical layout of the 3DS face buttons (Z/X bottom row = B/A, A/S top row = Y/X).
Circle Pad, C-Stick, touch, gamepads and remapping are planned for M1.
