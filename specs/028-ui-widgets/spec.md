# Spec 028: UI widgets

Status: implemented on the PC 2026-11-25 (console T10.2 and Linux T9.1 pending); approved 2026-11-25 (the maintainer, every open question as recommended)
Author: NNHGE maintainers · Date: 2026-11-25
Depends on: [spec 004](../004-2d-graphics/spec.md) (textures and regions, `gfx::draw`, `fill_rect`/`draw_rect`, layers,
`ignore_camera`, `max_draws`), [spec 005](../005-text-and-builtin-font/spec.md) (`gfx::draw_text`/`measure_text`, the
built-in font, UTF-8 and Latin-1), [spec 003](../003-full-input/spec.md) (buttons, Circle Pad, touch and their events),
[spec 001](../001-event-bus-and-scenes/spec.md) (scenes and the event bus) and D30's scene attachments
([spec 015](../015-tweens-and-timers/spec.md); `<nnh/ease.hpp>`'s libm-free easings), [spec 002](../002-core-foundation/spec.md)
(asserts, logs, `Result`, tagged memory, profiling). Respects [spec 023](../023-scene-transitions/spec.md)'s input block.
Works with [spec 017](../017-input-actions/spec.md)'s actions (an optional input source), spec 015's tween sets
(animating widgets by setter), [spec 016](../016-sprite-animation/spec.md)'s sprite libraries (icon frames) and
[spec 014](../014-data-assets/spec.md)'s data assets (styles in text files, by a documented pattern) without depending
on any of them. Prepares the seams the next two specs plug into: **game fonts** (ROADMAP M2 follow-up) and
**localization** (M7). Verified on the console with [spec 013](../013-console-remote/spec.md)'s `nnh run --script`.
Implements the ROADMAP item "UI widgets" (M7, Features in the spirit of Godot and Unity), the first of the last four
features before NNHGE 1.0. Optional for games (constitution XI, decision D25).

## Problem and goal

Every game has a title screen, a pause menu, an options screen with toggles, sliders and choices ("Difficulty:
< Normal >"), a scrolling inventory or level list, a dialog box and a "Quit without saving? Yes / No" prompt. Today an
NNHGE game draws each of these by hand: it computes rectangles for every row, keeps a selected index, writes the
D-Pad repeat, the wrap at the ends, the skipping of locked items, the scroll offset, the hit test for the bottom
screen, the highlight and the "which menu was I on" memory itself — in every game, differently, and usually without a
touch path. Godot (`Control` nodes, containers, focus neighbours, `NinePatchRect`) and Unity (uGUI `Selectable`
navigation, layout groups, 9-sliced sprites) give games **widgets** that lay themselves out in **boxes**, move a
**focus** with a controller, and report what the player chose. After this spec an NNHGE game builds its menus **in
C++ code** from labels, buttons, toggles, sliders, choice lists, scrolling lists, grids, images, progress bars, solid
or **9-slice panels** and **layout boxes**, on either screen (400×240 top, 320×240 bottom), moves focus with the
**D-Pad and Circle Pad** (with held-direction repeat) and with **touch on the bottom screen**, and reads what happened
by **query or callback**. The UI lives in **one block** the game owns, allocates nothing while playing, lays out in
integer pixels so it is **the same on PC and 3DS** (scripts and goldens compare exactly), draws through gfx on a layer
the game picks, and takes its text through a seam that game fonts and localization will fill without changing
widget code. A game that doesn't use it pays nothing.

## User stories

- As a game developer, I want to build a title menu (New game, Continue, Options, Quit) in a few lines — a vertical box
  of buttons centred on the bottom screen — and get "Options was chosen" back, so that I never compute a row
  rectangle or a selected index again.
- As a game developer, I want an options screen with a music volume slider (0–10), an "Invert Y" toggle, a
  "Difficulty < Easy / Normal / Hard >" choice and a "Back" button, changed with the D-Pad (left/right adjusts, up/down
  moves) or by touch (drag the slider, tap the toggle, tap the arrows), so that settings screens take minutes.
- As a game developer, I want an inventory or level-select list of hundreds of items — one row per item with an icon,
  a name and a count, or a grid of icons — that scrolls to keep the focused item visible, pages with L/R, and costs the
  same with 1000 items as with 10, without allocating, so that big lists are safe on an Old 3DS.
- As a game developer, I want focus that behaves like a console game: held D-Pad repeats after a delay, a list wraps
  from the last item to the first on a fresh press but not while held, locked items are skipped (or shown and refused),
  a menu opens on its default item, and going back to a menu returns to the item I left, so that menus feel right
  without code.
- As a game developer, I want a dialog box that wraps text in a 9-slice frame and reveals it a few characters per
  frame, and a modal "Are you sure?" prompt that takes the focus and gives it back when closed, so that conversations
  and confirmations are a few calls.
- As a game developer, I want to style everything once — colours or 9-slice textures per widget state (normal,
  focused, pressed, disabled), padding, text colour and shadow, a focus cursor — and override a style on one widget,
  so that the whole game looks consistent and placeholder colours work before any art exists.
- As a game developer, I want widgets on either screen, anchored to an edge or the centre, on a layer I choose above my
  game, fixed on the screen whatever the camera does, so that HUDs (health bar, score) and menus sit where I put them.
- As a game developer, I want menu input to come from the controls I choose — plain buttons by default, or my
  spec 017 actions so remapped confirm/cancel buttons work — and the press that opened a menu not to activate its first
  button, so that menus respect the player's settings and never misfire.
- As a game developer, I want to hook sounds and animations to UI changes (a tick when focus moves, a click on
  activation, a buzz when a locked item is refused, a menu sliding in with a tween) without the UI depending on audio
  or tweens, so that feedback is my choice.
- As a game developer who will localise my game, I want every widget's text to be either a literal, my own buffer, or a
  key resolved by a function I give, re-resolved when the language changes, so that adding languages later needs no
  rewrite of my menus.
- As a game developer testing on the console, I want the same button presses and touches to give the same focus,
  values and pixels on PC and 3DS, and a way to log what the UI did, so that `.nnhs` scripts check menus exactly.
- As a game developer who draws my own menus, I want my game, its size and its build time to stay as they are.
- As a contributor, I want all of it tested without a console, on the fake platform.

## Functional requirements

### The UI object and its memory

- **FR-1 A UI the game owns.** A UI is an object the game creates and owns (a scene member, a game-wide object), never a
  hidden global. It is created by a `Result`-returning `create(desc)` in **one block** of tagged memory (tag chosen by
  the game, default an engine tag `ui`) sized from the description's capacities: widgets (1..4096), bytes of copied
  text, styles, and the visible-row pool of lists. Errors (`InvalidArgument`, `OutOfMemory`) are returned. It is
  movable, not copyable. A game may have several UIs (a HUD and a pause menu); they never share state. The plan states
  the bytes per widget (expected ≤ 96 on the 3DS) and the total for the example's screens.
- **FR-2 Built in code, retained.** Widgets are created by calls on the UI (`label`, `button`, `vbox`, …) that take a
  parent and a plain options value, and return a **widget handle** (a small generational value: using a destroyed
  widget's handle is detected and asserts, never undefined behaviour). The tree persists between frames; the game
  changes it with setters (text, value, enabled, visible, style, options) and may create or destroy widgets (a whole
  subtree at once) at any time within capacity — nothing allocates. Creation beyond capacity is an error `Result` (and
  one Debug/Development assert, as other modules' "full" cases decide in the plan). The public API shape (names, option
  structs) is part of the product and is fixed by the plan with an example in `docs/api/`.
- **FR-3 Roots per screen.** A UI has up to two **roots**, one per screen, each the full screen's rectangle (400×240,
  320×240); a root can be hidden. Every widget belongs to one root's tree. A UI used only on one screen has one root.
- **FR-4 Game keys.** Any widget may carry a game **key** (a small integer, normally the game's own enum value), unique
  within the UI (a duplicate asserts). Results, queries and focus memory can use keys, so a game switches on
  `MenuItem::Options` instead of comparing handles, and a rebuilt menu restores focus by key (FR-17).

### Widgets

- **FR-5 Display widgets.** **Label**: text with alignment (left, centre, right; top, middle, bottom), optional word
  wrap at its width (UTF-8 aware, breaking at spaces and after hyphens, `\n` honoured, a word longer than the width
  broken by characters), optional maximum lines with an ellipsis (`…`, in the built-in font), and a **visible
  character count** for typewriter reveal (FR-23). **Image**: a texture region (or a frame of a sprite library given
  as a region by the game) with a scale and tint, or nothing. **Progress bar**: a value in 0..max drawn as a fill
  (colour or 9-slice) over a background, horizontal or vertical, optional text ("7/10" or the game's). **Panel**: a
  background (FR-26) with padding, holding one child or acting as a box (FR-9). **Spacer**: empty space that can expand.
- **FR-6 Interactive widgets.** All are focusable unless disabled or hidden (FR-15):
  - **Button**: a label and/or an icon; activated by confirm or a touch released inside (FR-19).
  - **Toggle** (checkbox / on–off switch): confirm or a tap flips it; left/right optionally set off/on.
  - **Slider**: an integer value in min..max with a step (e.g. volume 0–10), changed by left/right (with repeat), by
    touching or dragging its track; optional value text and a custom text callback ("70 %", "Off").
  - **Choice** (spinner / option cycler): one of N options shown as text with arrows ("< Normal >"), changed by
    left/right (wrapping or not, per widget) or by tapping an arrow; options are text sources (FR-24).
  - **List**: rows of uniform height selected by up/down (FR-11).
  - **Grid**: cells of uniform size in N columns, moved in four directions (FR-11).
- **FR-7 Common widget values.** Every widget has: visible (hidden widgets take no space and are skipped), enabled,
  a minimum size (from its content: text measured through FR-24's seam, image size, padding), an optional fixed size,
  size flags (FR-9), a style override (FR-26), a **render offset and opacity** that move and fade it at draw time
  without changing layout (so a spec 015 tween by setter slides or fades a menu in; the UI never includes the tween
  header), and an optional key (FR-4).
- **FR-8 Dialog helper.** A convenience builds a **modal prompt** from the primitives — a panel with a message label and
  1..3 buttons ("Yes / No", "OK") — opened modally (FR-18) with a default button and a cancel answer, returning the
  chosen button through the normal results (FR-20). It adds no widget kind; the plan may make it a documented helper or a
  small function.

### Layout

- **FR-9 Boxes and size flags.** Containers lay out their visible children in integer pixels:
  - **VBox / HBox**: children stacked on one axis with spacing; on the cross axis each child is aligned (start,
    centre, end) or filled.
  - **Grid box**: N columns, row-major, column widths and row heights from the largest child, spacing per axis.
  - **Stack**: children on top of each other, each placed by an **anchor** (FR-10) inside the stack.
  - **Panel** (FR-5): padding around its content and a background.
  - Every container has padding. A child may **expand** along the main axis, sharing the extra space by integer
    **stretch ratios** (Godot's size flags), with the remainder pixels given in a fixed documented order so results are
    exact; a child that doesn't expand keeps its minimum or fixed size. Content larger than its container is not
    shrunk below minimums: it overflows, and Debug/Development logs one warning per widget (a layout bug, not a crash).
- **FR-10 Anchors.** A root's (and a stack's) direct children are placed by an **anchor**: one of nine points (corners,
  edge centres, centre) of the parent plus a pixel offset, or stretched between two anchors with margins (a full-width
  bar along the bottom edge). This is how a game puts a menu in the centre of the bottom screen or a health bar in the
  top-left corner of the top screen, independently of the screen size.
- **FR-11 Lists and grids with many items, without allocation.** A list (or grid) has an **item count** (0..65535)
  that can change at any time, and draws only the rows visible in its height, from a fixed **row pool** sized at
  creation. For each visible row the UI asks the game to fill it through a **row callback** (index → the row's texts,
  icon region, enabled flag, an optional value text) called only when a row comes into view or the game marks items
  changed; the game keeps its own item data. A list keeps the **focused item** visible: moving past the visible window
  scrolls by whole rows (with an optional margin of rows kept visible above and below, as many games do), **page**
  inputs (L/R by default) move a page, and a list reports its first visible index and item count for a scroll bar the
  style can draw. Rows are whole: no row is partly drawn (see Non-goals: clipping). Changing the item count clamps the
  focus and the scroll. A scroll offset can be set and queried by the game (restoring a list where the player left it).
- **FR-12 Scroll box.** A vertical **scroll box** holds ordinary widgets (an options screen with 15 rows on a 240-pixel
  screen) and shows the window of whole children that keeps the focused child visible, with the same margin rule;
  touch drag scrolls it (FR-19). Children outside the window are not drawn and not touchable.
- **FR-13 When layout happens.** Layout is computed only when something that affects it changed (text, visibility,
  size, item count, style metrics), during the UI's update (FR-21) or on an explicit query of a rectangle, never during
  drawing. Every rectangle is integer pixels in its screen's coordinates; the same tree gives the same rectangles on
  every system (no float in layout decisions). Queries give a widget's rectangle, its root and its visibility.

### Focus and navigation

- **FR-14 One focus per UI.** A UI has at most one **focused** widget (or none), shown by its focused style and an
  optional **focus cursor** (FR-27). Focus is set by the game (`focus(widget or key)`), by navigation (FR-16) or by touch
  (FR-19). Focus changes are reported (FR-20).
- **FR-15 What can take focus.** Interactive widgets (FR-6) are focusable; a list or grid is one focusable widget whose
  focused item moves inside it. Hidden widgets and widgets inside hidden containers or outside a scroll window's
  reach are never targets (a scroll box scrolls to reveal its next child). **Disabled** widgets are skipped by default;
  a UI or container option makes them focusable but inert (activating one reports a **refused** result, so the game
  can show "Locked" and play a buzz). A list row the callback marks disabled follows the same rule. If the focused
  widget becomes hidden, disabled (when skipped) or destroyed, focus moves to the nearest focusable widget in tree order
  within the same container, else the root's default, else none — never a dangling focus.
- **FR-16 Navigation.** Directions (up, down, left, right) move focus:
  1. an explicit **neighbour** set on the widget for that direction wins (Godot's focus neighbours);
  2. otherwise inside a box, along its axis (VBox: up/down; HBox: left/right; grid box, list, grid: both), in child
     order, skipping non-targets;
  3. otherwise (the cross axis, or past the end without wrap) the nearest focusable widget in that direction within the
     same root, by a fixed geometric rule on rectangles (the plan names it: overlap on the cross axis first, then
     distance), ties broken by tree order — deterministic;
  4. **wrap** (a per-container option, default on for lists, choices and vertical menus): past the last item to the
     first. By default wrap happens **only on a fresh press, not on a held repeat**, so holding Down stops at the
     bottom (an option allows wrapping on repeat too).
  Sliders and choices consume left/right (to change their value) and pass up/down on; a toggle passes all four. A move
  that finds no target reports a **blocked** result (for a bump sound) and keeps focus. Navigation never crosses from one
  root to the other unless an explicit neighbour says so.
- **FR-17 Default and remembered focus.** Each root and each container may name a **default** child. When focus enters
  a container (by navigation, by `focus` on the container, or when the UI becomes active), it goes to the container's
  **last focused** child if that is still a target, else its default, else its first target in tree order. A list
  remembers its focused index and scroll. The game can read the focus as a key or a handle and set it back later, so a
  title menu rebuilt after returning from a game restores "Continue" by key. A UI that is kept in a scene member keeps
  all of this across pushes and pops of other scenes with no code.
- **FR-18 Modal subtrees.** A container can be **opened modally**: while open, navigation, confirm/cancel and touch reach
  only it and its descendants (touch elsewhere is ignored, or reports cancel when the modal says so), and it draws above
  the rest of its root (FR-28). Closing it restores the focus held before it opened. Modals nest up to a documented
  depth (8 proposed).
- **FR-19 Touch on the bottom screen.** Only widgets in the bottom root react to touch. A touch that starts on an enabled
  focusable widget focuses it and shows its **pressed** style; releasing inside **activates** it (a button activates, a
  toggle flips, a choice arrow changes the value, a list row is selected and activated — an option makes a first tap
  only select and a second tap on the same row activate); sliding out cancels the press without activating. Dragging on
  a slider sets its value continuously from the touch x. Dragging on a list, grid or scroll box past a threshold
  (pixels, documented) scrolls it by whole rows and cancels the press. Touches on nothing, on disabled (skipped)
  widgets or on top-screen widgets do nothing. Touch and directional input work together: the next D-Pad press moves
  from the touched widget. An option hides the focus cursor after a touch until the next directional press (common on
  DS/3DS touch menus).
- **FR-20 Results: what happened this frame.** Each update produces a **results view** for this frame, in a fixed
  order: focus changed (from, to), **activated** (widget, key, list index), **value changed** (widget, key, new value,
  for toggles, sliders, choices), **refused** (a disabled widget activated), **blocked** (a move with no target),
  **cancelled** (FR-22), **page** moves. Queries answer the same per widget (`activated(key)`, `value(key)`) for the
  current frame. A widget may also have a **callback** (function pointer + context, as tween sets do) called during the
  update for its activation or value change; callbacks may change the UI (changes take effect for the next update's
  input, layout and focus; documented) but may not destroy the UI. The UI publishes **bus events** only if the game opts
  in per UI (see Open questions); the default is query and callbacks.

### Input

- **FR-21 Updating.** A UI's `update()` reads one frame of UI input, moves focus, changes values, recomputes layout if
  needed and fills the results view. It is called by the game once per frame (normally in the scene's `update()`,
  before reading results), or the UI is **bound to a scene** through D30 unchanged: `BeforeUpdate` runs its update (so
  the scene's `update()` reads this frame's results and a pause scene pushed on top freezes the UI under it) and
  `AfterExit` resets its transient state (presses, repeats, open touch) while keeping focus memory. A UI not updated
  does nothing and still draws.
- **FR-22 Where input comes from.** The UI reads a small plain **UI input** value each frame: four directions with
  their fresh-press and repeat flags, confirm, cancel, page previous/next, and touch (down, held, released, position).
  - **Default**: filled from raw input — D-Pad, and the Circle Pad as directions above a threshold (on by default,
    configurable), A confirm, B cancel, L/R page, the bottom screen's touch — with the UI's own **repeat** (initial delay
    and interval in frames, defaults 24 and 6 as spec 017's menu example; configurable per UI). Which buttons confirm and
    cancel is a UI setting (a game may swap A/B).
  - **The game's own**: the game may fill the value itself from anything (its spec 017 actions, an AI, a test), and a
    small documented adapter fills it from an `ActionSet`'s named actions, linked only when a game calls it (the UI
    header never includes the actions header).
  - **Cancel**: reported as a result; a container may name a widget that cancel **activates** (a "Back" button), and a
    modal (FR-18) may close on cancel.
  - **No leaking presses**: when a UI becomes active (its first update, after `AfterExit`, a scene binding becoming top,
    a modal opening), any confirm, cancel, direction or touch already held is ignored until released — the A that chose
    "Options" doesn't press the options screen's first button.
  - **Input blocks**: the UI reads the same sample as `<nnh/input.hpp>`, so under `input::set_blocked` or a spec 023
    transition it sees idle input; scripted and remote input (spec 013) drive it like a player.
- **FR-23 Text reveal.** A label's visible character count can be set by the game or advanced by the UI at a rate in
  characters per frame (a fractional rate kept exactly by a fixed-point accumulator, as particles' rates), with a query
  "fully shown" and a "show all now" call (the confirm that skips a typewriter). Wrapping is computed on the whole text,
  so words never jump lines while revealing.

### Text, styles and drawing

- **FR-24 Text sources (the fonts and localization seam).** Every text a widget shows comes from a **text source**:
  (a) a literal **copied** into the UI's text bytes (bounded; over capacity is an error), (b) a **borrowed** pointer the
  game keeps alive (its own buffer, updated in place, with `mark_text_changed`), or (c) a **key** (an integer or a
  hashed name) resolved by a **resolver function** the game sets on the UI. A UI-wide "text changed" call re-resolves
  every key and relayouts — the hook the localization spec uses when the language changes. All measuring, wrapping and
  drawing of text goes through one internal text seam whose only implementation today is spec 005's built-in font; a
  style's text part has a **font** field whose only value today is the built-in font, so the game-fonts spec adds fonts
  without changing widget calls or styles. Characters the font lacks draw as spec 005's hollow box, as everywhere.
- **FR-25 Number text.** Sliders, progress bars and list value columns format integers without `printf` (no allocation,
  the same digits everywhere), with an optional prefix/suffix text source or a game text callback for anything else.
- **FR-26 Styles and themes.** A **style** is a plain, trivially copyable struct per widget kind, with per-**state**
  values (normal, focused, pressed, disabled, and checked for toggles): a background (none, a solid colour with an
  optional 1-pixel border, or a **9-slice**: a texture region and four insets in texture pixels, the centre and edges
  stretched, drawn pixel-exact at integer positions with no seams), a text colour and shadow, a tint, padding, and
  kind-specific parts (slider track and knob, toggle on/off images, choice arrows, list row and scroll bar, progress
  fill). A **theme** is the set of default styles of a UI; the engine ships a default theme of solid colours that needs
  no texture (art-free, readable on both screens with the built-in font). A widget may point at another style in the
  UI's style table. Changing a style relayouts only if its metrics changed. Style values use only kinds spec 014 can
  load, so a game can keep its theme in a data file by declaring its own data type (documented pattern, Open questions).
- **FR-27 Focus cursor and feedback.** Besides the focused style, a style may draw a **focus cursor**: a texture region
  (a hand, an arrow) placed at an anchor of the focused widget, and/or an outline. The cursor may **move smoothly**
  between widgets over N frames with one of `<nnh/ease.hpp>`'s easings and may **pulse** (an opacity or offset cycle),
  computed on the frame clock with integer-rounded positions, the same on every system. A pressed widget shows its
  pressed style for at least a few frames (documented) after a confirm, so a press is visible.
- **FR-28 Drawing.** `draw()` records the UI's visible roots with spec 004's public drawing calls (`gfx::draw`,
  `fill_rect`, `draw_rect`, `draw_text`'s glyphs) on each root's screen, always with `ignore_camera` (never moved or
  zoomed by `Camera2D`), on the root's **layer** (0–15; default 14, below spec 023's transition overlay on 15). Within a
  root, parents draw before children in tree order; an open modal and the focus cursor draw last. `draw()` only reads:
  it never changes focus, layout or values. Each drawn element costs what the same gfx call would (glyphs count against
  `max_draws`, spec 005); the UI reports how many draws its last `draw()` recorded. The UI draws in a frame like any
  game draw (calls outside a frame are ignored, as gfx's).

### Determinism, debugging, cost, safety, opt-in

- **FR-29 Same results everywhere.** Layout, focus, navigation, repeats, values, scroll positions, text wrapping and
  reveal, cursor animation and the recorded draws depend only on the tree, the styles and the per-frame UI input, in
  integer arithmetic (or `<nnh/ease.hpp>`'s bit-identical curves rounded once), never on wall time. The same calls and
  inputs give the same results and the same recorded draw items on PC (every supported host compiler) and on the 3DS.
- **FR-30 Queries and debugging.** Queries: focused widget and key, a widget's rectangle, state, value, text, visible
  and enabled flags, a list's focused index, first visible index and count; `find(key)`. Debug/Development tools: a
  debug draw of widget rectangles and the focus path, and a one-call text dump of the tree (kind, key, rectangle, state,
  value) to the log, for test scripts and bug reports; compiled out of Release. Stats: widgets used and capacity, text
  bytes, last layout cost, draws recorded.
- **FR-31 No allocation, bounded cost.** After `create`, nothing allocates. Update cost is proportional to the input's
  effect (a focus move visits the widgets the rule needs, a list's cost doesn't grow with its item count), plus a
  relayout only of what changed (the plan may relayout a whole root; the budget is AC-6's). No `virtual` per widget; the
  only game code called is row callbacks, text callbacks, the resolver and widget callbacks.
- **FR-32 Misuse is loud in Debug, harmless in Release.** A stale handle, a duplicate key, a widget added under a
  non-container, a cycle, a slider with min > max, a step of 0, a list without a row callback, a modal depth overflow,
  NaN or infinite render offsets or opacities, destroying the UI from its own callback: assert in Debug/Development and
  do nothing (or clamp, documented) in Release; never undefined behaviour.
- **FR-33 Optional and light.** The UI is an optional module (XI) with one public header that includes only light
  engine headers (math, gfx types, `Result`, `<nnh/ease.hpp>` if needed); it includes no actions, tween, data,
  animation, entities or audio header. Nothing in gfx, input or scenes names the UI. A game that doesn't include it
  links nothing new and `examples/hello` keeps its size on PC and 3DS.

## Acceptance criteria

- **AC-1 Unit tests first** (constitution VI), on the fake platform: creation and capacities, handles and stale use,
  keys; every widget's minimum size; VBox/HBox/grid box/stack/panel layout with padding, spacing, alignment, expand and
  stretch ratios (remainder pixels in the documented order), anchors (nine points and stretched) on both screen sizes,
  overflow warning; label wrapping (spaces, hyphens, `\n`, long words, UTF-8 Latin-1, max lines with ellipsis) and
  reveal (fractional rates, "show all"); navigation in every container kind, explicit neighbours, the geometric rule and
  its tie order, wrap on press vs repeat, disabled skipped vs inert (refused), hidden widgets, focus repair after hide/
  disable/destroy, default and remembered focus, focus by key after a rebuild, modals (scope, nesting, focus restored),
  cross-root navigation only by neighbours; sliders, toggles and choices by direction and touch; lists and grids
  (scrolling by whole rows, margins, pages, count changes, the row callback called only for rows coming into view, a
  list of 65 535 items); scroll box; touch press/release/slide-out/drag/threshold, tap-to-select option, cursor hiding
  option; repeat timing; no leaking presses (held confirm when activated, after a modal opens, after a scene binding);
  input blocked by `input::set_blocked`; custom UI input; cancel and back widgets; results order and callbacks
  (including a callback that changes the tree); scene binding through D30 (updated before `update()`, frozen under a
  pause scene, reset after exit with focus memory kept); text sources (copy, borrowed, key + resolver, "text changed");
  number formatting; styles per state, 9-slice geometry (insets, stretched parts, no seams at any size ≥ the insets),
  default theme; cursor easing and pulse in frames; drawing on the fake platform (items equal to the equivalent
  hand-written gfx calls, layers, `ignore_camera`, unaffected by camera position and zoom, modal and cursor last,
  hidden roots draw nothing); misuse cases (FR-32). The Linux build VM runs them with GCC 13/14 and clang 18 and with
  ASan/UBSan, as previous specs did.
- **AC-2 Golden session.** A test drives a UI with several screens (title menu, options with slider/toggle/choice, a
  1000-item list, a grid, a dialog with reveal, a modal prompt) through a fixed script of 3600 frames of UI input
  (directions with holds, confirms, cancels, pages, touches and drags at fixed points) and compares, every 600 frames, a
  checksum of every widget's rectangle, state, value, the focus, the lists' scroll, the results and the recorded draw
  items with golden values. The same test passes on every host compiler and in the console self-check (AC-4).
- **AC-3 Example `examples/ui`** (original art or the default theme only), modes chosen by `UI_DEMO` (`showcase`
  (default) | `check` | `bench`): a title menu on the bottom screen with the game's name on the top; an options screen
  (music and sound sliders, a toggle, a difficulty choice, a disabled "Online" item, Back) inside a scroll box; an
  inventory as a list of 300 items (icon, name, count) with the selected item's description in a 9-slice panel on the
  top screen; a grid of icons; a dialog box with typewriter text; a "Quit?" modal prompt; a HUD health bar and score
  anchored on the top screen over a moving background; one theme with 9-slice textures and the default theme, switched
  with SELECT. A tween slides the options screen in (by setter). The log prints one line per result
  (`ui: activated options.back`, `ui: value music 7`, `ui: refused online`). PC screenshots (title, options with focus
  on the slider, the list scrolled to item 250, the grid, the dialog half revealed, the modal, both themes), checked by
  reading the PNGs.
- **AC-4 Console script.** The `check` mode runs AC-2's session on the real UI and logs `ui: <frame> sum <checksum>`
  every 600 frames and `ui: PASS <checksum>` or `ui: FAIL ...`, against golden values recorded from the simulator.
  `examples/ui/tests/check.nnhs` starts it, expects every line, then switches to the showcase and **presses** the D-Pad
  (held for repeats), A, B, L/R and **touches** and drags fixed points on the bottom screen, expecting the logged
  results (for example `ui: focus list 12`, `ui: value music 3`, `ui: activated quit.no`) and taking screenshots. On an
  Old 3DS the maintainer only opens the Homebrew Launcher and presses Y, and
  `nnh run ui --script examples/ui/tests/check.nnhs` passes; the same script passes with `--pc`. The verdict, log and
  screenshots are in the saved log folder.
- **AC-5 Pixels.** The showcase screenshots of the simulator and the console's screenshots of the same states show
  widgets at the same pixel positions (text and solid colours identical; 9-slice texels per spec 004's rasterisation,
  unrotated so expected identical) — compared by the script's screenshots and recorded in the plan.
- **AC-6 Cost on the console.** The `bench` mode, measured on an Old 3DS with the profiler and logged as
  `ui bench: PASS <numbers>` (or FAIL). **Targets** (confirmed by the plan before code; then the maintainer's bench
  policy: optimise first, else set the budget from the first console measurement with margin and write it in the spec
  and docs):
  - `update()` of a UI with **128 widgets** and no input: **under 0.05 ms**; with a focus move: **under 0.15 ms**; with a
    full relayout of both roots (text changed): **under 0.6 ms**;
  - a **list of 10 000 items** scrolling one row per frame (the row callback filling 12 visible rows): **under 0.2 ms**
    per update, and within 10 % of the same list with 20 items;
  - `draw()` of the options screen (2 9-slice panels, 12 rows, about 350 glyphs): the UI's own CPU cost per recorded
    draw **at most 1.15×** the same draws written by hand in the same run, and the screen plus **300 game sprites** on
    the top screen at 60 Hz with no missed frame of 600;
  - memory: the plan states bytes per widget (≤ 96 expected) and the example's blocks.
  Spec 012's tile gate, the sprites bench and the platformer's 60 Hz replay verdict still pass.
- **AC-7 Opt-in.** `examples/hello` builds with the same executable size as before on PC and 3DS; `examples/sprites`,
  `examples/text`, `examples/actions`, `examples/transitions` and `examples/platformer` keep their screenshots
  pixel-identical and their scripts and goldens passing; every earlier spec's tests pass unchanged. A game that includes
  the UI header but never calls the actions adapter links no action code (checked on the 3DS map file or symbol list).
  Building a game `.cpp` that includes the UI header stays within constitution III's budgets (measured before and after
  on `examples/ui`).
- **AC-8 Docs.** A new `docs/api/ui.md` (creating a UI and its capacities, widgets, layout and anchors, both screens,
  focus rules, remembered focus, modals, touch, input sources and actions, results and callbacks, text sources and the
  fonts/localization seam, styles, themes and 9-slice, the cursor, drawing and layers, scene binding, determinism,
  debugging, costs, misuse); a guide `docs/guides/menus.md` (title menu, options, inventory, dialog, prompt, HUD,
  animating with tweens, sounds from results, a theme in a data file); `docs/api/gfx.md`, `docs/api/text.md`,
  `docs/api/actions.md` and `docs/api/tween.md` linking the UI where relevant; a decision row **D48** in
  `docs/architecture/decisions.md`; CLAUDE.md layout and ROADMAP updated; `docs/REFERENCES.md` citing the prior art
  below.

## Non-goals

- **An editor or a layout file format**: menus are C++ calls (constitution XI); themes may be data files by the
  documented pattern only.
- **Text entry and on-screen keyboards**: the 3DS has a system keyboard applet (`swkbd`), a candidate for a
  `<nnh/3ds/...>` extension; a portable keyboard widget is a follow-up.
- **Game fonts, kerning, proportional fonts, rich text and markup** (colour spans, inline icons), right-to-left and CJK:
  the game-fonts and localization specs; this spec only provides the seam (FR-24).
- **Clipping and smooth (sub-row) scrolling**: gfx has no clip rectangle today; lists and scroll boxes move by whole
  rows and children (Open questions).
- **Mouse hover, keyboard text focus and PC-only behaviour**: the simulator's mouse is the touch screen, as everywhere.
- **Drag and drop, tooltips, context menus, tree views, tables with sortable columns, rotation of widgets.**
- **Several focuses at once** (split-screen multiplayer menus) and several players' inputs.
- **Screen readers and text-to-speech**; accessibility here means repeat, configurable timing, swap of confirm/cancel,
  touch and D-Pad parity, and readable defaults.
- **3D or stereoscopic depth for widgets** (M8): UI draws at screen depth.
- **Widgets as entities or nodes** (specs 020, 022): a UI is its own tree; a game can draw a widget at a node's screen
  position by moving it.
- **Changing existing examples or `apps/nnh-link`** to use widgets (Open questions).
- **Bus events by default** (FR-20; Open questions).

## Constraints

- **I** — portable core code, no platform part and no platform `#ifdef`; layout and focus are the same on PC and 3DS
  (FR-29); the example and scripts run in the simulator and on the console.
- **II** — one block per UI sized at creation; bytes per widget and per list row stated by the plan; costs measured on an
  Old 3DS (AC-6). Text is the expensive part on the 3DS (one sprite per glyph, about 12 µs each on an Old 3DS as
  measured by the sprites bench), so budgets are stated with glyph counts.
- **III** — one light header (no standard containers, `<functional>`, `<string>`); `hello` unaffected; build times
  measured (AC-7).
- **IV** — state is queried (focus, values, results view); changes reach the game through the results view and
  callbacks the game registered; bus events only if opted in. The UI talks to gfx and input only through their public
  APIs.
- **V** — no exceptions (`Result` at creation and for capacity errors), no RTTI, no `virtual` per widget, no allocation
  after creation, no `printf` formatting in the update or draw path.
- **VI** — layout, navigation, input handling, text wrapping and drawing records unit-tested first on the fake platform
  with a golden session (AC-1, AC-2); visible behaviour by screenshots and on the console (AC-3 to AC-5).
- **VII** — layout, navigation, touch, input source, lists, text, styles/9-slice, cursor and drawing in small separate
  files; one public header for the module (the plan decides whether the dialog helper and actions adapter share it).
- **VIII** — header comments, `docs/api/ui.md`, the menus guide, linked pages and `examples/ui`.
- **IX** — written from published designs (Godot's and Unity's documentation, Clay's and microui's READMEs); no code
  copied; original art only; credited in `docs/REFERENCES.md`.
- **X** — no new external input parser (text comes from the game; themes from data files go through spec 014's bounded,
  fuzzed loader); misuse loud in Debug/Development and harmless in Release (FR-32).
- **XI, D25** — optional: the UI without actions, tweens, data assets, entities or audio; plain `gfx` menus stay
  supported.
- **Under pressure:**
  - **`max_draws` and glyph cost.** A text-heavy screen spends hundreds of draws (FR-28). Recording through public gfx
    calls keeps the platform contract unchanged; if AC-6's draw target is missed, the fallback is a UI batch in the draw
    list through D39's batch mechanism (a second user of it), not a contract change (Open questions).
  - **Layer 15** belongs to spec 023's transition overlay; the UI's default layer is 14 so a transition covers menus.
  - **The fonts/localization seam** (FR-24) commits two future specs to plug in without changing widget calls; their
    plans must fit it or amend it.
  - **D30** gains another user (scene binding), unchanged.
  - **Spec 017's actions** must stay optional: the adapter is a separately linked function (as spec 021's map code).
  - **No clip rectangle in gfx**: whole-row scrolling is a visible limitation of v1.

## Prior art

- **Godot** `Control` nodes: anchors and offsets, size flags with stretch ratios, containers (`VBoxContainer`,
  `HBoxContainer`, `GridContainer`, `MarginContainer`, `PanelContainer`, `ScrollContainer`), `NinePatchRect`, themes
  with per-state styleboxes, focus neighbours and automatic focus guessing, `grab_focus()` at start —
  [Control](https://docs.godotengine.org/en/stable/classes/class_control.html),
  [Using containers](https://docs.godotengine.org/en/stable/tutorials/ui/gui_containers.html),
  [Keyboard/Controller Navigation and Focus](https://docs.godotengine.org/en/stable/tutorials/ui/gui_navigation.html),
  [NinePatchRect](https://docs.godotengine.org/en/stable/classes/class_ninepatchrect.html). FR-9/FR-10's boxes, flags
  and anchors, FR-16's explicit neighbours and FR-26's per-state styles follow it; FR-17's default focus answers its
  "nothing is focused at start" caveat.
- **Unity** uGUI: `Selectable` navigation modes (Automatic, Horizontal, Vertical, Explicit; wrap around), layout groups
  with child force-expand, 9-sliced sprites —
  [Navigation Options](https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/script-SelectableNavigation.html),
  [Auto Layout](https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/comp-UIAutoLayout.html),
  [9-slicing](https://docs.unity3d.com/Manual/9SliceSprites.html). FR-16's geometric fallback and wrap follow it.
- **Clay** (Nic Barker): flexbox-style layout in a fixed arena with no allocation during layout, integer-friendly sizing
  rules — [github.com/nicbarker/clay](https://github.com/nicbarker/clay). FR-1's one block and FR-13's layout pass.
- **microui** and **Dear ImGui**: immediate-mode UIs (widgets declared every frame, ids for state) —
  [github.com/rxi/microui](https://github.com/rxi/microui), [github.com/ocornut/imgui](https://github.com/ocornut/imgui).
  Considered and not chosen for focus memory and controller navigation (Open questions, first item).
- **Console conventions** (DS/3DS games, Nintendo-style menus): held-direction repeat, wrap on fresh press only, L/R
  pages, A confirm / B back, touch and D-Pad parity on the lower screen — FR-16, FR-19, FR-22.

## Open questions

Every question has a recommended answer; none is decided until the maintainer approves the spec.

- [ ] **Retained tree or immediate mode?** (a) **a retained tree built once (in `on_enter`) and changed by setters**; (b)
      immediate mode (widgets declared every frame with ids, as microui/ImGui). — **Recommended: (a)** (FR-2). Focus
      memory, geometric navigation, layout caching and the "no leaking press" rule need state across frames; Godot and
      Unity are retained; immediate mode re-measures text every frame (expensive glyph walks on the Old 3DS).
- [ ] **Input source: raw buttons or spec 017 actions?** (a) **a plain UI input value per frame, filled by default from
      raw input (D-Pad, Circle Pad, A/B, L/R, touch) with the UI's own repeat, or by the game; a separately linked
      adapter from an `ActionSet`**; (b) actions required (the UI needs an `ActionSet`); (c) raw only. — **Recommended:
      (a)** (FR-22). Remapped confirm/cancel work for games that use actions, games that don't pay nothing, tests can
      feed the value directly, and XI holds.
- [ ] **How results reach the game.** (a) **a per-frame results view + per-key queries + optional per-widget callbacks;
      bus events opt-in per UI** (`UiActivated`, `UiValueChanged`, `UiFocusChanged`, `UiCancelled`, ≤ 32 bytes); (b)
      bus events always; (c) callbacks only. — **Recommended: (a)** (FR-20). Results are read in the same frame's
      `update()` (bus events published during an update arrive at the next frame's dispatch); events stay available for
      listeners that don't own the UI (an audio scene playing clicks).
- [ ] **Who calls `update()`?** — **Recommended: the game, or a D30 scene binding (`BeforeUpdate` updates, `AfterExit`
      resets transient state)**, as tween sets and particle emitters (FR-21).
- [ ] **Drawing path.** (a) **plain public gfx calls (sprites, rectangles, glyphs) on the root's layer**; (b) a UI batch
      in the draw list through D39's mechanism (one command per root, expanded at submit). — **Recommended: (a), with (b)
      as the plan's fallback only if AC-6's draw target is missed** (FR-28). (a) needs no gfx change and makes the
      fake platform's records directly comparable to hand-written menus.
- [ ] **Clipping and smooth scrolling.** (a) **whole-row/whole-child scrolling now; a gfx clip rectangle (scissor; a
      contract change every platform implements) as a follow-up spec, after which lists may scroll by pixels**; (b) add
      the clip rectangle in this spec. — **Recommended: (a)** (FR-11, FR-12). Keeps this spec free of platform work;
      many console menus scroll by rows anyway.
- [ ] **Text sources and the fonts/localization seam.** — **Recommended: copied, borrowed, or key + resolver, with a
      UI-wide "text changed"; a font field in text styles whose only value is the built-in font** (FR-24). The
      localization spec then supplies a resolver and calls "text changed" on a language switch; the fonts spec adds
      font values. Neither changes widget calls.
- [ ] **Themes in data files.** (a) **C++ style structs whose members are data-loadable; a game that wants a theme file
      declares its own data type and converts it (documented pattern, as spec 024's preset fallback)**; (b) an
      engine-shipped opt-in schema source; (c) C++ only. — **Recommended: (a)**. Avoids amending D29 (spec 024 found
      engine-shipped schemas don't fit cleanly) while still allowing data-only theme edits.
- [ ] **Touch semantics.** — **Recommended: press shows pressed, release inside activates, sliding out cancels; lists
      activate on a single tap by default with a "tap to select, tap again to activate" option; drags past a threshold
      scroll and cancel the press; the focus cursor stays visible after a touch by default, with an option to hide it**
      (FR-19).
- [ ] **Disabled items.** — **Recommended: skipped by navigation by default; an option makes them focusable but inert,
      reporting "refused"** (FR-15). Locked menu entries that explain themselves are common; skipping is the safer
      default.
- [ ] **Wrap behaviour.** — **Recommended: wrap on a fresh press only, not on a held repeat; per-container switch; an
      option to wrap on repeat too** (FR-16).
- [ ] **Navigation across the two screens.** — **Recommended: never automatic; only through explicit neighbours**
      (FR-16). Most dual-screen menus show information on the top screen and the controls on the bottom.
- [ ] **A tabs/pages container** (one child shown at a time, switched by L/R or by tapping a header row — common on
      3DS options and inventory screens). — **Recommended: not in this spec**; L/R page results (FR-20) and hiding
      panels give it in a few lines, documented in the menus guide; a container is a follow-up if games repeat that
      code.
- [ ] **9-slice centre and edges stretched or tiled?** — **Recommended: stretched only** (FR-26): 9 draws per panel;
      tiling multiplies draws by the panel size; a tiled mode is a follow-up.
- [ ] **Adopting widgets in existing programs** (the platformer's pause, `examples/saves`' slot screen, `apps/nnh-link`'s
      lists). — **Recommended: not in this spec**; `examples/ui` owns the demonstration, self-check and bench, and the
      platformer's replay lines stay untouched. NNH Link could adopt it in a follow-up once the API settles.
- [ ] **Old 3DS cost targets** — **Recommended: AC-6's targets, confirmed by the plan, then the maintainer's bench policy**
      (optimise, else set from the first console measurement with margin).
- [ ] **Capacities and bytes per widget** (≤ 96 B proposed, widgets ≤ 4096, modal depth 8, list row pool per list) — the
      plan fixes them from a first measurement.
- [ ] **Decision row** — **Recommended: D48** ("UI: a retained widget tree in one block, integer layout, focus rules,
      input through a UI input value, drawing through public gfx calls"), written by the plan.
