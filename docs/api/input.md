# input — buttons, sticks, touch

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

## Circle Pad and C-Stick

| Function | Returns |
|----------|---------|
| `nnh::input::circle_pad()` | `nnh::Stick {x, y}`: the Circle Pad this frame |
| `nnh::input::c_stick()` | the C-Stick (New 3DS); always (0, 0) when the system has none |

Each axis is a `float` in [-1, 1]: x grows to the right, **y grows up** (the opposite of screen y).
A dead zone of 10% around the center reads exactly (0, 0); past it the value starts at 0 and reaches 1
at full range, keeping the direction, and the length never exceeds 1 (diagonals included).

```cpp
const nnh::Stick pad = nnh::input::circle_pad();
player.x += pad.x * kRunSpeed;   // pushed halfway → about half speed
player.y -= pad.y * kRunSpeed;   // stick up = screen up
if (!nnh::system::has(nnh::Capability::CStick)) camera_with_l_r();   // Old 3DS
```

## Touch screen

| Function | True / returns |
|----------|----------------|
| `nnh::input::touching()` | the bottom screen is touched this frame |
| `nnh::input::touch_started()` | the touch started this frame (one frame only) |
| `nnh::input::touch_ended()` | the touch ended this frame (one frame only) |
| `nnh::input::touch_position()` | `nnh::TouchPoint {x, y}` in bottom-screen pixels (0–319, 0–239); while not touching, the last touched point |

```cpp
if (nnh::input::touching()) cursor = nnh::input::touch_position();   // drag
```

## Circle Pad as D-Pad (opt-in)

`Button::Up`/`Down`/`Left`/`Right` are the D-Pad only. For menus that should also follow the Circle Pad:

```cpp
nnh::input::set_circle_pad_mirrors_dpad(true);              // off by default and after every init()
if (nnh::input::pressed(nnh::Button::Down)) menu.next();    // D-Pad or Circle Pad
```

While on, pushing the Circle Pad to half its range holds the matching direction, with `pressed`,
`released` and `ButtonPressed`/`ButtonReleased` exactly like the D-Pad; it lets go below 40%, so it
doesn't flicker around the threshold. A diagonal holds two directions. `circle_pad_mirrors_dpad()` reads
the setting; a change takes effect from the next `nnh::frame()`.

## Events

The same changes are also published on the [event bus](events.md), for code that prefers to react
instead of polling:

| Event | Published | Payload |
|-------|-----------|---------|
| `nnh::ButtonPressed` | once per button that went down this frame | `Button button` |
| `nnh::ButtonReleased` | once per button that went up this frame | `Button button` |
| `nnh::TouchStarted` | on the frame a touch starts | `TouchPoint position` |
| `nnh::TouchEnded` | on the frame it ends | `TouchPoint position` (the last touched point) |

```cpp
nnh::Subscription sub = nnh::events::subscribe<nnh::ButtonPressed>([this](const nnh::ButtonPressed& e) {
    if (e.button == nnh::Button::Start) paused_ = true;
});
```

They are delivered in the same `nnh::frame()` that read the input, after events the game published in
the previous frame. Within a frame: all presses first, then all releases, each in `Button` order
(A, B, X, Y, L, R, ZL, ZR, Start, Select, Up, Down, Left, Right), then `TouchStarted`, then
`TouchEnded`. There is no event for touch moves or stick motion: poll `touch_position()` and the sticks.
Inside a handler, the queries agree with the event. Use queries for continuous state ("is Left held?") and events
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
The PC simulator's keys for the sticks, the mouse as touch and gamepads arrive with
[spec 003](../../specs/003-full-input/spec.md) (in progress); until then sticks read (0, 0) and the
bottom screen isn't touchable on PC. Remapping is a later spec.
