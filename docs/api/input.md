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
Inside a handler, the queries agree with the event. Use queries for continuous state ("is Left held?")
and events for moments ("Start was pressed").

## PC simulator controls

Keyboard, mouse and one gamepad work at the same time: buttons combine, and for each stick the input
pushed further wins.

| 3DS control | PC key | Gamepad (by position, any brand) |
|-------------|--------|----------------------------------|
| `Button::A` | X | right face button |
| `Button::B` | Z | bottom face button |
| `Button::X` | S | top face button |
| `Button::Y` | A | left face button |
| `Button::L` / `R` | Q / W | left / right shoulder |
| `Button::ZL` / `ZR` (New 3DS) | 1 / 2 | left / right trigger (past half) |
| `Button::Start` / `Select` | Enter / Backspace | Start / Back |
| D-Pad (`Up` `Down` `Left` `Right`) | arrow keys | D-Pad |
| Circle Pad | I / J / K / L (up / left / down / right) | left stick |
| C-Stick | numpad 8 / 4 / 2 / 6 | right stick |
| touch screen | left mouse button on the bottom screen | — |

- The keys follow the physical layout of the 3DS face buttons (Z/X bottom row = B/A, A/S top row = Y/X);
  the gamepad's face buttons map **by position**, so an Xbox pad's "B" (right) is the 3DS A.
- Keys move a stick fully; two keys give a diagonal of length 1.
- The first gamepad connected is used, also when plugged in while the game runs; if it is unplugged,
  its buttons are released and another connected pad takes over.
- A touch starts when the left button goes down **on the bottom screen**, then follows the mouse
  (clamped to the screen's edges, even outside the window) until the button is released. The position is
  in 3DS pixels whatever the window size and layout ([simulator.md](simulator.md#window)); in the "Top
  only" layout there is no touch screen.
- Scripted input for screenshots and tests: `NNH_SIM_INPUT` ([simulator.md](simulator.md)).

Named actions ("jump" on A or B, "move" from the Circle Pad or the D-Pad) with contexts, player remapping and saved
bindings are in [actions.md](actions.md) (`<nnh/actions.hpp>`, optional); they read the same input as these queries.

## Example

[`examples/input`](../../examples/input/main.cpp) shows every control: buttons, both sticks, the touch
point (live position from the query, start/end from the events) and the mirror (SELECT toggles it).

```sh
NNH_SIM_INPUT="circle=0.7,0.7;cstick=-1,0;touch=200,150;buttons=A+R" NNH_EXIT_AFTER_FRAMES=30   NNH_SCREENSHOT=build/input.png build/pc-debug/examples/input/input.exe
```
