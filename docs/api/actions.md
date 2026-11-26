# Input actions

`#include <nnh/actions.hpp>` — named actions ("jump", "move") bound to controls, grouped in contexts that can follow
scenes, read as queries or events, remapped by the player and saved. Spec:
[017](../../specs/017-input-actions/spec.md) · decision D32 ([decisions](../architecture/decisions.md)) · example
`examples/actions`.

Optional: a game that never creates an `ActionSet` links none of it and keeps reading [input](input.md) directly
(the frame loop pays one null check, 16 bytes of code on the 3DS).

## Declaring actions

A game owns its **action set** (usually one for the whole game, so remaps apply everywhere) and names actions with
its own enums:

```cpp
enum class Act { Move, Jump, Attack, Pause };
enum class Ctx { Play, Menu };

nnh::ActionSet controls{{.actions = 4, .contexts = 2}};   // one allocation (tag "actions"), about 4 KB
controls.add_context(Ctx::Play, "play");                   // context 0 exists as "default"; this renames it
controls.add_context(Ctx::Menu, "menu", /*enabled=*/false);
controls.add(Act::Move, {.name = "move", .kind = nnh::ActionKind::Vector,
                         .bindings = {{nnh::Source::stick(nnh::AnalogStick::CirclePad)}, {nnh::Source::dpad()}}});
controls.add(Act::Jump, {.name = "jump", .bindings = {{nnh::Source::button(nnh::Button::A)}}});
controls.add(Act::Pause, {.name = "pause",
                          .bindings = {{.source = nnh::Source::button(nnh::Button::Start), .locked = true}}});
```

- **Ids** are enum values `0..actions-1` (contexts `0..contexts-1`); queries are array lookups.
- **Names** are 1–31 characters of `[a-z0-9_]` (contexts 1–15), copied; they appear in logs, the debug view and
  saved bindings (by FNV-1a hash). Duplicate ids, names or name hashes, ids out of range, undeclared contexts and
  sources that don't fit the kind assert in Debug/Development and are refused with an error log in Release
  (`add` returns false).
- **Kinds**: `Button` (held/pressed/released; `value` 0..1), `Axis` (`value` in [-1, 1]), `Vector` (`vector()`,
  x right, y up, length ≤ 1). Every kind answers `held`: axes and vectors are held past `press` (default 0.5) until
  below `release` (0.4) of `|value|` / length.
- Capacities: 64 actions, 4 slots per action, 8 contexts per set (`kMaxActions`, `kMaxSlots`, `kMaxContexts`).

## Sources and bindings

Each action has up to 4 **slots**; a slot holds one `nnh::Source`:

| Source | Fits | Reads |
|--------|------|-------|
| `Source::button(b)` | button | the button; value 1 |
| `Source::stick_direction(stick, dir)` | button, axis | the push that way (0..1); a button action is held from `press` 0.5 until below `release` 0.4 (per binding) |
| `Source::stick(stick)` | vector | the whole stick (the engine's 10 % dead zone applied) |
| `Source::stick_axis(stick, axis)` | axis | one component |
| `Source::buttons(neg, pos)` | axis | positive − negative (opposites cancel) |
| `Source::buttons(up, down, left, right)`, `Source::dpad()` | vector | a diagonal has length 1 |
| `Source::touch(x, y, w, h)` | button | held while the bottom screen is touched inside (sliding in presses, out releases); always locked |

`Binding` adds: `dead_zone` (extra, for whole sticks and stick axes, re-scaled so the value still starts at 0 and
reaches 1), `invert_x`/`invert_y` (axis: `invert_x` negates it), `locked` (never changed by remapping, reset or
loading), and `fallback`/`fallback_for` (used only on systems **without** that capability). Thresholds and dead zones
are kept in 1 % steps.

**Combining** (several slots): a button action is held while any slot is, with the largest value; an axis takes
the slot with the largest `|value|`, a vector the longest; ties keep the lower slot.

## Reading actions

```cpp
if (controls.pressed(Act::Jump)) jump();
player.x += controls.vector(Act::Move).x * kSpeed;
if (controls.released(Act::Attack)) fire(controls.held_frames(Act::Attack));   // charge
if (controls.frames_since_pressed(Act::Jump) < 6 && on_ground) jump();          // jump buffer
```

`pressed`, `held`, `released`, `repeated`, `value`, `vector`, `held_frames` (1 on the press frame),
`frames_since_pressed` (0 on the press frame, `UINT32_MAX` = never, saturating), `usable` (any slot works on this
system). All queries of one frame agree with each other and with the events.

**Repeat** (menus): `.repeat = {.delay = 24, .interval = 6}` makes `repeated()` true on the press frame and every 6
frames after the first 24 while held. `interval = 0`: only the press.

## When sets are evaluated

Every live set is evaluated once per `nnh::frame()`, right after the input sample is read and its
`ButtonPressed`/`ButtonReleased`/`TouchStarted`/`TouchEnded` events are queued, before audio and the event dispatch
(D32). So queries in `update()` see this frame's input, and action events arrive in the same dispatch as the button
events. Sets run in creation order. Changes made during a frame (enable, bind, remap, reset, load) apply from the
next evaluation; queries keep this frame's values.

Actions read the same sample as `<nnh/input.hpp>`: the [simulator](simulator.md)'s keyboard, mouse and gamepad,
spec 013's remote input and scripts, and the D-Pad mirror when the game turned it on (a stick pushed with the mirror
on also holds D-Pad bindings).

## Events

```cpp
nnh::Subscription sub = nnh::events::subscribe<nnh::ActionPressed>([this](const nnh::ActionPressed& e) {
    if (e.is(controls_, Act::Pause)) nnh::scenes::push(pause_menu_);
});
```

`ActionPressed` / `ActionReleased` (4 bytes: the set's `id()` and the action) are published for button actions, and
for axis/vector actions when `held` changes. Order within a frame: the input events, then per set (creation order)
every press, then every release, each in declaration order. None for a disabled or blocked action, while listening,
or after `set_events_enabled(false)`. Values and vectors have no events: query them.

## Contexts and scenes

An action belongs to one context. A **disabled** context's actions read idle: no `held`, no edges, value 0, no
events, and disabling never sends `released`. Two enabled contexts may share a control (menu confirm and jump on A).

**Bound to a scene**: `controls.bind_context(Ctx::Menu, *this)` in the scene's `on_enter()` enables the context
exactly while that scene is the top scene, until it leaves the stack (pop, replace, end of `run()`,
`shutdown()`) — a binding lasts one stay. A pushed pause menu turns the level's context off and its own on from the
next frame, with no game code; popping restores them. Binding a scene that is not on the stack asserts; destroying a
set while a context is still bound to a scene on the stack asserts (and detaches in Release). `enable()` on a bound
context asserts; `unbind_context()` makes it manual and disabled.

**No leaking presses** (FR-13): when a context becomes enabled (by hand, by a scene, at the set's first
evaluation, and after a listen ends), every slot whose control is held right then is **blocked** until that control
is released (composites: all their buttons; sticks: below the release threshold). The A that confirmed "Start game"
doesn't make the player jump; a stick held in another slot keeps working. A control held when the set is created
doesn't press either.

Note for screenshots: `NNH_SIM_INPUT` holds its controls from the first frame, so they are blocked; drive actions
with a script (`nnh run --pc --script`) instead.

**Under an input block** (`input::set_blocked`, or a [scene transition](transition.md#input-is-blocked); spec 023):
actions read the blocked queries, so every action reads idle and no `ActionPressed` is queued; a binding whose control
is still held when the block ends stays unpressed until that control is released and pressed again (the confirm press
that started a transition never presses `jump` in the next scene). D32 is unchanged.

## Remapping

```cpp
controls.listen(Act::Jump, 0);                         // the next control pressed goes into slot 0
...
if (!controls.listening()) {
    const nnh::RemapResult& r = controls.listen_result();   // Bound, Cancelled, TimedOut or Rejected
}
```

- **Listen** (`ListenOptions`): controls held when it starts are ignored until released; then the first control
  that goes down is captured — buttons in `Button` bit order first, then Circle Pad, then C-Stick directions pushed
  past 0.5. A button action takes a button or a stick direction; an axis (`Part::Whole`) takes the **axis** of the
  pushed direction (pushing negative sets `invert_x`); a vector takes the whole stick; a composite **part**
  (`.part = Part::Up`, …) takes a button (a simple slot becomes an empty composite). `cancel` (default START) cancels,
  `exclude` is never captured, `sticks = false` ignores sticks, `timeout_frames` cancels after that many frames.
  Controls the system lacks can't be captured. While listening the whole set reads idle and sends no events; when it
  ends, held controls are blocked, so the press that binds doesn't also act. Listening a locked slot asserts.
- **Conflicts**: a source already used by another binding **of the same context** is resolved by `on_conflict`:
  `Swap` (default: the other binding gets this one's previous source, or is cleared when it doesn't fit),
  `Replace` (others cleared), `Allow` (both keep it) or `Reject` (nothing changes; the result names the holder).
  Locked bindings reject everything but `Allow`. Sticks conflict by direction (a whole stick covers its four).
  `conflicts(action, out, n)` lists shared controls for display.
- **Editing**: `set_source`, `set_part` (with a policy), `clear`, `set_invert`, `reset(action, slot)`,
  `reset(action)`, `reset_context`, `reset_all` — to the declared defaults, never touching locked slots.
- **Showing controls**: `actions::control_name(source)` (`"A"`, `"ZL"`, `"D-Pad Up"`, `"Circle Pad Left"`,
  `"C-Stick X"`, `"Touch"`), `actions::describe(source, …)` (`"D-Pad"`, `"L/R"`, `"X/B/Y/A"`, `-` for an empty part)
  and `set.describe(action, …)` (active slots: `"A / B"`). English, built-in font characters; translations and button
  glyphs are the game's.

## Saving and loading

```cpp
controls.save("controls.bin", kBindingsVersion);                 // nnh::save: atomic, backup, checksum
nnh::Result<nnh::LoadReport> r = controls.load("controls.bin", kBindingsVersion);
```

Only what the player changed is stored: one 12-byte entry per remappable slot that differs from its default, keyed
by action-name hash, slot and part, so reordering the enum, adding actions or changing defaults in a new release
keeps the player's choices and gives new defaults to the rest. `save_to`/`load_from` use a caller buffer (at most
`kMaxBindingsSize` = 3088 bytes) for games that keep bindings in their own save. The game's own **bindings version**
must match, or the file is ignored (`version_ignored`): the way to reset everyone after a big controls change.

Loading validates the whole blob first (a damaged or too-new blob is an error and changes nothing; a missing file is
`found = false`, no remaps), then returns every remappable slot to its default and applies each entry that fits;
unknown actions, slots, parts or kinds, data out of range, touch, locked slots and repeated keys are **skipped and
counted** (one warning). It never asserts on file content and is fuzz-tested. A source the system lacks (ZL on an
Old 3DS) is kept, inactive, and saved again, so an SD card moved between consoles keeps the choice.

`NNHB` layout (little-endian): `NNHB`, format 1, reserved 0, entry count (u16, ≤ 256), the game's version (u32),
CRC-32 of the entries; then entries `hash u32 · slot u8 · part u8 · source kind u8 · flags u8 (bit 0 invert x, bit 1
invert y) · data[4]` (a part entry: `data[0]` = the button, kind `Button`, or `None` for an emptied part).

## Old 3DS

ZL/ZR need `Capability::ZButtons`, C-Stick sources `CStick`, Circle Pad sources `CirclePad`, touch `TouchScreen`;
slots the system can't produce are inactive. Declare a fallback for them:

```cpp
controls.add(Act::Aim, {.name = "aim", .kind = nnh::ActionKind::Axis,
    .bindings = {{nnh::Source::stick_axis(nnh::AnalogStick::CStick, nnh::StickAxis::X)},
                 {.source = nnh::Source::buttons(nnh::Button::L, nnh::Button::R),
                  .fallback = true, .fallback_for = nnh::Capability::CStick}}});
```

In Debug/Development, the first time a context is enabled with an action that has no active slot, one warning
names it. The PC simulator has every control: check the Old 3DS path with the fake platform
(`platform_fake::set_capabilities`) or a console.

## Debug view

`controls.debug_draw(nnh::Screen::Bottom, 8, 8)` prints the enabled contexts and the held actions as debug text
(`actions: play` / `held: jump move`). Compiled out of Release.

## Costs and limits

| | |
|--|--|
| Memory | one block per set, tag `actions`: ≈ 0.5 KB header + 160 B per action + a 3088-byte file buffer (16 actions ≈ 6 KB, 64 ≈ 14 KB) |
| Per frame | proportional to enabled actions × active slots; no allocation, no `virtual`, no logging; profile scope `actions` |
| Measured | `examples/actions` bench, 64 actions × 4 mixed slots plus the game's set: PC simulator (Debug) avg 0.026 ms, worst 0.055 ms; Old 3DS (Development) avg 0.142 ms, worst 0.228 ms (0.224 / 0.317 before the console bench refinement, plan 017); console budget < 0.2 ms, about 2.2 µs per action |
| Per slot at rest | buttons, touch areas, composites and sticks not moved are skipped with a few integer tests (no float compare, no divide); a moved stick with an extra dead zone costs two divisions |
| Allocation | only the set's block at construction; evaluating, listening, remapping, resetting, saving and loading allocate nothing |
| `hello` | 16 bytes of code in `nnh::frame()` on the 3DS (one load, compare, indirect call), 14 on PC; no action code linked |

Not thread-safe (the game's thread only). Destroy sets before `nnh::shutdown()` or they appear in the leak report.

## UI widgets

`Ui::use_actions(set, map)` makes a UI read its directions, confirm, cancel and pages from your actions (a vector action
as four directions), so remapped buttons work in menus; the UI keeps its own repeat. It lives in its own object, linked
only by games that call it: [ui.md](ui.md#input).

## Not included

Remapping PC keys to 3DS controls (the simulator's concern), chords and double-taps, context priorities, several
players, gyroscope and touch gestures, button glyphs and translations, loading defaults from a data asset.
