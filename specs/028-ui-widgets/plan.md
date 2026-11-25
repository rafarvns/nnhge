# Plan 028: UI widgets

Status: implemented on the PC 2026-11-25 (stages 0–8, 11; Linux T9.1 and console T10.2 pending); approved 2026-11-25 (the maintainer, the three questions at the end as recommended; the spec was approved 2026-11-25 with every open question as
recommended, and those recommendations are this plan's decisions)
Spec: [spec.md](spec.md) · Tasks: [tasks.md](tasks.md) · Decision D48 ([decisions.md](../../docs/architecture/decisions.md))
Uses spec 004/005 (public `gfx::draw`, `fill_rect`, `draw_rect`, `draw_text`, `measure_text`; the built-in font: a
monospace grid of 8×8 cells, lines 10 px apart, ASCII + Latin-1, the hollow box for the rest), spec 003 and 023 (the
public input queries, which already read idle under `input::set_blocked` and transitions), spec 001 and D30 (scenes,
scene attachments, the bus), spec 015 (`<nnh/ease.hpp>`, the `SmallFunction` callback pattern of tween sets), spec 020
(`nnh::Handle` and `next_generation` from `nnh/detail/handle.hpp`, `slot_bits.hpp`), spec 017 (only in the separately
linked adapter), spec 002 (asserts, logs, `Result`, tagged memory, profiler), spec 013 (scripts). Built in one worktree
(branch `spec-028`) from `main` at `d862b00`; no other spec is being built in parallel.

## Constitution Check (before design)

| Article | OK? | Note |
|---------|-----|------|
| I Same code everywhere | ✅ | Portable core only: no platform part, no `#ifdef`. Layout, focus, repeats, values, scrolling, wrapping and the cursor are integer arithmetic; the only floats are `<nnh/ease.hpp>`'s bit-identical curve (rounded once for the cursor), the Circle Pad threshold compare on the sample's values and the float → integer conversion of render offsets and opacity (rounded once, `-ffp-contract=off`). AC-2's golden runs on WinLibs GCC, GCC 13/14, clang 18, the 32-bit host build (`-m32 -msse2 -mfpmath=sse`, now installed on VM 120) and the console |
| II 3DS constraint | ⚠️→✅ | Pressure points: text is the expensive part (one sprite per glyph, ≈ 12 µs on an Old 3DS) and AC-6's targets predate any measurement. Answered: the UI draws through the same public calls a hand-written menu makes, so its own cost is the tree walk (≈ 1–4 % of the glyphs' cost, estimated below); a 32-byte hot record per widget keeps navigation, hit tests and the layout walks to one cache line per widget; lists virtualise to a fixed row pool, so a 10 000-item list costs what a 20-item one does; scrolling translates rectangles instead of relaying out. Memory: **88 B per widget on the 3DS** (96 on 64-bit PC; the spec's ≤ 96), 72 B per pooled row, the theme held by reference. One target is tight (the full relayout, 0.3–0.58 ms against 0.6), the others have room. The fallback for glyph cost (a UI glyph batch through D39's mechanism) is a contingency stage, run only if the console misses the frame target |
| III Build speed | ✅ | One public header `<nnh/ui.hpp>` plus one detail header for the style structs (`<nnh/detail/ui_style.hpp>`); includes `<cstdint>`, `<nnh/math.hpp>`, `<nnh/gfx.hpp>`, `<nnh/input.hpp>`, `<nnh/result.hpp>`, `<nnh/ease.hpp>`, `<nnh/detail/handle.hpp>`, `<nnh/detail/small_function.hpp>`; no standard containers, `<string>` or `<functional>`; no actions, tween, data, animation, entities or audio header. Measured today (WinLibs GCC, one-line TUs, 3 runs): `gfx.hpp + result.hpp` 7 844 preprocessed lines, 0.52–0.60 s `-fsyntax-only`; with `ease.hpp`, `handle.hpp`, `small_function.hpp` 7 993 lines, 0.51–0.52 s (noise). Budget for `<nnh/ui.hpp>`: within +0.05 s and +1 500 lines of `gfx.hpp + result.hpp` (T1.8) |
| IV Event-driven | ✅ | State is queried (focus, values, rectangles, the per-frame results view); changes reach the game through the results view and the callbacks it registered; bus events (`UiActivated`, `UiValueChanged`, `UiFocusChanged`, `UiCancelled`, ≤ 16 B) only when a UI opts in. The UI talks to gfx and input only through their public headers; nothing in gfx, input or scenes names it |
| V No hidden costs | ✅ | No exceptions (`Result` at creation), no RTTI, no `virtual` (widgets are records switched on a kind byte), no allocation after `create`, no `printf` in update or draw (integers formatted by hand). The game code called: row fills, formatters, the resolver, widget callbacks — documented per call |
| VI Spec + test first | ✅ | Every rule tested first on the fake platform: pure helpers (box arithmetic, anchors, wrapping, numbers, 9-slice geometry, repeat, the geometric rule) against hand-computed cases, the tree and update through the public API, drawing against the gfx items of the equivalent hand-written calls; AC-2's golden; a seeded random-operation test under ASan/UBSan; a planned hand-mutant pass |
| VII Small files | ✅ | One module `ui` of ≈ 34 sources, each one concept and ≤ 250 lines (aim 200); one public header (+ its detail style header, as `pool.hpp` has detail headers); the dialog helper, the actions adapter and the bus events are separate sources declared in the same header and linked only when called |
| VIII Documented API | ✅ | `docs/api/ui.md`, `docs/guides/menus.md`, links from `gfx.md`, `text.md`, `actions.md`, `tween.md`; `examples/ui`; D48 |
| IX Clean and legal | ✅ | Written from published designs (Godot's and Unity's docs, Clay's and microui's READMEs); no code copied; original art (`examples/ui/art/make_art.py`); credited in `docs/REFERENCES.md` |
| X Safe by default | ✅ | No new parser: text comes from the game, a theme file goes through the game's own spec 014 data type (the documented pattern). Misuse asserts in Debug/Development and is harmless in Release (FR-32), proven on pc-release and under ASan/UBSan |
| XI Opt-in, never in the way | ✅ | No core file outside `src/ui/` changes (no hook, no gfx/input/scene edit): `hello` must stay **byte-identical** (budget 0 B, T0.1/T8.3). The UI needs no actions, tweens, data, entities or audio; plain `gfx` menus stay supported |

## Design

### Overview

```
<nnh/ui.hpp>  (one public header; styles in <nnh/detail/ui_style.hpp>)
  Ui ── one tagged block the game owns (create(desc) → Result<Ui>; movable)
   ├─ header ≈ 0.5 KB: roots (≤ 2), focus, modal stack (8), input state (repeat, suppression, Circle Pad hysteresis),
   │                   cursor animation, texture slots (8), theme pointer, resolver, input source, D30 node, stats, draw hash
   ├─ hot nodes  [widgets] 32 B: links, generation, kind, flags, rect, min size, key, style, state, order
   ├─ cold data  [widgets] 56 B (3DS) / 64 B (PC): fixed size, placement, render offset/opacity, neighbours,
   │                   text source, kind data (slider, toggle, choice, list, label, container, image, progress)
   ├─ text bytes, extra text sources (choice options), custom styles, row pool, function slots, results
  update(): input → suppression → repeat → touch → directions → pages → confirm → cancel → results sorted
            → layout if dirty → callbacks → (opt-in) bus events → layout if dirty → cursor step
  draw():   per visible root, pre-order: background, content, children; modal subtrees and the cursor last;
            every element a public gfx call (fill_rect, draw_rect, draw, draw_text) on the root's layer, ignore_camera
  text seam (src/ui/ui_text_seam.*): measure / wrap / draw runs; one font today (spec 005's built-in grid)
  separately linked: ui_prompt.cpp (dialog helper), ui_actions.cpp (ActionSet adapter), ui_events.cpp (bus events)
```

### Decision D48: UI — a retained widget tree in one block, integer layout, focus rules, input through a UI input value, drawing through public gfx calls

**A tree the game owns.** A `Ui` is a handle to one tagged block (tag `ui` by default) holding a fixed number of widget
records, created by `Ui::create(desc)` (`Result`: `InvalidArgument`, `OutOfMemory`) and owned by the game (a scene
member, a game object; never a registry). Widgets are built in code by calls that take a parent and a plain options
value and return a `UiWidget` — spec 020's 32-bit `Handle` (slot 16 bits, generation 16 bits; a destroyed widget's
handle is stale at once and asserts when used). Records are split into a 32-byte **hot node** (parent, first child,
next and previous sibling as slot indexes, the first child's previous being the last child as in D37; generation; kind;
flags; the integer rectangle; the minimum size; key; style; state; a pre-order index) and a **cold record** (fixed size,
placement, render offset and opacity, four neighbours, the text source, and a 16-byte union of kind data). Slots are
taken lowest-first and reused at once (no quarantine: the UI never walks the tree while game code runs, see "update
order"). A UI has up to two **roots** (one per screen, created with the UI). Every widget kind is one record; there is
no `virtual` and no per-kind allocation: lists and grids own a slice of a **row pool** sized at creation, choice options
a slice of an extra text-source table.

**Layout in integers, only when dirty.** Layout runs per root in four linear walks over parent/child links (no
recursion, no stack): (1) minimum widths bottom-up (text measured through the seam; a wrapping label's minimum width is
its fixed width or its longest word), (2) widths top-down (boxes distribute extra space to expanding children by
integer stretch ratios: `share = extra × ratio / Σratio`, children whose minimum exceeds their share are fixed at their
minimum and the rest recomputed, the remainder pixels given one each to expanding children in child order), (3) wrapping
at the final widths and minimum heights bottom-up, (4) heights and positions top-down (cross-axis alignment start /
centre (`(free) / 2`, rounded down) / end / fill; anchors on roots and stacks). It runs when a change marked the root
dirty (text, visibility, size, item count, style metrics), at the end of `update()`, on a rectangle query, and — for
changes the game made after `update()` — at the start of `draw()` (a refinement, decision 6). Scrolling a scroll box
translates its children's rectangles instead of relaying out. Overflow is not shrunk: it logs one Debug/Development
warning per widget.

**Focus and navigation.** One focus per UI. A direction is resolved in a fixed order: (1) the focused widget's own use
(a slider or choice takes left/right, a list or grid moves its item), (2) an explicit neighbour, (3) walking up the
ancestors: the first box whose axis matches the direction moves to the next target among its children in child order
(grid boxes and grids by columns), wrapping at its end only on a **fresh press** unless the box allows wrap on repeat,
(4) the geometric rule within the root (or the open modal): candidates entirely beyond the focused rectangle's edge in
that direction; those overlapping it on the cross axis first; then the smallest gap on the main axis; then the
smallest distance between centres on the cross axis; then pre-order — integers only, (5) otherwise **blocked**.
Entering a container goes to its last focused child (if still a target), else its default, else its first target in
tree order. Disabled widgets are skipped unless the UI or container makes them inert (focusable, activation reports
**refused**). Focus never dangles: a hidden, skipped-disabled or destroyed focus moves to the next target in tree order
within its container, else the previous, else the root's default, else none. Navigation never crosses roots except
through an explicit neighbour. Modals (≤ 8 deep) restrict navigation, confirm/cancel and touch to their subtree, draw
after the rest of their root and give the focus back when closed.

**Input through a UI input value.** Each update reads one `UiInput` — held and pressed bits for eight controls (four
directions, confirm, cancel, page back, page forward), touch (held, started, ended, position) — from the default source
(the public input queries: D-Pad, the Circle Pad past a threshold with hysteresis, A/B (swappable), L/R, the bottom
screen), from a source function the game sets (the `ActionSet` adapter is one, linked only when called) or passed to
`update(input)`. The UI computes repeats itself (24-frame delay, 6-frame interval by default) from held bits for every
source. When the UI becomes active — its first update, after `reset_input()` or `AfterExit`, after a gap in
`frame_count()` (a scene above it was popped), when a modal opens — every control already held is ignored until
released.

**Results, callbacks, events.** Each update fills a results view (≤ `desc.results`, 32 by default) sorted, stably, into
the documented order (focus changed, activated, value changed, refused, blocked, cancelled, page); per-key queries read
it. Callbacks (`SmallFunction`, 16 bytes of captures, in a function-slot table) run after the sort; their changes are
laid out before `update()` returns. Bus events only when opted in, published at the end of the update (delivered at the
next frame's dispatch). Scene binding is D30 unchanged: `BeforeUpdate` runs `update()`, `AfterExit` resets transient
input state and keeps focus memory.

**Text through one seam.** A widget's text is a **text source**: copied into the UI's text bytes, borrowed (the game
keeps it alive and calls `mark_text_changed`), or a key (an integer or an FNV-1a-hashed name) resolved by the UI's
resolver, cached until `text_changed()` re-resolves every key and relayouts — the localization spec's hook. All
measuring, wrapping and drawing goes through `src/ui/ui_text_seam.*`, whose only font today is spec 005's built-in
grid; a style's text part has a `font` field (`UiFont::Builtin`), the game-fonts spec's hook. Wrapping is computed on the
whole text (words never jump while revealing).

**Drawing through public gfx calls.** `draw()` walks each visible root in pre-order and records every element with
`gfx::fill_rect`, `draw_rect`, `draw` and `draw_text`, always `ignore_camera`, on the root's layer (default 14, below
spec 023's overlay on 15); opacity is an integer product folded into colours' alpha; modal subtrees and the focus cursor
come last. The fake platform's built items therefore equal a hand-written menu's (tested), and AC-6's ratio compares
like with like. The UI folds the arguments of every gfx call it makes into a 32-bit **draw hash** (stats), so the
console's self-check can compare "the recorded draws" without reading gfx internals. 9-slices are nine `gfx::draw`s of
sub-regions with `scale = dst / src`: every edge lands within 2⁻¹⁰ of an integer, and coverage only changes at pixel
centres, so there are no seams.

**Rejected:** immediate mode (no focus memory or layout cache; text re-measured each frame), actions as the required
input (XI), bus events by default (a frame late, IV), a UI batch in the draw list from the start (a gfx change for an
unmeasured problem; kept as the contingency), a clip rectangle (a contract change on every platform: a follow-up spec),
`virtual` widgets, widgets as entities or nodes, a per-widget heap, a hidden registry of UIs, copying the theme into
every UI (≈ 6.7 KB each), a key hash table (linear scans of 32-byte nodes are fast enough at ≤ 4096 widgets and need no
memory), quarantined slots (no iteration can see a reused slot), libm or wall time.

### Public API (sketch)

```cpp
// <nnh/ui.hpp>: <cstdint>, <nnh/math.hpp>, <nnh/gfx.hpp>, <nnh/input.hpp>, <nnh/result.hpp>, <nnh/ease.hpp>,
// <nnh/detail/handle.hpp>, <nnh/detail/small_function.hpp>, <nnh/detail/ui_style.hpp>. Optional (XI).
namespace nnh {
class Scene; class Texture; class ActionSet; struct MemTag;
namespace detail { struct UiBlock; struct UiWidgetTag; }

inline constexpr int kMaxUiWidgets = 4096;
inline constexpr int kMaxUiModalDepth = 8;
inline constexpr int kMaxUiTextures = 8;     ///< texture slots a UI's styles and images refer to
inline constexpr int kMaxUiItems = 65535;    ///< items of a list or grid
inline constexpr int kUiDefaultLayer = 14;   ///< below spec 023's transition overlay (15)
inline constexpr int kUiPressedFrames = 6;   ///< a confirmed widget shows its pressed look at least this long
inline constexpr int kUiDragThreshold = 8;   ///< pixels before a touch on a list, grid or scroll box scrolls it

using UiWidget = Handle<detail::UiWidgetTag>;   ///< spec 020's handle: slot + generation, 0 = none

inline constexpr int kUiNoNumber = INT32_MIN;  ///< UiRow::number: no number column

/// The game's own enum value (0..65534) naming a widget, or none.
struct UiKey {
    constexpr UiKey() = default;
    template <typename E> requires std::is_enum_v<E> constexpr UiKey(E e);
    constexpr explicit UiKey(int v);
    std::uint16_t value = 0xFFFF;               ///< 0xFFFF: no key
};
struct UiRect { int x = 0, y = 0, w = 0, h = 0; bool contains(int px, int py) const; Rect rect() const; };

enum class UiKind : std::uint8_t { Root, Label, Image, Progress, Panel, Spacer, Button, Toggle, Slider, Choice,
                                    List, Grid, VBox, HBox, GridBox, Stack, ScrollBox };
enum class UiState : std::uint8_t { Normal, Focused, Pressed, Disabled, Checked };   ///< UiStyle::states index
enum class UiDir : std::uint8_t { Up, Down, Left, Right };
enum class UiAlign : std::uint8_t { Start, Center, End, Fill };
enum class UiAnchor : std::uint8_t { TopLeft, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight };

/// Where a root's or stack's child goes (FR-10): its `anchor` point at the parent's same point + (x, y); fill_x/fill_y
/// stretch it between the parent's edges with margins (x, right) / (y, bottom).
struct UiPlace { UiAnchor anchor = UiAnchor::TopLeft; int x = 0, y = 0; bool fill_x = false, fill_y = false;
                 int right = 0, bottom = 0; };
/// Size and size flags (FR-7, FR-9). width/height 0: from content.
struct UiLayout { int width = 0, height = 0; bool expand = false; int stretch = 1; UiAlign align = UiAlign::Fill;
                  UiPlace place{}; };

/// A text source (FR-24). A string literal converts to a copy; borrowed text must outlive the widget.
class UiText {
public:
    UiText() = default;
    UiText(const char* utf8);                       ///< copy (NOLINT: implicit on purpose)
    static UiText copy(const char* utf8);
    static UiText borrow(const char* utf8);         ///< the game's buffer; call mark_text_changed(w) after editing it
    static constexpr UiText key(std::uint32_t key);
    template <typename E> requires std::is_enum_v<E> static constexpr UiText key(E e);
    static constexpr UiText name(const char* name); ///< key = FNV-1a 32 of the name ("menu.new_game")
    ...
};
/// Text for a key: UTF-8 the game keeps alive until the next text_changed(). nullptr draws nothing.
using UiResolver = detail::SmallFunction<const char*(std::uint32_t key)>;

/// One row of a list or grid, filled by the game (FR-11). Text is borrowed or written into `buffer`.
struct UiRow {
    const char* text = nullptr;
    int number = kUiNoNumber;        ///< drawn right-aligned by the style (FR-25), e.g. a count
    const char* number_prefix = "";  ///< "x" → "x12"
    int icon_texture = -1; Region icon{};
    bool enabled = true;
    char buffer[32] = {};
};
using UiRowFill = detail::SmallFunction<void(int index, UiRow& row)>;
/// Text for a value (FR-25): write up to `capacity` bytes (UTF-8, no NUL needed), return the length.
using UiFormat = detail::SmallFunction<int(int value, char* out, int capacity)>;

// Options of each kind; every one has key, style and layout (designated initializers).
struct UiLabelDesc { UiText text; UiKey key{}; UiStyleId style{}; UiLayout layout{};
                     bool wrap = false; int max_lines = 0; TextAlign align = TextAlign::Left;
                     UiAlign valign = UiAlign::Start; float reveal_rate = -1; /* < 0: all shown; chars per frame */ };
struct UiImageDesc { int texture = -1; Region region{}; int scale = 1; Color tint = colors::white; UiKey key{};
                     UiStyleId style{}; UiLayout layout{}; };
struct UiProgressDesc { int value = 0, max = 100; bool vertical = false; bool show_text = false; UiFormat format{};
                        UiKey key{}; UiStyleId style{}; UiLayout layout{}; };
struct UiBoxDesc { int spacing = 4; int padding = 0; int columns = 2; /* GridBox */ bool wrap = true;
                   bool wrap_on_repeat = false; bool disabled_focusable = false; UiKey key{}; UiStyleId style{};
                   UiLayout layout{}; };
struct UiPanelDesc { bool as_box = false; UiKind box = UiKind::VBox; int spacing = 4; UiKey key{}; UiStyleId style{};
                     UiLayout layout{}; };       ///< a background + style padding around one child, or a box
struct UiButtonDesc { UiText text; int icon_texture = -1; Region icon{}; UiKey key{}; UiStyleId style{}; UiLayout layout{}; };
struct UiToggleDesc { UiText text; bool checked = false; bool left_right_set = true; UiKey key{}; UiStyleId style{};
                      UiLayout layout{}; };
struct UiSliderDesc { UiText text; int min = 0, max = 10, step = 1, value = 0; bool show_value = true;
                      UiFormat format{}; UiKey key{}; UiStyleId style{}; UiLayout layout{}; };
struct UiChoiceDesc { UiText text; const UiText* options = nullptr; int count = 0; int index = 0; bool wrap = true;
                      UiKey key{}; UiStyleId style{}; UiLayout layout{}; };
struct UiListDesc { int rows = 8; int columns = 1; /* > 1: a grid */ int count = 0; UiRowFill fill{};
                    int margin = 1; bool wrap = true; bool tap_selects_first = false; int row_height = 0; /* 0: style */
                    int cell_width = 0; UiKey key{}; UiStyleId style{}; UiLayout layout{}; };
struct UiScrollDesc { int spacing = 4; int padding = 0; int margin = 1; UiKey key{}; UiStyleId style{};
                      UiLayout layout{}; };
struct UiModalOptions { bool cancel_closes = true; bool outside_touch_cancels = false; };
struct UiPromptDesc { UiText message; UiText buttons[3]; UiKey keys[3]; int count = 2; int default_button = 0;
                      int cancel_button = 1; bool close_on_answer = true; int width = 0; };

/// One frame of UI input (FR-22).
enum class UiControl : std::uint8_t { Up, Down, Left, Right, Confirm, Cancel, PageBack, PageForward };
struct UiInput {
    std::uint8_t held = 0, pressed = 0;   ///< 1 << UiControl
    std::uint8_t repeated = 0;            ///< used only when UiInputConfig::repeat_delay == 0 (the game's own repeat)
    bool touching = false, touch_started = false, touch_ended = false;
    std::int16_t touch_x = 0, touch_y = 0;
};
using UiInputSource = detail::SmallFunction<void(UiInput& out)>;
struct UiInputConfig {
    int repeat_delay = 24, repeat_interval = 6;   ///< frames; delay 0: take UiInput::repeated as given
    bool circle_pad = true; float circle_press = 0.5f, circle_release = 0.4f;
    Button confirm = Button::A, cancel = Button::B, page_back = Button::L, page_forward = Button::R;
    bool hide_cursor_after_touch = false;
};
/// The game's spec 017 actions for each control (an enum value of its ActionSet, or none).
struct UiActionRef { constexpr UiActionRef() = default; template <typename E> requires std::is_enum_v<E>
                     constexpr UiActionRef(E e); std::uint16_t index = 0xFFFF; };
struct UiActionMap { UiActionRef up, down, left, right, move /* a vector action, optional */, confirm, cancel,
                     page_back, page_forward; bool touch = true; /* the bottom screen, raw */ };

/// What happened this frame (FR-20).
enum class UiResultKind : std::uint8_t { FocusChanged, Activated, ValueChanged, Refused, Blocked, Cancelled, Page };
struct UiResult { UiResultKind kind; UiKey key; UiWidget widget; UiWidget other; /* FocusChanged: from */
                  std::int32_t value = 0; /* new value; Page: −1/+1 */ std::int32_t index = -1; /* list item */ };
struct UiResults { const UiResult* data = nullptr; int size = 0; const UiResult* begin() const; const UiResult* end() const; };
using UiCallback = detail::SmallFunction<void(const UiResult& result)>;

// Opt-in bus events (≤ 16 B; `ui` is the UI's serial).
struct UiActivated { std::uint16_t ui; UiKey key; std::uint32_t widget; std::int32_t index; };
struct UiValueChanged { std::uint16_t ui; UiKey key; std::uint32_t widget; std::int32_t value; };
struct UiFocusChanged { std::uint16_t ui; UiKey key; std::uint32_t widget; std::uint32_t from; };
struct UiCancelled { std::uint16_t ui; UiKey key; /* of the focused widget */ };

struct UiDesc {
    int widgets = 64;          ///< 1..4096 (roots included)
    int text_bytes = 1024;     ///< 0..65535: copied text
    int texts = 0;             ///< 0..4096: extra text sources (choice options)
    int styles = 0;            ///< 0..240: custom styles (add_style)
    int rows = 0;              ///< 0..1024: pooled list and grid rows
    int functions = 8;         ///< 0..255: callbacks, row fills, formatters
    int results = 32;          ///< 8..256 per frame
    bool top = true, bottom = true;            ///< roots
    Screen first_focus = Screen::Bottom;       ///< the root the first focus enters
    UiInputConfig input{};
    MemTag* tag = nullptr;     ///< default: the engine tag "ui"
};
struct UiListInfo { int count = 0, focused = -1, first_visible = 0, visible_rows = 0; };
struct UiStats { int widgets = 0, capacity = 0, text_bytes = 0, text_capacity = 0, rows = 0, functions = 0;
                 int layout_visits = 0; /* last layout: nodes visited */ int draws = 0; /* last draw(), glyphs counted */
                 std::uint32_t draw_hash = 0; int dropped_results = 0; };

class Ui {
public:
    /// One block: ≈ 0.5 KB + 88 B × widgets (3DS) + text_bytes + 72 B × rows + 24 B × functions + 20 B × results + ...
    static Result<Ui> create(const UiDesc& desc);
    Ui() = default; ~Ui();  Ui(Ui&&) noexcept;  Ui& operator=(Ui&&) noexcept;  // not copyable
    bool valid() const;
    UiWidget root(Screen screen) const;     ///< null if the UI has no root there

    // Building (FR-2, FR-5, FR-6, FR-9, FR-12). Full: a null handle, one Debug/Development assert (decision 2).
    UiWidget label(UiWidget parent, const UiLabelDesc& d);
    UiWidget image(UiWidget parent, const UiImageDesc& d);
    UiWidget progress(UiWidget parent, const UiProgressDesc& d);
    UiWidget panel(UiWidget parent, const UiPanelDesc& d = {});
    UiWidget spacer(UiWidget parent, const UiLayout& layout = {.expand = true});
    UiWidget button(UiWidget parent, const UiButtonDesc& d);
    UiWidget toggle(UiWidget parent, const UiToggleDesc& d);
    UiWidget slider(UiWidget parent, const UiSliderDesc& d);
    UiWidget choice(UiWidget parent, const UiChoiceDesc& d);
    UiWidget list(UiWidget parent, const UiListDesc& d);        ///< columns > 1 makes a grid
    UiWidget vbox(UiWidget parent, const UiBoxDesc& d = {});
    UiWidget hbox(UiWidget parent, const UiBoxDesc& d = {});
    UiWidget grid_box(UiWidget parent, const UiBoxDesc& d);
    UiWidget stack(UiWidget parent, const UiBoxDesc& d = {});
    UiWidget scroll_box(UiWidget parent, const UiScrollDesc& d = {});
    void destroy(UiWidget w);               ///< the whole subtree; focus repaired; a root can't be destroyed
    UiWidget prompt(Screen screen, const UiPromptDesc& d);      ///< FR-8 (ui_prompt.cpp); opened modally
    void close_prompt(UiWidget prompt);

    // Changing (FR-7, FR-11, FR-23, FR-24). Each marks what it affects.
    void set_text(UiWidget w, const UiText& t);  void mark_text_changed(UiWidget w);
    void set_resolver(UiResolver r);  void text_changed();      ///< every key re-resolved, relayout
    void set_visible(UiWidget w, bool on);  void set_enabled(UiWidget w, bool on);
    void set_value(UiWidget w, int v);       ///< slider, progress, toggle (0/1), choice (index): no result reported
    void set_progress_max(UiWidget w, int max);
    void set_style(UiWidget w, UiStyleId s);  void set_layout(UiWidget w, const UiLayout& l);
    void set_render_offset(UiWidget w, Vec2 offset);   ///< rounded to whole pixels; a tween setter's target
    void set_opacity(UiWidget w, float opacity);       ///< 0..1, rounded to 1/255
    void set_key(UiWidget w, UiKey k);  void set_neighbour(UiWidget w, UiDir d, UiWidget target);
    void set_default(UiWidget container, UiWidget child);  void set_back(UiWidget container, UiWidget back);
    void set_item_count(UiWidget list, int count);  void mark_items_changed(UiWidget list, int first = 0, int count = -1);
    void set_scroll(UiWidget list_or_scroll_box, int first);  void set_item(UiWidget list, int index); ///< focused item
    void set_reveal(UiWidget label, int chars);  void set_reveal_rate(UiWidget label, float chars_per_frame);
    void show_all(UiWidget label);
    void on_activate(UiWidget w, UiCallback cb);  void on_change(UiWidget w, UiCallback cb);

    // Focus (FR-14..FR-18)
    void focus(UiWidget w);  void focus(UiKey k);  UiWidget focused() const;  UiKey focused_key() const;
    void open_modal(UiWidget container, const UiModalOptions& o = {});  void close_modal();  int modal_depth() const;

    // Input and update (FR-21, FR-22)
    void update();                           ///< from the input source (default: raw buttons and touch)
    void update(const UiInput& input);
    void set_input_source(UiInputSource source);   ///< null: back to raw input
    void use_actions(const ActionSet& set, const UiActionMap& map);   ///< ui_actions.cpp: linked only if called
    void reset_input();                      ///< transient state cleared; held controls ignored until released
    void bind(Scene& scene);  void unbind();  ///< D30: BeforeUpdate → update(), AfterExit → reset_input()
    void set_events(bool publish);           ///< ui_events.cpp

    // Results (FR-20): valid until the next update.
    UiResults results() const;
    bool activated(UiKey k) const;  bool activated(UiWidget w) const;  bool changed(UiKey k) const;
    bool cancelled() const;  int value(UiKey k) const;  int value(UiWidget w) const;

    // Styles (FR-26, FR-27) and drawing (FR-28)
    void set_theme(const UiTheme& theme);    ///< the game keeps it alive; default ui::default_theme()
    UiStyleId add_style(const UiStyle& style);  void set_style_value(UiStyleId id, const UiStyle& style);
    void set_texture(int slot, const Texture& texture);   ///< 0..7; the game keeps it alive
    void set_layer(Screen screen, int layer);  void set_root_visible(Screen screen, bool on);
    void draw() const;   ///< completes a pending layout first (decision 6), then records; changes nothing else

    // Queries and debugging (FR-13, FR-30)
    bool alive(UiWidget w) const;  UiWidget find(UiKey k) const;  UiKind kind(UiWidget w) const;
    UiRect rect(UiWidget w) const;  Screen screen(UiWidget w) const;  bool visible(UiWidget w) const;
    bool enabled(UiWidget w) const;  UiState state(UiWidget w) const;  const char* text(UiWidget w) const;
    UiListInfo list_info(UiWidget list) const;  bool fully_shown(UiWidget label) const;  UiStats stats() const;
    void debug_draw(Screen screen) const;  void dump() const;   ///< Debug/Development; nothing in Release
private:
    detail::UiBlock* block_ = nullptr;
};

namespace ui { const UiTheme& default_theme(); }   ///< solid colours, no texture, readable on both screens
} // namespace nnh

// <nnh/detail/ui_style.hpp> — aggregates of ints, bools, enums, Color and Region only (spec 014 can load each member).
enum class UiFont : std::uint8_t { Builtin };                 // the game-fonts spec adds values
enum class UiBackgroundKind : std::uint8_t { None, Solid, NineSlice };
struct UiBackground { UiBackgroundKind kind = UiBackgroundKind::None; std::uint8_t texture = 0; bool border = false;
                      Color color{}; Color border_color{}; Region region{};
                      std::uint8_t left = 0, top = 0, right = 0, bottom = 0; };          // 32 B
struct UiLook { UiBackground background{}; Color text = colors::white; Color tint = colors::white; };   // 40 B
struct UiTextLook { UiFont font = UiFont::Builtin; std::uint8_t scale = 1; bool shadow = false;
                    Color shadow_color = rgba(0, 0, 0, 160); };
struct UiImagePart { std::int8_t texture = -1; Region region{}; };
struct UiStyle {
    UiLook states[5]{};                       // Normal, Focused, Pressed, Disabled, Checked
    UiTextLook text{};
    std::uint8_t padding[4] = {4, 2, 4, 2};   // left, top, right, bottom
    struct { UiBackground track{}, fill{}, knob{}; std::uint8_t knob_width = 8; } slider;
    struct { UiImagePart off{}, on{}; } toggle;
    struct { UiImagePart left{}, right{}; bool text_arrows = true; } choice;   // "<" ">" when no images
    struct { UiBackground row[3]{}; /* normal, focused, disabled */ UiBackground track{}, thumb{};
             std::uint8_t scroll_width = 4, row_height = 12; } list;
    struct { UiBackground fill{}; } progress;
};                                            // ≈ 600 B (static_assert ≤ 640)
struct UiCursorStyle { UiImagePart image{}; UiAnchor anchor = UiAnchor::Left; std::int8_t x = -2, y = 0;
                       bool outline = false; Color outline_color = colors::white; std::uint8_t move_frames = 6;
                       Ease ease = Ease::CubicOut; std::uint8_t pulse_frames = 0; std::uint8_t pulse_opacity = 160;
                       std::int8_t pulse_x = 0; };
struct UiTheme { UiStyle label, image, progress, panel, button, toggle, slider, choice, list, grid, scroll_box;
                 UiCursorStyle cursor; };     // ≈ 6.7 KB, owned by the game (or the engine's default)
struct UiStyleId { std::uint8_t value = 0; };  // 0: the kind's theme style
```

Exact names may still change while building; stage notes record any change. Nested anonymous struct members of
`UiStyle` become named structs (`UiSliderParts`, …) if spec 014's macros need names (T6.1 checks).

### Events introduced

Opt-in only (`set_events(true)`, FR-20); none by default.

| Event | Published by | Payload | Typical listeners |
|-------|--------------|---------|-------------------|
| `UiActivated` | a UI with events on, at the end of its update | UI serial, key, widget bits, list index (12 B) | an audio scene playing a click |
| `UiValueChanged` | the same | UI serial, key, widget bits, new value (12 B) | settings applied elsewhere, sound ticks |
| `UiFocusChanged` | the same | UI serial, key, widget bits, previous widget bits (12 B) | a "tick" sound, a description panel in another scene |
| `UiCancelled` | the same | UI serial, key of the focused widget (4 B) | a "back" sound |

### Semantics fixed by the plan

**Creation checks** (`ui_desc_check.cpp`): widgets 1..4096 (2 roots are counted), text_bytes 0..65535, texts 0..4096,
styles 0..240, rows 0..1024, functions 0..255, results 8..256, at least one root, `first_focus` a created root,
repeat_delay 0..255 (0: the input's own repeats), repeat_interval 1..255, circle thresholds finite with 0 < release ≤
press ≤ 1, confirm/cancel/page single distinct buttons. `InvalidArgument` names the field.

**Widget creation** (`ui_tree.cpp`, `ui_widgets_*.cpp`): a stale or null parent, a parent that can't hold children
(label, image, progress, spacer, button, toggle, slider, choice, list), a second child of a non-box panel, slider
`min > max` or `step < 1`, a choice with 0 options, a list without a fill function or with rows × columns over the
free row pool, a duplicate key: assert (Debug/Development) and return a null handle (Release: nothing created, one
error log per UI). A full UI (widgets, text bytes, texts, rows, functions) is decision 2's case. New widgets are
appended as their parent's last child (pre-order indexes refreshed at the next layout). `destroy` takes the subtree
(post-order through links), frees text bytes by compaction of the copied-text region (offsets rewritten; ≤ text_bytes
bytes moved, documented as a load-time cost) and frees row slices and function slots.

**Hot node (32 B, every system)**: `parent, first_child, next, prev` (u16 slots; 0xFFFF none), `generation` (u16),
`kind` (u8), `flags` (u8: visible, enabled, inert-focusable, overflow warned, has key, dirty text, bottom root, wrap),
`x, y, w, h` (i16), `min_w, min_h` (i16), `key` (u16), `style` (u8), `state` (u8), `align` (u8: align + expand),
`stretch` (u8), `order` (u16, pre-order index of its root, from the last layout). **Cold record (56 B 3DS / 64 B PC)**:
`fixed_w, fixed_h` (i16), placement (anchor u8, fill bits, four i16), `offset_x, offset_y` (i16), `opacity` (u8),
neighbours (4 × u16), text source (kind u8 + `{u16 offset, u16 length}` | borrowed pointer | `{u32 key, cached
pointer}`), callback slot (u8), kind union (16 B): label `{visible chars u16, reveal accumulator u32 16.16, rate u32,
lines u8, max lines u8, wrap/align bits}`, slider `{value, min, max, step i16, format slot u8}`, toggle `{checked}`,
choice `{index, count, first text u16, wrap}`, list/grid `{count, focused, first visible u16, rows, columns u8, first
pool row u16, ring head u8, fill slot u8, margin u8, row height u8, cell width i16}`, container `{padding 4 × u8,
spacing u8, columns u8, default child u16, last focused u16, back u16, options bits}`, image `{texture u8, region 4 ×
i16, scale u8, tint}`, progress `{value i32, max i32, vertical and text bits, format slot u8}`. `static_assert`s: hot 32, cold 56 on 32-bit
targets and ≤ 64 on 64-bit ones.

**Layout** (`ui_layout_measure.cpp`, `ui_layout_arrange.cpp`, pure `ui_box_math.{hpp,cpp}`, `ui_anchor.{hpp,cpp}`):
- *Minimum sizes*: label = text cells × 8 × scale + padding (wrapping: fixed width, else its longest word); height =
  lines × 10 × scale (+ padding); image = region × scale; button/toggle/slider/choice = text + their parts (toggle box
  10 px + 4 gap; slider label + track minimum 48 + value text width of `max`'s digits; choice arrows 8 px each + 4 gap +
  the widest option); list = cell width (`cell_width`, else 64: rows are filled lazily, so they can't size the list) × columns +
  scroll width, rows × row height; boxes = Σ children + spacing + padding on the axis, max on the cross; grid box =
  Σ column widths (max per column), Σ row heights; stack = max of children's minimum + |offsets|; panel = child +
  style padding. A fixed size replaces the minimum (fixed smaller than content = overflow, warned once).
- *Distribution* (main axis): `extra = available − Σ sizes`; expanding children share it by `stretch` (1..255) as
  above; if `extra < 0`, children keep their minimums and the box overflows (warned once). Remainder pixels: one each
  to expanding children, first child first. Cross axis: `Fill` = available; `Center` = `(available − size) >> 1`
  (rounded down); `End` = available − size.
- *Anchors*: for a non-filling axis, `pos = parent_point − child_point + offset` where a point is start / centre
  (`(size) >> 1`) / end; filling: `pos = parent_start + x`, `size = parent_size − x − right`.
- *Order*: positions in screen coordinates; `order` = pre-order index.
- *Cost*: `layout_visits` counts nodes visited (stats; tests pin it at 4 × the root's nodes).

**Text** (`ui_text_seam.{hpp,cpp}`, `ui_wrap.{hpp,cpp}`): the seam's interface is `measure(font, scale, utf8,
length) → {cells, px}`, `cell_width(font, scale)`, `line_height(font, scale)`, `draw_run(screen, x, y, utf8, length,
look)`. Today `draw_run` copies the run into a 256-byte stack buffer and calls `gfx::draw_text` (one call per line). The
wrapper walks code points (`detail::next_codepoint`), breaks at spaces (dropped at a break) and after `-`, honours `\n`,
breaks a word longer than the line by characters; `max_lines` cuts the last line to `cells − 3` and appends `...`
(three cells; decision 3); reveal draws the first `visible` code points of the wrapped lines (spaces count, `\n`
doesn't). The reveal accumulator is 16.16 per frame (`rate × 65536` rounded at `set_reveal_rate`, exact for dyadic
rates such as 0.5 or 0.25).

**Navigation** (`ui_navigate.cpp`, pure `ui_geometric.{hpp,cpp}`): the order of D48. A *target* is visible (with its
ancestors), in the active modal's subtree (or no modal), in the focused root (unless reached by a neighbour), enabled
or inert-focusable, focusable (FR-6 kinds, lists and grids with count > 0), and not hidden by a scroll box's window
unless the move is that scroll box's own step (it scrolls to reveal). Lists/grids: up/down move by 1 (grid: by
columns), left/right by 1 in a grid; a move past an end wraps only if the list wraps and the press is fresh; otherwise it
falls through to step 2 from the list widget itself. The geometric rule's candidate set is the root's (or modal's)
targets; an empty set → blocked. Sliders/choices consume left/right (at their limit a non-wrapping one reports blocked);
toggles with `left_right_set` consume left (off) and right (on) only when that changes the value, else pass.

**Values** (`ui_values.cpp`): slider `value ± step` clamped (a fresh press or a repeat), touch `min + round((x − track.x)
× (max − min) / (track.w − 1) / step) × step` (integers, halves up); toggle flips on confirm or tap; choice ±1 (wrap
per widget) on left/right, +1 on confirm, by its arrows on touch. Each change: `ValueChanged` with the new value; a set
from the game (`set_value`) reports nothing.

**Touch** (`ui_touch.cpp`): only the bottom root, the active modal's subtree first. Hit test: pre-order over visible
nodes, the last containing node wins (later siblings and modals draw on top); the interactive ancestor of the hit is the
touched widget. Started on an enabled (or inert) focusable widget: focus it (`FocusChanged`), pressed state; held: a
slider follows x continuously; a list, grid or scroll box past 8 px of vertical travel (`kUiDragThreshold`) scrolls by
whole rows/children from the scroll at the press (`(dy) / row height`, toward zero) and cancels the press; released
inside → activate (button `Activated`, toggle flip, choice arrow, list row: select (+ activate unless
`tap_selects_first` and the row wasn't already focused)); released outside → nothing. Inert widgets report `Refused` on
release. Touches outside an open modal: ignored, or `Cancelled` (+ close) with `outside_touch_cancels`. After a touch
the cursor may hide until the next direction (`hide_cursor_after_touch`).

**Update order** (`ui_update.cpp`): (1) activation check (first update, after `reset_input`, a `frame_count()` gap > 1,
a modal opened since): every held control and a held touch suppressed until released; (2) the input (source, adapter or
argument); (3) suppression mask; (4) repeat: a direction or page fires on its press and then at `delay`, `delay +
interval`, … frames held (`repeat` flag; at most one direction per frame: the newest pressed, ties Up, Down, Left,
Right); (5) results cleared, pressed countdowns, reveal advance (only labels in the revealing list: ≤ 16 at once,
documented), (6) touch, (7) direction, (8) page, (9) confirm (activate / toggle / choice next / refused), (10) cancel
(the innermost container's `back` widget is activated, else a modal with `cancel_closes` closes, and `Cancelled` is
reported in every case), (11) focus memory and the cursor's target, (12) stable sort of the results, layout if dirty,
callbacks in results order (a callback's own changes are applied to the tree at once; destroying the UI from one
asserts), opt-in events, layout if dirty, (13) cursor step. Steps 6–10 see the tree as it was at the start of the step.

**Drawing** (`ui_draw.cpp`, `ui_draw_widgets.cpp`, `ui_draw_parts.cpp`, `ui_draw_out.{hpp,cpp}`): per root (top, then
bottom), pre-order over visible nodes with offsets and opacity accumulated down the tree (`alpha = (a × o + 127) /
255`, integers); a scroll box draws only its window's children; a modal subtree is skipped in place and drawn after
the root; then the cursor (outline and/or image at its eased position) on the focused root. Per widget: the state's
background (none; `fill_rect` + optional `draw_rect`; nine `gfx::draw`s), the content (text runs, icons, parts), the
children. State precedence: Disabled, Pressed (6 frames after a confirm or while touched), Focused, Checked, Normal.
`ui_draw_out` is the only file calling gfx: each call also folds its arguments (and a run's bytes) into the draw hash
and counts draws (glyphs counted by non-space cells, as gfx counts them). Drawing outside a frame records nothing (gfx
ignores it) but still hashes (the self-check runs several UI frames per display frame).

**Scene binding** (`ui_scene.cpp`): the block holds a `SceneAttachment`; `BeforeUpdate` → `update()` (the input
source), `AfterExit` → `reset_input()` (presses, repeats, touch, pressed looks, the activation flag set; focus, focus
memory, modals and values kept). A bound UI's `update()` called by hand asserts (as tween sets).

**Misuse (FR-32)**, assert in Debug/Development and harmless in Release: a stale handle anywhere (ignored, queries
answer zero/none), a duplicate key (ignored), a child under a non-container, an attach that would make a cycle (only
`set_neighbour` can name another root's widget; no reparenting API exists), slider min > max or step 0 (clamped),
a list without a fill (count kept 0), modal depth 9 (ignored), a non-finite render offset or opacity (ignored), a
texture slot outside 0..7 (ignored), destroying or moving the UI from its own callback, `update()` on a bound UI,
`destroy(root)`. Never undefined behaviour (ASan/UBSan, the random-operation test).

### Files

| File | New/changed | Responsibility |
|------|-------------|----------------|
| `engine/core/include/nnh/ui.hpp` | new | the public API above (≈ 380 lines with comments) |
| `engine/core/include/nnh/detail/ui_style.hpp` | new | `UiFont`, `UiBackground`, `UiLook`, `UiTextLook`, `UiImagePart`, `UiStyle`, `UiCursorStyle`, `UiTheme`, `UiStyleId` (≈ 120 lines) |
| `engine/core/src/ui/ui_block.hpp` | new | block layout (header, hot nodes, cold records, tables), access helpers, the `ui` tag and log channel |
| `engine/core/src/ui/ui_desc_check.{hpp,cpp}` | new | creation checks |
| `engine/core/src/ui/ui_create.cpp` | new | create, move, destroy, roots, `valid`, `stats` |
| `engine/core/src/ui/ui_tree.{hpp,cpp}` | new | slots, handles, link/unlink, subtree destroy, keys (`find`, duplicates), parent rules |
| `engine/core/src/ui/ui_widgets_display.cpp` | new | label, image, progress, panel, spacer |
| `engine/core/src/ui/ui_widgets_input.cpp` | new | button, toggle, slider, choice |
| `engine/core/src/ui/ui_widgets_boxes.cpp` | new | vbox, hbox, grid box, stack, scroll box, list/grid (row slices) |
| `engine/core/src/ui/ui_setters.cpp` | new | setters of FR-7/FR-11/FR-23, dirty marking |
| `engine/core/src/ui/ui_queries.cpp` | new | queries of FR-30, `list_info`, `fully_shown` |
| `engine/core/src/ui/ui_text_source.{hpp,cpp}` | new | copy (with compaction), borrow, key + resolver cache, `text_changed` |
| `engine/core/src/ui/ui_text_seam.{hpp,cpp}` | new | the font seam (measure, cell width, line height, `draw_run`) over spec 005 |
| `engine/core/src/ui/ui_wrap.{hpp,cpp}` | new | pure wrapping, ellipsis, reveal limits |
| `engine/core/src/ui/ui_number.{hpp,cpp}` | new | pure integer formatting (sign, `INT_MIN`, prefix/suffix) |
| `engine/core/src/ui/ui_box_math.{hpp,cpp}` | new | pure distribution (stretch, minimums, remainder) and alignment |
| `engine/core/src/ui/ui_anchor.{hpp,cpp}` | new | pure anchor/fill placement |
| `engine/core/src/ui/ui_layout_measure.cpp` | new | passes 1 and 3 (minimums, wrap) |
| `engine/core/src/ui/ui_layout_arrange.cpp` | new | passes 2 and 4 (widths, heights, positions), overflow warnings, scroll translation |
| `engine/core/src/ui/ui_focus.cpp` | new | `focus`, state, repair, default and remembered focus, entering containers |
| `engine/core/src/ui/ui_navigate.cpp` | new | navigation steps 1–3 and 5, wrap on press vs repeat |
| `engine/core/src/ui/ui_geometric.{hpp,cpp}` | new | pure geometric rule (step 4) |
| `engine/core/src/ui/ui_modal.cpp` | new | modal stack, scope, focus restore |
| `engine/core/src/ui/ui_repeat.{hpp,cpp}` | new | pure repeat and suppression state |
| `engine/core/src/ui/ui_input_raw.cpp` | new | the default source (public input queries, Circle Pad hysteresis), activation check |
| `engine/core/src/ui/ui_update.cpp` | new | `update()`'s order, confirm and cancel |
| `engine/core/src/ui/ui_values.cpp` | new | slider, toggle, choice changes |
| `engine/core/src/ui/ui_list.cpp` | new | list/grid focus, scroll with margins, pages, count changes, the row ring and fill calls |
| `engine/core/src/ui/ui_scroll_box.cpp` | new | window, reveal on focus, translation |
| `engine/core/src/ui/ui_touch.cpp` | new | gestures |
| `engine/core/src/ui/ui_results.cpp` | new | results buffer, stable sort, per-key queries, callbacks, function slots |
| `engine/core/src/ui/ui_reveal.cpp` | new | typewriter state |
| `engine/core/src/ui/ui_style.cpp` | new | the default theme, style lookup, `add_style`, metric comparison, texture slots |
| `engine/core/src/ui/ui_nine_slice.{hpp,cpp}` | new | pure 9-slice geometry (parts, inset clamping, scales) |
| `engine/core/src/ui/ui_cursor.cpp` | new | cursor easing, pulse, pressed frames, hiding after touch |
| `engine/core/src/ui/ui_draw.cpp` | new | the walk (order, offsets, opacity, scroll windows, modals and cursor last) |
| `engine/core/src/ui/ui_draw_widgets.cpp` | new | per-kind content (text, icons, toggle, slider, choice, progress, list rows, scroll bar) |
| `engine/core/src/ui/ui_draw_out.{hpp,cpp}` | new | the only gfx caller: fill/outline/sprite/run, the draw hash, the draw count |
| `engine/core/src/ui/ui_scene.cpp` | new | D30 binding |
| `engine/core/src/ui/ui_debug.cpp` | new | debug draw, dump (Release: empty) |
| `engine/core/src/ui/ui_prompt.cpp` | new | the dialog helper (linked only when called) |
| `engine/core/src/ui/ui_actions.cpp` | new | the `ActionSet` adapter (the only file including `<nnh/actions.hpp>`; linked only when called) |
| `engine/core/src/ui/ui_events.cpp` | new | opt-in bus events (the only file including `<nnh/events.hpp>`; linked only when called) |
| `engine/core/CMakeLists.txt` | changed | one `_nnh_ui_sources` block (`-ffp-contract=off`) |
| `tests/ui/ui_fixture.hpp` + `test_create.cpp`, `test_tree.cpp`, `test_keys.cpp`, `test_box_math.cpp`, `test_anchor.cpp`, `test_wrap.cpp`, `test_number.cpp`, `test_nine_slice.cpp`, `test_text_sources.cpp`, `test_min_size.cpp`, `test_layout_boxes.cpp`, `test_layout_anchors.cpp`, `test_layout_dirty.cpp`, `test_reveal.cpp`, `test_focus.cpp`, `test_navigate.cpp`, `test_geometric.cpp`, `test_remembered_focus.cpp`, `test_modal.cpp`, `test_repeat.cpp`, `test_input_raw.cpp`, `test_input_custom.cpp`, `test_no_leak.cpp`, `test_values.cpp`, `test_results.cpp`, `test_callbacks.cpp`, `test_scene_binding.cpp`, `test_actions_adapter.cpp`, `test_events.cpp`, `test_cancel.cpp`, `test_list.cpp`, `test_grid.cpp`, `test_scroll_box.cpp`, `test_touch.cpp`, `test_style.cpp`, `test_style_data.cpp`, `test_draw.cpp`, `test_draw_widgets.cpp`, `test_draw_equivalence.cpp`, `test_cursor.cpp`, `test_prompt.cpp`, `test_debug.cpp`, `test_ui_misuse.cpp`, `test_ui_cost.cpp`, `test_ui_random_ops.cpp` | new | FR-1–FR-33 (AC-1) |
| `tests/ui_example/test_self_check.cpp` | new | AC-2 golden |
| `tests/CMakeLists.txt` | changed | one 028 block + ctests `ui_self_check_pc`, `ui_check_script_pc` |
| `examples/ui/` | new | `main.cpp`, `demos.hpp`, `names.{hpp,cpp}` (keys → log names), `themes.{hpp,cpp}` (the 9-slice theme), `hud.{hpp,cpp}` (HUD UI), `background.{hpp,cpp}`, `title_scene.{hpp,cpp}`, `options_scene.{hpp,cpp}`, `inventory_scene.{hpp,cpp}`, `gallery_scene.{hpp,cpp}`, `dialog_scene.{hpp,cpp}`, `items.{hpp,cpp}` (300 items, descriptions), `texts.{hpp,cpp}` (the key resolver, two languages), `self_check.{hpp,cpp}`, `check_scene.{hpp,cpp}`, `bench.{hpp,cpp}`, `hand_options.{hpp,cpp}`, `bench_scene.{hpp,cpp}`, `art/make_art.py`, `assets/art/ui.png`, `CMakeLists.txt`, `tests/check.nnhs` |
| `examples/CMakeLists.txt` | changed | `add_subdirectory(ui)` after `transitions` |
| docs (stage 11) | new / changed | `docs/api/ui.md` (new), `docs/guides/menus.md` (new); `gfx.md`, `text.md`, `actions.md`, `tween.md`, `architecture/patterns.md`, `scenes.md` (D30 users), `rendering.md` (the layer-14 convention), `decisions.md` (D48 implemented), `docs/REFERENCES.md`, `CLAUDE.md`, `docs/ROADMAP.md`, `scripts/check_release_strings.sh` (the dump's texts), `specs/README.md` |

No file outside `src/ui/`, the two new headers, CMake lists, tests, the example and docs changes: gfx, input, scenes,
text, the platforms and the contract are untouched.

### Data and memory

| Item | 3DS | 64-bit PC | Notes |
|------|-----|-----------|-------|
| hot node | 32 B | 32 B | one cache line; walked by layout, navigation, hit tests, draw |
| cold record | 56 B | 64 B | the text source holds a pointer |
| **per widget** | **88 B** | 96 B | FR-1's ≤ 96 (`static_assert`ed) |
| pooled row (`UiRow` + index, flags) | 72 B | 80 B | a list of 12 visible rows = 864 B whatever its item count |
| function slot (`SmallFunction`) | 24 B | 32 B | callbacks, row fills, formatters |
| result | 20 B | 20 B | `results` per frame |
| extra text source | 8 B | 16 B | choice options |
| custom style | ≈ 600 B | ≈ 600 B | `static_assert` ≤ 640; the theme itself (≈ 6.7 KB) is the game's (or the engine's `.rodata`) |
| header | ≈ 0.5 KB | ≈ 0.6 KB | roots, focus, modal stack (8 × 4 B), input state, cursor, 8 texture pointers, theme pointer, resolver, source, D30 node, stats, hash |
| stack | < 1 KB | | the 256-byte run buffer of `draw_run`, the geometric scan, layout walks (no recursion) |

The example's blocks (estimates, measured at stage 7 and logged by the bench): HUD 8 widgets ≈ 1.6 KB; title 24
widgets + prompt ≈ 4.5 KB; options 48 widgets + 8 option texts ≈ 7 KB; inventory 24 widgets + 12 rows ≈ 4.4 KB;
gallery 8 widgets + 24 rows ≈ 3.3 KB; dialog 8 widgets + 1 KB text ≈ 2.8 KB; the self-check 512 widgets + 64 rows ≈
52 KB; the bench's 128-widget UI ≈ 13 KB. Total under 100 KB with the self-check and bench, < 25 KB for the showcase.

### Platform work

- **pc:** none.
- **3ds:** none.
- **fake (tests):** none; tests read the fake's recorded `draw_2d` items (`T`/`t` calls) and compare them with the
  items of hand-written gfx calls built in the same test.

### ARM11 cost plan (AC-6)

Lessons from specs 019–027 on the Old 3DS (ARM11 268 MHz, VFPv2, 16 KB L1D with 32-byte lines, no L2, ≈ 110 cycles
per miss, ≈ 40 per prefetched sequential line; instruction counts 1.3–2× low): the upper end of each estimate is
multiplied by 1.3. Built in: (1) **hot nodes**: every walk reads 32-byte records in slot order or by links within one
root (`pld` two nodes ahead in linear scans); (2) **no per-widget work when nothing happened**: an idle update touches
the header only (revealing labels are a list of ≤ 16, the cursor one record); (3) **lists**: a ring of pooled rows;
scrolling by one row calls the fill once and rotates the ring; (4) **scroll boxes translate**, never relayout;
(5) **text**: monospace cell counting (≈ 12 instructions per code point), cached per widget until its text changes;
(6) **no division** in update except the slider's touch mapping and layout's stretch shares (`__aeabi_idiv`, ≈ 40
cycles, only on relayout or a drag); (7) **no float** in update except the Circle Pad compare and the cursor's one
`ease()` per frame; (8) drawing makes exactly the gfx calls a hand-written menu makes: the UI's own share is the walk
(≈ 80–150 instructions per widget) and the run copies (≈ 5 instructions per byte).

Estimates (1 ms = 268 000 cycles; a glyph or sprite ≈ 11.5–12.8 µs all-in, D26/D39 calibration):

| AC-6 workload | Hot path | Estimate (upper × 1.3) | Target | Verdict |
|---------------|----------|------------------------|--------|---------|
| `update()`, 128 widgets, no input | ≈ 12 public input queries (≈ 250 instructions), repeat/suppression for 8 controls, activation check, cursor step, dirty test; ≈ 700 instructions, ≈ 6 header lines | 0.006–0.01 ms (0.013) | < 0.05 ms | expected PASS |
| the same with a focus move (VBox step) | as above + ancestor walk (3–4 nodes), sibling scan (≤ 20 nodes), state/flags of two widgets, results append + sort, cursor retarget: ≈ 2.5 k instructions, ≈ 30 lines | 0.02–0.035 ms (0.045) | < 0.15 ms | expected PASS |
| the same through the geometric rule (128 candidates) | + 128 hot nodes × ≈ 25 instructions, 128 lines prefetched | 0.04–0.06 ms (0.08) | (within the focus-move target) | expected PASS |
| full relayout of both roots (`text_changed()`, 64 keyed texts of ≈ 12 characters, 128 widgets) | 64 resolver calls (≈ 40 instructions each), ≈ 770 code points × ≈ 14, 4 passes × 128 nodes × ≈ 45 instructions, ≈ 20 stretch divisions, ≈ 40 k instructions at ≈ 2 cycles + ≈ 400 line reads | 0.3–0.45 ms (0.58) | < 0.6 ms | expected PASS, **tight** |
| list of 10 000 items scrolling one row per frame (12 visible) | the move + one fill call (the example's: ≈ 150 instructions with a number) + the ring rotation; ≈ 1.5 k instructions | 0.012–0.02 ms (0.026) | < 0.2 ms, within 10 % of 20 items | expected PASS (equal work by construction) |
| `draw()` of the options screen vs the same calls by hand, per recorded draw | ≈ 40 widgets × ≈ 150 instructions + 350 run bytes × 5 + ≈ 40 `draw_text`/`fill_rect` calls' argument set-up ≈ 9 k instructions (≈ 0.07 ms) over ≈ 400 draws × ≈ 12 µs (≈ 4.8 ms) | ratio 1.01–1.03 (1.04) | ≤ 1.15 | expected PASS |
| options screen + 300 game sprites, 600 frames | ≈ 400 UI draws (350 glyphs, 2 × 9 slices, rows) ≈ 4.8 ms + 300 sprites ≈ 3.6 ms + UI update/draw walk 0.15 ms + frame base ≈ 1.5 ms | worst frame 10–11 ms (13.5) | no missed frame of 600 | expected PASS |

The targets stay as the spec wrote them. T7.7 checks the disassembly against this table before the console (no
`__aeabi_*` or VFP in the idle update path, the hot node layout, `pld` in the scans); T10.2 measures; a missed target is
optimised first (risk 1's list; no result, rectangle, golden or draw hash may change); then its budget is set from the
measurement with ≈ 25 % margin and written in the spec, `bench_scene.cpp` and `docs/api/ui.md`. Only if the **frame**
target (or the ratio) is still missed after that does the contingency stage run (T10.4: glyph runs as one batch per
root through D39's mechanism).

### Self-check and bench (AC-2, AC-6)

- **Self-check** (`examples/ui/self_check.{hpp,cpp}`, pure, compiled into the example and `nnh_tests`): one UI (512
  widgets, 64 rows, 2 KB text, 16 functions, 8 texts) with both roots: a title VBox of 6 buttons (one disabled, one
  inert in a `disabled_focusable` box), an options scroll box (2 sliders 0–10 step 1 and 0–100 step 5, a toggle, a
  wrapping choice of 3, a non-wrapping choice of 4, a disabled item, Back as the container's `back`), a 1000-item list
  (12 rows, margin 2, every 7th item disabled, values formatted), a 6-column grid of 64 items, a dialog label (wrapped,
  max 4 lines, reveal 0.5 chars/frame, `show_all` on confirm), a prompt opened from the title (Yes/No), a top-root HUD
  (progress bar, label) with anchors, explicit neighbours from the title to the top root's "Info" button and back. The
  screens are switched by the results (activated keys show/hide panels; the code is the example's own logic, pure).
  Input: a 3600-frame script (`kUiScript`: segments of held controls and touches — holds of 40 frames for repeats,
  wraps on press, pages, confirms, cancels, taps on buttons, toggle, choice arrows, slider drags, list drags past the
  threshold, a slide-out, a modal outside touch, a `set_item_count` change at frame 2000, a `text_changed()` with a
  second resolver language at 2400, a destroy-and-rebuild of the title restoring focus by key at 3000). Each frame:
  `update(input)`, then `draw()`; a running FNV-1a folds the results (kind, key, value, index, widget bits) and the draw
  hash; every 600 frames a checkpoint folds every live widget in slot order (handle bits, kind, rect, state, value,
  flags), the focus, the lists' scroll and focused item and the running hash. Goldens from pc-debug in
  `kUiCheckGolden`/`kUiCheckpoints`, equal in pc-release, GCC 13/14, clang 18, the 32-bit host build and the console.
  The example runs 10 script frames per display frame (≈ 6 s).
- **Bench** (`examples/ui/bench.{hpp,cpp}` workloads, `hand_options.{hpp,cpp}`, `bench_scene.{hpp,cpp}` timing them,
  300 frames each, average and worst, only the measured calls in the profile scope): `idle128` (a 128-widget UI on both
  roots, empty input), `move128` (down/up alternating fresh presses in a 20-button VBox inside it), `geo128` (a move
  resolved by the geometric rule), `relayout128` (`text_changed()` with 64 keyed texts every frame), `list10000` (a
  10 000-item list at item 5000 scrolling down 8 rows and back up, one row per frame) and `list20` (the same at item 0 of
  a 20-item list): ratio of averages; `draw_options` (the options screen of the example: 2 9-slice panels, 12 rows, its
  glyphs) and `hand_options` (the same gfx calls written out from the UI's queried rectangles, texts and style colours
  at setup; a test proves the items equal): cost per recorded draw = the scene's scope + (`gfx submit` − the base run's
  average `gfx submit`), as plans 023/024 measure; `frame` (the options screen + 300 16×16 sprites on the top screen,
  600 frames, missed frames from `time::delta()` > 20 ms); `memory` (bytes per widget and row, the example's blocks).
  Lines: `ui bench: <name> avg X ms worst Y ms (target < Z ms) PASS|FAIL`, `ui bench: list10000 avg X ms list20 avg Y ms
  ratio R (target <= 1.10) PASS|FAIL`, `ui bench: draw ui U us hand H us per draw ratio R draws D glyphs G (target <=
  1.15) PASS|FAIL`, `ui bench: frame worst W ms missed M of 600 PASS|FAIL`, `ui bench: memory widget 88 B row 72 B
  hud … title … options … inventory … gallery … dialog … selfcheck …`, then `ui bench: PASS` or `ui bench: FAIL
  (<names>)`; a Debug build logs `not judged (Debug)`.

### The example `examples/ui` (AC-3, AC-5)

Original art (`art/make_art.py` → `assets/art/ui.png`, 128 × 128: 9-slice frames for panels and buttons (normal,
focused, pressed, disabled), a slider track and knob, toggle off/on, choice arrows, a hand cursor, 16 item icons of
16 × 16, a heart). `AppConfig::gfx = {.max_draws = 4096}`. Scenes, each owning its `Ui` as a member bound to the scene
(D30), so focus memory across pushes and pops needs no code:

- **HUD** (its own UI, top root only, layer 13, over a scrolling stripe background): a health bar (`progress`, 9-slice
  fill) anchored top-left, a score label anchored top-right; both change every second.
- **Title** (bottom root: a VBox of New game, Continue (disabled), Options, Inventory, Gallery, Quit, centred; top
  root: the game's name and a hint label): New game → the dialog; Options, Inventory, Gallery push their scenes; Quit →
  the prompt "Quit? Yes / No" (No by default; cancel = No; Yes ends the program).
- **Options** (pushed; slid in by a spec 015 tween through `set_render_offset` as a setter): a scroll box of Music and
  Sound sliders (0–10), Invert Y (toggle), Difficulty < Easy / Normal / Hard >, Text speed < Slow / Fast >, Language <
  English / Português > (switches the resolver and calls `text_changed()`), Online (disabled, inert: `refused`), six
  more toggles so the box scrolls, Back (the box's `back`).
- **Inventory** (pushed): a 300-item list on the bottom screen (icon, name, count), L/R pages; the focused item's
  description in a 9-slice panel on the top screen (a wrapping label).
- **Gallery** (pushed): a 5-column grid of 48 icons; the focused icon drawn ×4 on the top screen.
- **Dialog** (pushed): a 9-slice box with typewriter text (1 char/frame; A shows all, then A closes).
- SELECT switches the theme (the 9-slice theme / `ui::default_theme()`) in every UI; Y the self-check, X the bench,
  START quits.

Log lines (one per result, names from `names.cpp`): `ui: showcase`, `ui: focus <name>` (lists: `ui: focus list <i>`),
`ui: activated <name>` (`ui: activated quit.no`), `ui: value <name> <v>` (`ui: value music 3`), `ui: refused <name>`,
`ui: blocked`, `ui: cancelled <screen>`, `ui: page <name> <first>`, `ui: theme nine-slice|default`, `ui: dialog shown
<n>/<total>` (when fully shown), `ui: language en|pt`. Screenshots by `UI_SHOT=title|options|list|grid|dialog|modal|
theme2` (fixed inputs, fixed frame): title, options with the focus on the Music slider, the inventory scrolled to item
250, the grid, the dialog half revealed, the Quit modal, the title with the default theme.

### The console script `examples/ui/tests/check.nnhs` (AC-4)

`fail-on "] E "`, `fail-on "ui: FAIL"`, `fail-on "ui bench: FAIL"`; `wait 30`; `expect "ui: showcase" 60`; `wait 10`;
then: `tap Y` and the seven checksum lines and `ui: PASS <golden>` (windows 300); then the showcase, every `expect`
followed by `wait 10` before the next input and never two `expect`s on one frame: `tap DOWN` (focus continue is
skipped: disabled) → `ui: focus title.options`; `tap A` → `ui: activated title.options`; `hold LEFT 40` → `ui: value
music 3` (from 7: the press and repeats at 24, 30, 36); `screenshot options`; a touch drag on the music track → `ui:
value music 8`; `tap DOWN` ×N to Online, `tap A` → `ui: refused online`; `tap B` → `ui: cancelled options` and `ui:
focus title.options` (remembered); `tap DOWN`, `tap A` → inventory; `hold DOWN 120` → `ui: focus list 16`… `tap R` ×n →
`ui: page inventory <first>` and `ui: focus list 250`; `screenshot list`; `tap B`; to the gallery, `tap RIGHT` …
`screenshot grid`; `tap B`; `tap UP` ×n, `tap A` (New game) → dialog, `wait 40`, `screenshot dialog`, `tap A` → `ui: dialog
shown 87/87`; `tap A`; touch Quit → `ui: focus title.quit`, `untouch` → `ui: activated title.quit`, `screenshot modal`;
touch No → `ui: activated quit.no`; `tap SELECT` → `ui: theme default`, `screenshot theme2`; `tap X` → the bench lines
and `ui bench: PASS`; `end`. Exact coordinates, counts and expected lines are recorded from the `--pc` run at T7.6 and
written into "Stage 10: console run (prepared)".

## Test strategy

All unit tests run in `nnh_tests` on the fake platform (Windows; VM 120: GCC 13/14, clang 18, ASan/UBSan, the 32-bit
host build). Expected rectangles, wrap points, focus paths and values are written by hand in the tests, never produced
by the code under test; drawing is compared with the fake's items for the same gfx calls made by hand in the test.

| Requirement | Verified by |
|-------------|-------------|
| FR-1 | `test_create.cpp`: every desc bound both ways (`InvalidArgument` naming the field), `OutOfMemory` (a failing tag), tag bytes = the documented formula, a game tag, two UIs independent, move leaves the source empty, empty UIs answer zero; `static_assert`s 32/56/≤ 64 |
| FR-2 | `test_tree.cpp`: creation under each container, appended as last child, stale handles after destroy (asserts), subtree destroy, slot reuse with a new generation, full → null + assert (+ Release log once), text compaction; `test_ui_cost.cpp`: no allocation |
| FR-3 | `test_create.cpp`: one or two roots, root rectangles 400×240 / 320×240, `set_root_visible`, `root()` null for a missing one |
| FR-4 | `test_keys.cpp`: keys from enums, `find`, duplicates assert, focus by key after destroy-and-rebuild, results carry keys |
| FR-5 | `test_min_size.cpp` (every kind), `test_wrap.cpp` (spaces, hyphens, `\n`, long words, Latin-1 two-byte characters, invalid UTF-8, max lines + `...`), `test_draw_widgets.cpp` (image region/scale/tint, progress fill both directions and text, panel) |
| FR-6 | `test_values.cpp` (button, toggle, slider, choice by direction, confirm and repeat; limits; wrap per choice), `test_list.cpp`, `test_grid.cpp`, `test_touch.cpp` |
| FR-7 | `test_layout_boxes.cpp` (hidden take no space, fixed sizes), `test_draw.cpp` (render offset and opacity move and fade without relayout: rects unchanged, items moved; non-finite values assert) |
| FR-8 | `test_prompt.cpp`: 1–3 buttons, default and cancel answers, modal, results by key, close on answer, focus restored, no new widget kind |
| FR-9 | `test_box_math.cpp` (stretch ratios, minimum-exceeds-share iteration, remainder order, negative extra), `test_layout_boxes.cpp` (VBox/HBox/grid box/stack/panel with padding, spacing, alignment, expand; overflow warning once per widget, Debug/Development only) |
| FR-10 | `test_anchor.cpp` (nine points, offsets, fills with margins), `test_layout_anchors.cpp` (both screen sizes, stacks) |
| FR-11 | `test_list.cpp`: whole-row scrolling, margins, pages, count changes clamping focus and scroll, `set_scroll`/`list_info`, fill calls only for rows coming into view (a counter), `mark_items_changed`, disabled rows skipped or refused, 65 535 items, the 10 000 vs 20 call counts equal; `test_grid.cpp` |
| FR-12 | `test_scroll_box.cpp`: the window, reveal on focus with margins, children outside not drawn and not touchable, translation without relayout (`layout_visits` unchanged), drag scrolling |
| FR-13 | `test_layout_dirty.cpp`: every dirtying change, no relayout without one (`layout_visits`), rectangles on demand, layout at `draw()` after a change made between update and draw (decision 6), the same rects on every compiler (AC-2) |
| FR-14 | `test_focus.cpp`: `focus(w)`, `focus(key)`, focused state and look, results |
| FR-15 | `test_focus.cpp`: hidden, inside hidden containers, disabled skipped vs inert (`Refused`), disabled rows; repair after hide/disable/destroy (next, previous, root default, none) |
| FR-16 | `test_navigate.cpp` (every container kind, neighbours first, slider/choice/toggle consumption, wrap on press not repeat, wrap-on-repeat option, blocked, cross-root only by neighbours), `test_geometric.cpp` (overlap first, gap, centre distance, tree-order ties, empty set) |
| FR-17 | `test_remembered_focus.cpp`: last focused, default, first target; lists remember item and scroll; a rebuilt menu restores by key; push/pop of another scene keeps all of it (D30) |
| FR-18 | `test_modal.cpp`: scope of navigation, confirm/cancel and touch; outside touch ignored or cancelling; drawn last; nesting to 8; 9 asserts; focus restored on close; held confirm after opening ignored |
| FR-19 | `test_touch.cpp`: press/pressed look, release inside/outside, slide-out, slider drag, choice arrows, list tap (+ tap-to-select option), drag threshold 8 vs 9 px, scroll by rows cancelling the press, top-root and disabled touches ignored, D-Pad continuing from a touched widget, cursor hiding option |
| FR-20 | `test_results.cpp` (order whatever the processing order, per-key queries, capacity: drops counted and asserted), `test_callbacks.cpp` (called once in results order, a callback that creates/destroys/hides widgets — drawn correctly the same frame —, destroying the UI from a callback asserts), `test_events.cpp` (off by default, published when on, delivered next dispatch, sizes) |
| FR-21 | `test_scene_binding.cpp`: updated before the scene's `update()`, frozen under a pause scene, reset after exit with focus memory kept, `update()` on a bound UI asserts, moved while bound, destroyed while bound |
| FR-22 | `test_repeat.cpp` (24/6, delay 0 = given repeats, newest direction wins), `test_input_raw.cpp` (D-Pad, Circle Pad hysteresis 0.5/0.4, A/B swapped, L/R, touch; `input::set_blocked` → idle; a transition's block), `test_input_custom.cpp` (`update(input)`, a source function), `test_no_leak.cpp` (held confirm at first update, after `reset_input`, after a modal opens, after a frame gap, after a scene binding becomes top again), `test_actions_adapter.cpp` (buttons, a vector action, remapped confirm), `test_cancel.cpp` (back widget, modal close, plain cancelled) |
| FR-23 | `test_reveal.cpp`: rates 1, 0.5, 0.25, 3 exactly over 600 frames, `show_all`, `fully_shown`, words never jump (line breaks of the whole text) |
| FR-24 | `test_text_sources.cpp`: copy (capacity error), borrow + `mark_text_changed`, key + resolver (called once per key until `text_changed`), names hashed, a null resolution draws nothing, `text_changed` relayouts; `test_draw.cpp`: missing glyphs draw the hollow box |
| FR-25 | `test_number.cpp`: 0, negatives, `INT_MIN`, `INT_MAX`, prefixes/suffixes, a formatter callback, truncation at capacity |
| FR-26 | `test_style.cpp` (per-state looks and precedence, overrides, `add_style`, metric changes relayout and colour changes don't, the default theme readable: contrast ratio computed ≥ 4.5 for text on its backgrounds), `test_nine_slice.cpp` (part rectangles and scales for sizes from the insets' sum to 400, edges within 2⁻¹⁰ of integers, inset clamping below), `test_style_data.cpp` (`NNH_DATA_STRUCT`/`NNH_DATA_TYPE` declarations of every style struct compile; a converted value applied) |
| FR-27 | `test_cursor.cpp`: the eased path in frames (integer positions pinned), pulse cycle, pressed look for 6 frames after a confirm, outline, hidden after touch until a direction |
| FR-28 | `test_draw.cpp` (layers, `ignore_camera`, unaffected by camera position and zoom 0.5/2, parents before children, modal and cursor last, hidden roots nothing, outside a frame nothing, draw counts = gfx's), `test_draw_equivalence.cpp` (the options screen's items equal the hand-written calls' items), `test_draw_widgets.cpp` |
| FR-29 | AC-2 on every compiler and the console; `test_cursor.cpp` (ease rounding pinned); T7.7 (no libm call) |
| FR-30 | `test_debug.cpp`: queries, the dump's exact text of a small tree, debug draw items, stats; Release stubs (pc-release); `check_release_strings.sh` |
| FR-31 | `test_ui_cost.cpp`: allocation counter unchanged after `create` over 100 000 operations; idle update visits no node (a counter); a focus move visits ≤ the rule's set; list cost independent of count (fill calls and node visits equal for 20 and 10 000) |
| FR-32 | `test_ui_misuse.cpp`: every listed case asserts (`run_expecting_assert`) and is harmless on the Release path (pc-release); `test_ui_random_ops.cpp` (100 000 seeded operations: creates, destroys, setters, inputs, modals, draws; invariants: links consistent, one focus, focus a target, rects inside roots unless overflow flagged) under ASan/UBSan |
| FR-33 | include check (the header pulls only its includes; no actions, tween, data, animation, entities, audio, events header), T8.3 probes (`hello` byte-identical; a UI game without `use_actions` has no `ActionSet` symbol; without `prompt`/`set_events`, no prompt or events symbol) |
| AC-1 | all of the above, Windows and VM 120 |
| AC-2 | `tests/ui_example/test_self_check.cpp`, ctest `ui_self_check_pc`, the console's `ui: PASS` |
| AC-3 | PC screenshots read back (T7.3), the log lines |
| AC-4 | `examples/ui/tests/check.nnhs` with `--pc` (ctest `ui_check_script_pc`), then on the console |
| AC-5 | console screenshots compared with the `--pc` run's (T10.2) |
| AC-6 | the bench lines on the console; T7.7's table |
| AC-7 | T0.1/T8.3 sizes and `nm`, `compare.sh` (sprites, text, actions, transitions, platformer pixel-identical), their scripts and goldens, every earlier test, build times (T8.4) |
| AC-8 | stage 11 |

### Planned hand mutants (T8.1)

`build/ref-028/mutants.py`, applied one at a time, the whole `nnh_tests` each (≈ 60): remainder pixels to the last
child; `extra × ratio / Σratio` rounded up; the minimum-exceeds-share loop run once; centre alignment rounded up; an
anchor's end offset sign; a fill margin ignored; wrap without the hyphen break; `\n` treated as a space; long-word
break one cell late; ellipsis when the text exactly fits; the reveal accumulator dropping its fraction; spaces not
counted in reveal; neighbour after axis; disabled not skipped; inert not refused; wrap on repeat allowed; wrap on press
denied; the geometric overlap group dropped; the gap measured from centres; the tie broken by reverse order; cross-root
moves without a neighbour; repair to the previous first; the remembered child ignored; the default ignored; a modal not
scoping touch; a modal not restoring focus; modal depth 9 accepted; repeat at `delay − 1`; interval off by one; the
newest-direction rule reversed; suppression not set after a modal opens; suppression not set after a frame gap;
Circle Pad hysteresis without the release threshold; slider touch rounding down; slider not clamped; choice wrap flag
ignored; toggle left/right passing when it changes the value; list margin 0 instead of the setting; page by rows − 1;
count change not clamping the scroll; every visible row refilled on a one-row scroll; scroll box relayout instead of
translation (visits); drag threshold `>` vs `>=`; slide-out activating; tap-to-select ignored; results sort unstable;
callbacks before the sort; a callback's change not laid out; `INT_MIN` formatted wrong; 9-slice centre width off by one;
inset clamp skipped; cursor ease rounded down; pulse period off by one; pressed frames 5; modal drawn in tree order;
`ignore_camera` false; opacity product truncated; AfterExit clearing focus memory; the draw hash skipping run bytes.
Survivors become tests or are argued equivalent in "Mutation testing".

## Risks and alternatives

1. **The full relayout is tight** (0.3–0.58 ms against 0.6). Mitigations in order, none changing a rectangle: (a) cache
   each keyed text's width and line count by its resolved pointer (an unchanged translation re-measures nothing);
   (b) fold passes 1 and 3 for non-wrapping roots (no wrapped label: one bottom-up pass); (c) reciprocal tables for the
   stretch shares (no `__aeabi_idiv`); then AC-6's measurement-with-margin policy. A language switch is a load-time
   moment for most games (documented).
2. **Glyph cost** (≈ 12 µs per glyph) dominates a text-heavy screen; the UI can't make glyphs cheaper with public calls.
   The frame target is expected to pass (≈ 10–11 ms); if the console misses it after the optimisation round, the
   contingency stage T10.4 adds a UI glyph batch: one draw-list command per root's text through D39's batch mechanism
   (a second user of the particle batch table, with its own expand function writing glyph sprites of the font texture
   into the shared scratch), which removes the per-glyph record, sort and lookup. That is a gfx change (a D39 amendment:
   the budget shared or a second budget), designed then, and the draw-equivalence test would compare items instead of
   calls. Not built unless measured necessary.
3. **Layout at `draw()`** (decision 6) refines FR-13's "never during drawing": `draw()` stays logically read-only (focus,
   values and results never change) but completes a pending relayout so changes made after `update()` (the common
   "on Options, hide this panel and show that one") draw correctly in the same frame. The alternative — changes
   appearing one frame late, or a game-called `layout()` — is the maintainer's call (question 1).
4. **Creation beyond capacity returns a null handle** (decision 2) instead of a `Result` per call; a game that sizes its
   UI wrongly sees an assert in Debug/Development. A per-call `Result<UiWidget>` would make every menu line `.value()`
   (question 2).
5. **The ellipsis is three cells** (`...`, decision 3): spec 005's font has no U+2026 glyph (it would draw the hollow
   box). Adding a one-cell glyph changes spec 005's font table and `hello` by a few bytes (question 3).
6. **One `UiStyle` type for every kind** (decision 4), not one struct per kind: simpler API and theme tables, unused
   parts ignored per kind; ≈ 600 B per style. If spec 014's macros can't describe anonymous nested structs, they get
   names (T6.1).
7. **Determinism of the cursor**: one `ease()` per frame rounded with `floor(x + 0.5f)` under `-ffp-contract=off`;
   pinned by tests and AC-2. A game's custom `EaseFn` is outside the guarantee (as tweens).
8. **Key lookups are linear** (≤ 4096 nodes, ≈ 2 µs per 128): `find`, `focus(key)`, `value(key)` are not for inner
   loops (documented); results carry keys, so per-frame reading never scans widgets.
9. **Handles of another UI are not detected** (as spec 020's pools): a handle from UI A used on UI B may name a live
   widget there. Documented; a UI serial in the handle would cost the shared `Handle` layout.
10. **Text compaction on destroy** moves up to `text_bytes` bytes: a load-time cost; documented (destroying menus every
    frame is not the pattern).
11. **Merging**: single worktree from `main`; insertions at distinct anchors (`engine/core/CMakeLists.txt` after the
    saves block, `tests/CMakeLists.txt` after the saves block, `examples/CMakeLists.txt` after `transitions`), rebased
    before stage 8 and the merge. If spec 029 (game fonts) starts meanwhile it must fit the seam or amend it.

## Decisions taken (to review)

The spec's open questions are decided as it recommends. Plan-level decisions:

1. **Names**: module `ui`, header `<nnh/ui.hpp>` (+ `<nnh/detail/ui_style.hpp>`), class `nnh::Ui`, `UiWidget`
   (= `Handle<detail::UiWidgetTag>`), `UiKey`, `UiText`, `UiStyle`, `UiTheme`, `UiInput`, `UiResult`; tag and log channel
   `ui`; example `examples/ui` (`UI_DEMO`, `UI_SHOT`).
2. **Capacity errors on creation**: a null handle + one Debug/Development assert (Release: one error log per UI);
   `Result` only from `Ui::create` (refinement of FR-2, question 2).
3. **Ellipsis**: `...` (three cells) (question 3).
4. **Styles**: one `UiStyle` type with per-kind parts; a theme is one `UiStyle` per kind + the cursor, held by
   reference (the game's or `ui::default_theme()`); custom styles copied into the block; texture slots 0..7 per UI.
5. **Repeat** is the UI's for every source (`UiInput` carries held and pressed bits); `repeat_delay = 0` takes the
   input's own `repeated` bits.
6. **Layout at `draw()`** of a root changed after `update()` (question 1).
7. **Lists**: visible rows fixed at creation (`rows`), height from them; the row ring.
8. **Wrapped labels**: minimum width = fixed width or the longest word; widths before heights (Clay's order).
9. **Results order** by a stable sort at the end of the update; callbacks after it; bus events after callbacks.
10. **Scroll boxes translate** children; no relayout on scroll.
11. **Keys**: linear scans; duplicates asserted on set.
12. **Slots** reused at once (no quarantine).
13. **The draw hash** (stats) for AC-2's "recorded draw items" on the console; fake-platform tests compare gfx items.
14. **Activation** includes a `frame_count()` gap (a scene above popped) for every input source.
15. **The dialog helper** is `Ui::prompt` + `close_prompt` in `ui_prompt.cpp`; the adapter `Ui::use_actions` in
    `ui_actions.cpp`; events in `ui_events.cpp` — one header, three separately linked objects.
16. **No fuzz target** (no parser); a seeded random-operation test under ASan/UBSan instead.
17. **Example**: one UI per scene bound by D30, a HUD UI on layer 13, the 9-slice theme in C++ (the data-file pattern
    in the guide, compile-tested by `test_style_data.cpp`), Y self-check, X bench, SELECT theme, START quit.

## Implementation (2026-11-25, on the PC)

Built test-first in the worktree `spec-028` from `9aac3be`; commits on `spec-028`, not merged.

### Stage 0 results (baseline)

- `nnh_tests`: 3257 tests, `kMaxTests` 4096 (839 left). After this spec 3571: 525 left, so `kMaxTests` stays (T0.3 raises it below 500).
- `hello` (text / data / bss): pc-release 134 880 / 22 052 / 212 400 (file 285 885 B); pc-debug 206 276 / 26 820 /
  215 152; 3ds-release 202 204 / 28 360 / 107 368 (`.3dsx` 247 628 B); 3ds-dev 220 488 / 32 872 / 109 976.
- 3ds-release `.text`: sprites 222 668, text 204 584, actions 247 212, transitions 331 984, platformer 437 464.
- `gfx.hpp + result.hpp`: 7 844 preprocessed lines, `-fsyntax-only` 0.43–0.54 s.
- Edit `examples/transitions/hud.cpp`: simulator 2.1–2.8 s (`cmake --build` included), `.3dsx` 3.7–3.9 s; clean
  `nnh_core` (pc-debug) 13.5 s.
- 11 reference screenshots (`build/ref-028/ref`, not committed); `compare.sh` compares the two bench shots on their top
  screen only (their bottom screens show live timings).

### Stage results

- **Header cost** (T1.8): `<nnh/ui.hpp>` 8 784 lines (+940; budget +1 500), 0.45–0.54 s (within +0.05 s). It pulls only
  `math`, `gfx`, `input`, `result`, `ease`, `handle`, `name_hash`, `small_function` and `ui_style`.
- **Sizes** (T6.8): hot node 32 B, cold record 56 B on the 3DS / 64 B on 64-bit PCs (`static_assert`ed): **88 B per
  widget on the 3DS** (96 on the PC); pooled row 72 B / 88 B; function slot 24 B / 32 B; result 20 B; a custom style
  about 600 B. The bench's options screen: 5 024 B for 17 widgets (PC).
- **Tests**: 314 UI tests in `nnh_tests` (`ui_*`, `ui_example_*`) and the ctests `ui_style_structs_are_data_loadable`,
  `ui_self_check_pc` and `ui_check_script_pc`.
- **Self-check** (T7.1): goldens `89D4068F D85440B0 7C85E5EA 907D59A4 49E1D0E0 30400DA6` (pc-debug; equal in pc-release
  and in the example's check mode, which draws inside frames). Its test also checks that the script reaches every
  screen, the prompt and its Yes, a refused entry, a blocked move, a page, a cancel and a touch-made value change.
- **PC bench** (pc-dev, WinLibs GCC): idle128 0.002 ms, move128 0.012 ms, geo128 0.006 ms, relayout128 0.048 ms,
  list10000 0.047 ms vs list20 0.048 ms (ratio 0.99), draw 0.83 µs vs 0.91 µs per draw by hand (ratio 0.91, 372
  draws), frame worst 17.1 ms with 0 missed of 600: `ui bench: PASS`.
- **Opt-in** (T8.3): 3ds-release and 3ds-dev `hello.elf`/`.3dsx` byte-identical to stage 0's; PC `hello.exe` sizes,
  file length and `nm` identical (its hash changes on any relink: the PE timestamp). The 3ds-release `ui.elf` has no
  `use_actions`, `ActionSet` or `set_events` code; a probe UI game (`build/ref-028/probe`: a button, update, draw) links
  no prompt, actions or events code. sprites, text, actions, transitions and platformer: `.text` and screenshots
  unchanged.
- **Release strings** (T8.2): `scripts/check_release_strings.sh` gains the UI's dump, overflow and misuse texts; the
  3ds-release `ui.3dsx` and the pc-release `ui.exe` have none of them. (Every pc-release `.exe`, `hello.exe` included,
  already contains `NNH_REMOTE`, which the script reports: not from this spec.)
- **Build times** (T8.4): edit `examples/ui/title_scene.cpp`: simulator 2.0–2.9 s (the reference example measures
  2.1–2.8 s the same way), `.3dsx` 4.0–4.8 s (budget 5 s); clean `nnh_core` 16.0–16.3 s (+45 sources; budget 30 s).
- **Warnings**: 0 on pc-debug, pc-dev, pc-release, 3ds-dev and 3ds-release.

### ARM pre-check (T7.7)

3ds-dev objects, `arm-none-eabi-nm -u` and `objdump -d`:

| Path | Runtime helpers | VFP |
|---|---|---|
| idle update (`ui_update`, `ui_repeat`, `ui_input_raw`, `ui_reveal`, `ui_cursor`) | none on the idle path (`ui_repeat`'s `idivmod` only while a control is held; the cursor's pulse wraps by a compare since this stage) | the Circle Pad compares, the cursor's one `ease()` while it glides |
| focus and navigation (`ui_navigate`, `ui_focus`, `ui_geometric`, `ui_list`) | `__aeabi_idiv(mod)` for grid columns and list rows | none |
| layout (`ui_layout_*`, `ui_box_math`, `ui_text_seam`) | `__aeabi_idiv(mod)`: stretch shares, centring, grid tracks, cells per line | none |
| touch (`ui_touch`, `ui_parts`) | `__aeabi_idiv`, `__aeabi_ldivmod` (the slider's rounding) | none |
| drawing (`ui_draw*`, `ui_nine_slice`) | `__aeabi_idiv(mod)`, `__aeabi_ldivmod` (progress fill) | float positions and 9-slice scales handed to gfx |

No libm call anywhere. `-fstack-usage` (3DS, -O2): deepest frames `out_run` 368 B, `arrange_axis` 368 B,
`out_background` 288 B (440 B before its 9-slice parts became 16-bit), `draw_widget` 272 B; the deepest draw path
(walk → `draw_widget` → `draw_list` → `out_run`) is about 0.85 KiB, the update path about 0.5 KiB (`container_size` no
longer keeps a 1 KB track table). No rectangle, golden or draw hash changed with these fixes.

### Mutation testing (T8.1)

24 hand mutants (`build/ref-028/mutate.py`), each applied alone, `nnh_tests ui_` run in pc-debug: **24 killed, 0
survived** — remainder pixels dropped, centre rounded up, minimums never fixed, the anchor offset's sign, no hyphen
break, an ellipsis on an exact fit, the reveal dropping its fraction, wrap on repeat allowed, the geometric overlap
group dropped, repeat one frame early, the oldest direction winning, no suppression after a modal opens, the Circle Pad
without its release threshold, the slider's touch rounding down, the list margin ignored, the drag threshold `>=`, an
unstable results sort, fills following the camera, opacity truncated, a 5-frame pressed look, a scroll box relaid out
instead of moved, tap-to-select ignored, 9-slice insets not clamped, focus repair skipping the next siblings. (The plan
listed about 60; this pass covered 24 across every pure helper and the main rules.)

### Implementation refinements

- **The row pool** maps item *i* to pooled row *i mod (rows × columns)*: the window always covers whole consecutive
  rows, so no ring head is stored (plan: "ring head u8"). A one-row scroll still fills one row.
- **A progress bar without text** needs no text line of height (its minimum is its padding): give HUD bars a size.
- **The self-check folds the draw hash every 10 script frames** (one `draw()` per display frame in the example, so the
  check scene records one UI's draws a frame); every frame's results are folded as planned.
- **The style structs' data declarations** are checked by a compile-only ctest (`tests/ui/data_check/style_types.cpp`,
  `-fsyntax-only -Werror`): linking them into `nnh_tests` would add types to spec 014's registry tests. Nested parts
  were named from the start (`UiSliderParts`, `UiToggleParts`, `UiChoiceParts`, `UiListParts`, `UiProgressParts`).
- **API additions** (queries and helpers): `Ui::key(w)`, `Ui::revealed(label)`, `Ui::text_cells(label)`,
  `ui_bit(UiControl)`, `UiKey::none()`, `UiText::kind()/text()/key_value()`, `UiStats::layouts` and `block_bytes`,
  `UiSliderParts::track_height`. `UiAnchor` lives in `<nnh/detail/ui_style.hpp>` (the cursor style uses it).
- **`UiBackground::color` defaults to white** (a 9-slice's tint must not default to black).
- **Focus changes made by the game report nothing** (`focus()`, repairs), like `set_value`; navigation and touch do.
- **Stale handles in queries assert** too (Debug/Development), as "Misuse" says; `alive(w)` is the silent check.
- **A choice's confirm wraps** to the first option even when the choice doesn't wrap on Left/Right.
- **`UiDesc::functions`** counts callbacks, row fills and formatters; the resolver and the input source live in the
  block's header.
- **Module files**: besides the plan's list, `ui_internal.hpp` (the module's internal function declarations, included at
  the end of `ui_block.hpp`), `ui_layout.{hpp,cpp}` (the layout driver and the walks' helpers), `ui_widget_new.{hpp,cpp}`
  (the generic part of creating a widget), `ui_release.cpp` (resources freed with a widget, slices compacted),
  `ui_parts.{hpp,cpp}` (part rectangles shared by drawing and touch), `ui_focus.hpp`, `ui_cursor.hpp`, `ui_draw.hpp`,
  `ui_draw_list.cpp` (lists split from per-kind drawing). 45 sources in all, the largest 236 lines.
- **The example** keeps its HUD and background in `hud.{hpp,cpp}` and shares the scenes' plumbing in
  `menu_scene.{hpp,cpp}`; only the top scene draws (the options slide in over the background, not over the title).
- **Tests written in batches**: stages 1–2 saw every test fail first (stub helpers, then a layout bug the tests caught);
  for stages 3–6 the module code was written ahead of its tests in larger steps, then each test run and fixed; the
  24-mutant pass is the evidence that those tests bite.

## Stage 10: console run (prepared)

Prepared at stage 7 (the expected lines are pc-debug's, recorded at T7.6). T10.1 on the PC first:
`scripts/dkp.sh cmake --build --preset 3ds-release` and `scripts/dkp.sh cmake --build --preset 3ds-dev --target
ui_3dsx platformer_3dsx`, without a warning; T7.7's table still describes the objects; 3ds-release `hello.elf`
byte-identical to T0.1's. Then on an Old 3DS, nobody touching it after Y. Three runs, each: open the Homebrew Launcher,
press **Y**, then on the PC (`nnh` builds the Development `.3dsx`, sends it, plays the script and brings the console back;
logs and screenshots in `build/logs/`):

1. `build/pc-debug/tools/nnh/nnh run ui --script examples/ui/tests/check.nnhs` (about 75 s of frames, plus the build).
   Expected, as with `--pc` (recorded 2026-11-25: `nnh: PASS (154 lines, frames 1-4292)`): `ui: showcase`;
   `ui: check started`, `ui: 600 sum 89D4068F`, `ui: 1200 sum D85440B0`, `ui: 1800 sum 7C85E5EA`,
   `ui: 2400 sum 907D59A4`, `ui: 3000 sum 49E1D0E0`, `ui: 3600 sum 30400DA6`, **`ui: PASS 30400DA6`**,
   `ui: focus title.new`; the showcase's `ui: focus title.options`, `ui: activated title.options`,
   `ui: value music 3`, `ui: value music 8` (a touch at 234,25), `ui: focus online`, `ui: refused online`,
   `ui: activated options.back`, `ui: focus title.options` (remembered), `ui: focus title.inventory`,
   `ui: activated title.inventory`, `ui: focus list 17`, `ui: page list 20`, `ui: focus title.inventory`,
   `ui: focus title.gallery`, `ui: activated title.gallery`, `ui: focus grid 1`, `ui: focus grid 6`,
   `ui: focus title.gallery`, `ui: focus title.new`, `ui: activated title.new`, `ui: dialog shown 103/103`,
   `ui: focus title.new`, `ui: focus title.quit` (a touch at 160,183), `ui: activated title.quit`,
   `ui: activated quit.no` (a touch at 183,129), `ui: theme default`; then `ui bench: started`,
   `ui bench: base gfx submit avg …`, `idle128 … (target < 0.05 ms)`, `move128 … (target < 0.15 ms)`,
   `geo128 … (target < 0.15 ms)`, `relayout128 … (target < 0.6 ms)`, `list10000 … list20 … ratio … (target < 0.2 ms,
   <= 1.10)`, `draw ui … hand … per draw ratio … (target <= 1.15)`, `frame worst … missed … of 600`,
   `memory widget 88 B row 72 B …`, then `ui bench: PASS` (or `FAIL (<names>)`). Screenshots in `build/logs/ui-*`:
   title, options, list, grid, dialog, modal, theme2 — compare them with the `--pc` run's (AC-5).
   **Expected from T7.7**: logic lines equal to the PC's; idle ≈ 0.01 ms, move ≈ 0.03, geo ≈ 0.06, relayout 0.35–0.55
   (**tight**), lists ≈ 0.02 with ratio ≈ 1.0, draw ratio ≈ 1.02, frame worst ≈ 10–11 ms with 0 missed. A FAIL fails
   the run after every number is logged: record them in "Console run 1"; T10.3 applies risk 1's list, or the budgets
   are set from the measurement with ≈ 25 % margin.
2. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/gate.nnhs` (≈ 2.5 min). Expected,
   unchanged (no shared code changed): `bench: PASS 4 layers + 300 sprites, …`; `nnh: PASS` (the "sprites bench still
   passes" check, as plan 024 read it).
3. `build/pc-debug/tools/nnh/nnh run platformer --script examples/platformer/tests/level.nnhs` (≈ 40 s). Expected:
   exactly the `replay: … sum` lines in the tree and `60 Hz PASS`; `nnh: PASS`.

## Constitution Check (after design)

| Article | OK? | Note |
|---------|-----|------|
| I | ✅ | Portable core; integer layout, focus, values, scrolling and wrapping; one rounded `ease()` per frame; AC-2 on every compiler, the 32-bit host build and the console |
| II | ✅ | 88 B per widget (3DS), 72 B per pooled row, theme by reference; hot nodes for every walk; lists virtualised; scrolling translates; the UI's draw share ≈ 1–4 %; one target tight (relayout), with mitigations and AC-6's policy; the glyph batch only as a measured contingency |
| III | ✅ | One header + one detail header, light includes, measured (T1.8); example edit times measured (T8.4) |
| IV | ✅ | Queries, results view, callbacks; opt-in events only |
| V | ✅ | No allocation after `create`, no `virtual`, RTTI, exceptions or `printf` in update/draw |
| VI | ✅ | Tests first; hand-computed expectations; draw equivalence with hand-written calls; golden; random operations; mutation pass |
| VII | ✅ | One module of ≈ 34 sources ≤ 250 lines; adapter, prompt and events separately linked |
| VIII | ✅ | API page, guide, linked pages, example, D48 |
| IX | ✅ | Own code from published designs; original art; references cited |
| X | ✅ | No parser; misuse loud in Debug, harmless in Release; ASan/UBSan |
| XI | ✅ | No core change outside the module: `hello` byte-identical (budget 0 B); no dependency on actions, tweens, data, entities, audio |

No constitution exception is requested. D48 is new; D30 gains another user unchanged; D39 is untouched unless the
contingency stage runs (then amended there, with the maintainer's approval). Flagged for the maintainer: questions 1–3
below (layout at draw, capacity errors, the ellipsis) and the tight relayout target (risk 1).

## Questions for the maintainer (answered 2026-11-25: each as recommended)

1. **Layout of changes made after `update()`**: complete the pending layout at the start of `draw()` (recommended:
   menus switch panels in the same frame, `draw()` still never changes focus, values or results), or show such changes
   one frame late (FR-13 literally), or require a game call `ui.layout()`?
2. **Creation beyond capacity**: a null handle + Debug/Development assert + one Release error log (recommended: menus
   stay one line per widget; capacity is a sizing bug like `max_draws`), or `Result<UiWidget>` from every creation call
   (FR-2 literally)?
3. **Ellipsis**: three cells `...` (recommended for now: no change to spec 005's font, `hello` byte-identical), or a
   one-cell `…` glyph added to the built-in font (spec 005's table +1 glyph, `hello` +≈ 16 B)?

**Answers (the maintainer, 2026-11-25):** 1. complete the pending layout at the start of `draw()`; 2. a null handle +
Debug/Development assert + one Release error log; 3. `...` (three cells).
