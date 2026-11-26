# UI widgets

`<nnh/ui.hpp>` (spec 028, decision [D48](../architecture/decisions.md)): title menus, options screens, inventories,
dialogs, prompts and HUDs built **in C++** from labels, buttons, toggles, sliders, choices, lists, grids, images,
progress bars, panels and layout boxes. A `Ui` is a **retained tree in one block** the game owns: built once (usually
in a scene's constructor or `on_enter`), changed by setters, laid out in **integer pixels** only when something
changed, navigated by the **D-Pad, Circle Pad and touch**, read by **query or callback**, and drawn through the same
public gfx calls a hand-written menu makes. The same presses give the same focus, values, rectangles and draws on PC
and 3DS. Optional: a game that doesn't include the header links nothing of it.

Demo: [`examples/ui`](../../examples/ui/main.cpp) (title, options slid in by a tween, a 300-item inventory, a grid
gallery, a typewriter dialog, the quit prompt, a HUD, a 9-slice theme and the default one, the self-check and the
bench). Guide: [menus](../guides/menus.md).

```cpp
#include <nnh/ui.hpp>

enum class Item { NewGame, Options, Quit };

class Title : public nnh::Scene {
public:
    Title() {
        auto made = nnh::Ui::create({.widgets = 16});                       // one block, ~2 KB
        if (!made) return;                                                  // made.error().message says why
        ui_ = std::move(made.value());
        const nnh::UiWidget menu = ui_.vbox(ui_.root(nnh::Screen::Bottom),
                                            {.layout = {.width = 160, .place = {.anchor = nnh::UiAnchor::Center}}});
        ui_.button(menu, {.text = "New game", .key = Item::NewGame});
        ui_.button(menu, {.text = "Options", .key = Item::Options});
        ui_.button(menu, {.text = "Quit", .key = Item::Quit});
        ui_.bind(*this); // updated right before update(), input reset after on_exit()
    }
    void update() override {
        if (ui_.activated(Item::Options)) nnh::scenes::push(options_);
        if (ui_.activated(Item::Quit)) nnh::scenes::pop();
    }
    void draw() override { ui_.draw(); }

private:
    nnh::Ui ui_;
    // ...
};
```

## A UI and its capacities

`Ui::create(desc)` returns a `Result`: `InvalidArgument` naming the field, or `OutOfMemory`. It allocates **one
block** of `desc.tag` (default `ui`) and nothing afterwards. A `Ui` is movable, not copyable; destroying it frees the
block. A game may have several (a HUD and a pause menu): they never share state.

| `UiDesc` field | Range | Default | Bytes |
|---|---|---|---|
| `widgets` | 1..4096 (roots included) | 64 | 88 per widget on the 3DS (96 on 64-bit PCs): a 32-byte hot node + a cold record |
| `text_bytes` | 0..65535 | 1024 | copied text, each copy + 1 (compacted when a widget is destroyed) |
| `texts` | 0..4096 | 0 | extra text sources: a choice's options (one each) |
| `styles` | 0..240 | 0 | custom styles (`add_style`), about 600 B each |
| `rows` | 0..1024 | 0 | pooled list and grid rows: 72 B each on the 3DS; a list takes `rows × columns` |
| `functions` | 0..255 | 8 | callbacks, row fills, formatters: 24 B each on the 3DS |
| `results` | 8..256 | 32 | results kept per frame |
| `top`, `bottom` | at least one | both | the roots (one per screen: 400×240, 320×240) |
| `first_focus` | a created root | `Bottom` | the root the focus enters first |
| `input` | see [Input](#input) | | repeat, Circle Pad thresholds, buttons |

The header of the block is about 0.5 KB. `stats().block_bytes` gives the total. A full table (widgets, text bytes,
texts, rows, functions) makes the creating call return a **null handle** with an assert in Debug/Development and one
error log per UI in Release (the maintainer's answer to plan question 2): size the UI like `max_draws`.

## Widgets

Each kind is created by one call taking the **parent** and a plain options struct (designated initializers); every
options struct has `key`, `style` and `layout`. New widgets are appended as their parent's last child. Every call
returns a `UiWidget` — spec 020's 32-bit handle (16-bit slot, 16-bit generation): `destroy(w)` frees `w` and its
subtree at once, and every later use of its handle asserts (Debug/Development) and does nothing (Release). Handles of
another `Ui` are not detected.

| Call | What | Notes |
|---|---|---|
| `label(parent, {.text, .wrap, .max_lines, .align, .valign, .reveal_rate})` | text | word wrap at its width (spaces dropped at a break, a break after `-`, `\n` honoured, long words by characters); `max_lines` ends with `...` (three cells); reveal: [Typewriter](#typewriter-text) |
| `image(parent, {.texture, .region, .scale, .tint})` | a texture region | `texture` is a slot (`set_texture`), −1 draws nothing |
| `progress(parent, {.value, .max, .vertical, .show_text, .format})` | a bar | fill over its background; text `"7/10"` or the formatter's; give it a size |
| `panel(parent, {.as_box, .box, .spacing})` | a background | the style's padding around one child, or a VBox/HBox (`as_box`) |
| `spacer(parent, layout)` | empty space | expands by default |
| `button(parent, {.text, .icon_texture, .icon})` | activated by confirm or a touch released inside | icon and text centred |
| `toggle(parent, {.text, .checked, .left_right_set})` | on/off | confirm or a tap flips it; Left sets off, Right on (only when that changes it) |
| `slider(parent, {.text, .min, .max, .step, .value, .show_value, .format})` | an integer | Left/Right step it (with repeat), a touch on its track sets it; at a limit: `Blocked` |
| `choice(parent, {.text, .options, .count, .index, .wrap})` | one of N | Left/Right step (wrapping or blocked), confirm steps forward (wrapping), tapping an arrow steps |
| `list(parent, {.rows, .count, .fill, .margin, .wrap, .tap_selects_first, .row_height, .cell_width})` | many items | `columns > 1` makes a **grid**; see [Lists and grids](#lists-and-grids) |
| `vbox`, `hbox`, `grid_box`, `stack` (`{.spacing, .padding, .columns, .wrap, .wrap_on_repeat, .disabled_focusable}`) | boxes | see [Layout](#layout) |
| `scroll_box(parent, {.spacing, .padding, .margin})` | a window of whole children | see [Scroll boxes](#scroll-boxes) |

Children go only under containers (roots, panels, boxes, stacks, scroll boxes); a non-box panel holds one. Creation
also refuses a duplicate key, a slider with `min > max` or `step < 1`, a choice without options, a list without a fill
function or with more rows than the pool has left.

**Common values** (setters; each marks only what it affects): `set_text`, `set_visible` (hidden widgets take no space
and never take focus), `set_enabled` (disabled: see [Focus](#focus)), `set_value` (slider, progress, toggle 0/1, choice
index, list item; reports nothing), `set_progress_max`, `set_style`, `set_layout`, `set_key`, `set_neighbour`,
`set_default`, `set_back`, `set_render_offset` and `set_opacity` (move and fade a widget and its subtree **when drawn**,
never relaying out: a spec 015 tween by setter slides or fades menus, see the guide), `on_activate` and `on_change`.

**Keys** (`UiKey`): the game's own enum values (0..65534), unique in a UI. Results carry them, `find(key)`,
`focus(key)`, `activated(key)`, `value(key)` use them (linear scans: not for inner loops).

**Queries**: `alive`, `kind`, `key`, `rect` (completes a pending layout), `screen`, `visible` (it and every ancestor),
`enabled`, `state` (`Normal`, `Focused`, `Pressed`, `Disabled`, `Checked`), `text`, `value`, `list_info`, `focused`,
`focused_key`, `modal_depth`, `fully_shown`, `revealed`, `text_cells`, `stats`. A stale handle answers zero/none (and
asserts in Debug/Development).

## Layout

Layout is integer and per root, computed only after something marked it dirty (text, visibility, size, style metrics,
item count, children): at the end of `update()`, on a `rect()` query, and at the start of `draw()` for changes made
after `update()` (the maintainer's answer to plan question 1: a menu that switches panels after reading its results
draws the new ones in the same frame). `draw()` still changes nothing else. A layout walks each visible node four times
(`stats().layout_visits`), without recursion: minimum widths bottom-up, widths top-down, wrapping and minimum heights
bottom-up, heights and positions top-down. A root with no wrapping label needs no width to know its heights, so it
takes three: the minimum heights come with the minimum widths.

- **Minimum sizes** come from content plus the style's padding (labels: 8 px per character and 10 px per line at
  scale 1; a wrapping label: its longest word; buttons: text and icon; toggles: a 10-pixel box + 4 + text; sliders:
  label + 4 + a 48-pixel track + 4 + the widest value; choices: label + arrows + the widest option; lists: `cell_width`
  (else 64) × columns + the scroll bar, `rows × row_height`). `layout.width`/`height` replace them.
- **VBox / HBox** stack visible children with `spacing` and `padding`. Along the axis, children with `expand` share the
  extra space by `stretch` (1..255): a child's share is `extra × stretch / Σstretch`; one whose minimum is larger is
  kept at its minimum and the rest recomputed; the remainder pixels go one each to the expanding children, **first
  child first**. Across the axis each child is `Start`, `Center` (rounded down), `End` or `Fill` (`layout.align`,
  default `Fill`).
- **Grid box**: `columns` columns in row-major order; column widths and row heights are their largest child's.
- **Stack** and **roots** place each child by `layout.place` (FR-10): one of nine anchors on the parent's same point
  plus `(x, y)`, or `fill_x`/`fill_y` between margins `x`/`right` and `y`/`bottom`.
- **Panel**: the style's padding around its child (or a box).
- **Overflow**: content larger than its container is never shrunk; Debug/Development logs one warning per widget.

## Focus

One widget has the focus (or none). **Targets** are buttons, toggles, sliders, choices, and lists/grids with items;
visible with every ancestor; inside the open modal; enabled — or disabled under a box with `disabled_focusable` (then
**inert**: activating it reports `Refused`, e.g. a "Locked" sound). Disabled widgets are skipped otherwise. When the
focused widget is hidden, disabled (and skipped) or destroyed, the focus goes to the next target among the following
siblings, else the previous ones, else the root's default, else none: it never dangles.

A direction is resolved in this order (FR-16):

1. the focused widget's own use: a slider or choice takes Left/Right, a list or grid moves its item;
2. an explicit **neighbour** (`set_neighbour`) — the only way to the other root;
3. the ancestors, nearest first: the first box whose axis matches (VBox and scroll box: Up/Down; HBox: Left/Right;
   grid box: all four, by columns) steps to its next child that holds a target, **wrapping at its end only on a fresh
   press** (holding Down stops at the bottom) unless `wrap_on_repeat`;
4. the **geometric rule** within the root or the open modal: targets entirely beyond the focused rectangle's edge;
   those overlapping it across first; then the smallest gap; then the smallest distance between centres across; then
   tree order;
5. else `Blocked` (for a bump sound), the focus kept.

**Entering** a container (a step, a neighbour, `focus(container)`, the first update) goes to its **last focused**
child if still a target, else its **default** (`set_default`), else its first target in tree order. Lists remember
their item and scroll. A UI kept in a scene member keeps all of this across pushes and pops of other scenes with no
code; a rebuilt menu restores its focus with `focus(key)`.

**Modals**: `open_modal(container, {.cancel_closes, .outside_touch_cancels})` restricts navigation, confirm, cancel
and touch to that subtree, draws it after the rest of its root and gives the focus back on `close_modal()`. Up to 8
nest. `prompt(screen, {.message, .buttons, .keys, .count, .default_button, .cancel_button, .close_on_answer})` builds
a modal "Quit? Yes / No" from a panel, a wrapping label and 1–3 buttons (linked only when called); the answer is the
button's `Activated` result, and with `close_on_answer` the prompt closes itself at the end of that update.

## Input

`update()` reads one `UiInput` (held and pressed bits of Up, Down, Left, Right, Confirm, Cancel, PageBack,
PageForward; touch) from, in order: the game's source (`set_input_source(fn)`), the actions adapter
(`use_actions(set, map)`: buttons from button actions, a vector action as four directions past 0.5 / below 0.4, the
touch screen raw; the only UI file including `<nnh/actions.hpp>`, linked only when called), else the **default**: the
D-Pad, the Circle Pad past 0.5 (let go below 0.4; `UiInputConfig::circle_pad`), `confirm`/`cancel` (A/B, swappable),
`page_back`/`page_forward` (L/R) and the bottom screen. `update(input)` takes one directly (tests, AI, replays).

- The UI makes its own **repeat** for every source: a direction or page fires on its press, then at `repeat_delay`
  (24) and every `repeat_interval` (6) frames held; one direction per frame (the newest pressed). `repeat_delay = 0`
  takes `UiInput::repeated` as given.
- **No leaking presses**: when the UI becomes active — its first update, after `reset_input()` (and a binding's
  `AfterExit`), after a gap in `frame_count()` (a scene above it was popped), after a modal opens — every control
  already held, and a held touch, is ignored until released. The A that chose "Options" never presses the options
  screen's first button.
- The default source reads the public input queries, so `input::set_blocked` and a transition's block make it idle.
- **Cancel** activates the `back` widget (`set_back`) of the innermost container around the focus, else closes an open
  modal with `cancel_closes`; `Cancelled` is reported in every case.
- **Touch** (bottom root only; an open modal first): a touch on a target focuses it and shows it pressed; releasing
  inside activates it (a toggle flips, a choice arrow steps, a list row is selected and activated — with
  `tap_selects_first` a first tap only selects); sliding out cancels; a slider follows x along its track; a list, grid
  or scroll box dragged more than `kUiDragThreshold` (8) pixels scrolls by whole rows from where the drag began and
  cancels the press. `hide_cursor_after_touch` hides the cursor until the next direction.

**Scene binding** (D30 unchanged): `bind(scene)` updates the UI right before the scene's `update()` (so it reads this
frame's results, and a pause scene pushed on top freezes it) and calls `reset_input()` after its `on_exit()` (focus,
memory, modals and values kept). Calling `update()` on a bound UI asserts.

## Results and callbacks

Each update fills a **results view** (`results()`, valid until the next update) in this order: `FocusChanged` (from in
`other`; list moves too, with `index`), `Activated` (with a list's `index`), `ValueChanged` (`value`), `Refused`,
`Blocked`, `Cancelled`, `Page` (`value` −1/+1, `index` the list's first visible item). `activated(key | widget)`,
`changed(key)`, `cancelled()` and `value(key | widget)` read it. Changes the game makes with setters (`set_value`,
`focus`) report nothing.

`on_activate(w, fn)` and `on_change(w, fn)` take a `SmallFunction` (16 bytes of captures: `this` and a pointer) called
during the update, after the results are sorted, in their order. A callback may change the UI — its changes are laid
out before `update()` returns, so the same frame's `draw()` shows them — but may not update or destroy the UI (asserts;
in Release a UI destroyed from its own callback is freed when the update ends).

`set_events(true)` also publishes `UiActivated`, `UiValueChanged`, `UiFocusChanged` and `UiCancelled` (≤ 16 bytes,
with the UI's serial) at the end of the update, delivered at the next frame's dispatch: for listeners that don't own
the UI (an audio scene playing clicks). Off by default; linked only when called.

## Text sources: the fonts and localization seam

Every text is a `UiText`: a **copy** (a string literal converts to one), a **borrowed** pointer the game keeps alive
(`UiText::borrow(buffer)`, then `mark_text_changed(w)` after editing it), or a **key** (`UiText::key(Text::Hello)` or
`UiText::name("menu.new_game")`, FNV-1a) turned into text by the UI's resolver (`set_resolver(fn)`: `const char*
(std::uint32_t key)`, the text kept alive by the game; nullptr draws nothing). Keys are resolved once and cached;
`text_changed()` resolves them all again and relayouts — the hook the localization spec uses when the language changes
(the example switches English/Português this way). All measuring, wrapping and drawing goes through one internal seam
in pixels, shared with `gfx::draw_text`'s wrapper ([text.md](text.md)). A style's `text.font` is the
[built-in font](text.md) (`UiFont::Builtin`) or one of the UI's 8 font slots (`UiFont::Slot0..Slot7`), filled with
`ui.set_font(slot, font)` ([fonts.md](fonts.md)); `nnh::loc::apply(ui, roles)` sets the resolver to `loc::resolve`, each
slot to the current language's role font and calls `text_changed()` ([localization.md](localization.md#api)).
Characters a font lacks draw as its replacement glyph or a hollow box.

Numbers (slider values, progress text, list counts) are formatted without `printf`; a `UiFormat` function
(`int (int value, char* out, int capacity)`) writes anything else ("75%", "Off").

## Lists and grids

A list has `rows` visible rows (`columns` per row for a grid) and an item **count** (0..65535) that `set_item_count`
changes at any time (the focus and scroll are clamped). It draws only visible rows, from a **row pool** sized at
creation: item *i* lives in pooled row *i mod (rows × columns)*, filled by the game's `fill(index, UiRow&)` only when it
comes into view or `mark_items_changed` marks it; the game keeps its own item data. A `UiRow` has a text (borrowed or
written into its 32-byte buffer), a number with a prefix (drawn right-aligned: `"x12"`), an icon region and `enabled`
(disabled rows are skipped, or refused when inert). The focused item stays visible: moving past the window scrolls by
whole rows with `margin` rows kept above and below; L/R page by the visible rows; `set_item`, `set_scroll` and
`list_info` (count, focused, first visible, visible rows) restore and inspect it. A 10 000-item list costs what a
20-item one does. Rows are whole: no row is drawn partly (gfx has no clip rectangle; a follow-up spec).

## Scroll boxes

A `scroll_box` stacks ordinary children vertically and shows the **window** of whole children that fits its height.
Moving the focus to a child outside it scrolls (with `margin` children kept visible); scrolling moves the children's
rectangles without a layout; children outside the window are neither drawn nor touchable; a touch drag scrolls by
children; `set_scroll(box, first)` sets the first one shown.

## Typewriter text

A label with `reveal_rate` ≥ 0 shows its characters a few per frame (`0.5` = one every two frames, kept exactly in a
16.16 accumulator). Wrapping is computed on the whole text, so words never jump lines. `set_reveal(w, n)`,
`set_reveal_rate`, `show_all(w)` (the confirm that skips a typewriter), `fully_shown`, `revealed` and `text_cells`
control and read it. Up to 16 labels reveal at once (the next one is shown whole).

## Styles, themes and 9-slice

A `UiStyle` (in `<nnh/detail/ui_style.hpp>`) has a `UiLook` per state — a background (`None`, `Solid` with an optional
1-pixel border, or `NineSlice`: a region of a texture slot cut by four insets, corners kept, edges and centre
stretched, drawn at whole pixels with no seams), a text colour, a tint — plus text settings (`font`, `scale` 1..8,
shadow), padding and kind parts (slider track/fill/knob, toggle images, choice arrows, list rows and scroll bar,
progress fill). States are drawn with this precedence: Disabled, Pressed (6 frames after a confirm, or while touched),
Focused, Checked, Normal. A **theme** (`UiTheme`) is one style per kind plus the cursor, held **by reference**
(`set_theme(theme)`; the game keeps it alive); `ui::default_theme()` is solid colours with no texture, its text
readable (contrast ≥ 4.5) on its faces. `add_style(style)` copies a custom style into the block (`UiStyleId`, then
`set_style(w, id)`); `set_style_value` changes it (a change of metrics relayouts, a colour doesn't). `set_texture(slot,
texture)` names the textures (slots 0..7) styles and images use.

All style members are integers, bools, enums, `Color` and `Region`: a game can keep its theme in a
[data file](data.md) by declaring the style structs in its schema source (the ctest `ui_style_structs_are_data_loadable`
compiles every one) and converting its own type — see the [guide](../guides/menus.md#a-theme-in-a-data-file).

## The focus cursor

`UiCursorStyle` (in the theme): an `image` placed outside the focused widget (or list item) at `anchor` + `(x, y)`
(the image's opposite point meets the widget's: `Left` puts a hand left of it) and/or an `outline`; it **glides** to a
new target over `move_frames` with `ease` (one `ease()` per frame, rounded once: the same pixels everywhere) and may
**pulse** (opacity down to `pulse_opacity`, x by `pulse_x`, over `pulse_frames`).

## Drawing and layers

`draw()` records each visible root (top, then bottom) on its layer (`set_layer`, default **14**, below spec 023's
transition overlay on 15), always `ignore_camera` (never moved or zoomed by `Camera2D`): parents before children in
tree order, render offsets and opacity accumulated down the tree in integers, an open modal after the rest of its
root, then the cursor. Every element is one public gfx call (`fill_rect`, `draw_rect`, `draw`, `draw_text`), so it
costs what the same call costs (glyphs count against `max_draws`) and the fake platform's items equal a hand-written
menu's. `set_root_visible(screen, false)` hides a root. Outside a frame gfx records nothing, but `stats().draws` and
`stats().draw_hash` (an FNV-1a of every call's arguments, used by the self-check) are still computed.

## Determinism

Layout, focus, navigation, repeats, values, scrolling, wrapping, reveal, the cursor and the recorded draws depend only
on the tree, the styles and the per-frame input, in integers (the only floats: the Circle Pad compare, one eased value
per frame while the cursor glides, the float positions handed to gfx). The example's 3600-frame self-check gives
`ui: PASS 30400DA6` on every host compiler and must on the console.

## Debugging and stats

`dump()` logs the tree (one `ui dump:` line per widget: kind, key, rectangle, state, value); `debug_draw(screen)`
outlines every visible widget on layer 15, the focus path in yellow. Both are empty in Release (no text kept).
`stats()`: widgets and capacity, text bytes, rows, functions, the last layout's visits, layouts run, the last draw's
draws and hash, dropped results, the block's bytes.

## Costs

Measured on the PC (pc-dev, WinLibs GCC) by the example's bench: an idle update of a 128-widget UI 0.002 ms, a focus
move 0.012 ms, a geometric move 0.006 ms, a full relayout of 64 keyed texts 0.048 ms, a 10 000-item list scrolling a
row a frame 0.047 ms (ratio to a 20-item list 0.99), the UI's cost per recorded draw 0.91× the same calls written by
hand, the options screen with 300 sprites at 60 Hz with no missed frame.

Measured on an Old 3DS (3ds-dev, the same bench) against the budgets the console run checks:

| Workload | Old 3DS | Budget |
|----------|---------|--------|
| idle update, 128 widgets | 0.025 ms (worst 0.104) | < 0.05 ms |
| focus move / geometric move | 0.051 / 0.099 ms | < 0.15 ms |
| full relayout, 64 keyed texts changed | 0.744 ms (worst 0.889) | < 0.93 ms |
| 10 000-item list scrolling a row a frame | 0.051 ms (ratio to 20 items 1.00) | < 0.2 ms, within 10 % |
| UI cost per recorded draw vs by hand | 7.52 vs 6.66 µs (1.13×) | ≤ 1.15× |
| options screen + 300 sprites, 600 frames | worst 18.0 ms, none missed | no missed frame |

A full relayout is for rare moments (a language switch, a new screen), not every frame; details in
[plan 028](../../specs/028-ui-widgets/plan.md). The idle update makes no division and touches no widget; text
dominates drawing on the console (one sprite per glyph).

## Misuse

A stale handle, a duplicate key, a child under a non-container, `min > max` or `step < 1`, a list without a fill, a 9th
modal, a non-finite offset, opacity or reveal rate, a texture slot outside 0..7, a layer outside 0..15, `update()` on a
bound UI or from the UI's own callback, destroying a root: an assert in Debug/Development; ignored (or clamped) in
Release, with one generic error line per UI. Never undefined behaviour (the random-operation test runs 100 000 of them
under ASan/UBSan on the Linux build VM).
