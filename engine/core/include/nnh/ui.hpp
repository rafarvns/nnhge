#pragma once
/// @file nnh/ui.hpp
/// UI widgets (spec 028, decision D48): menus, options screens, lists, dialogs and HUDs built in C++ from a retained
/// widget tree in one block the game owns. Integer layout in boxes and anchors on either screen, focus moved by the
/// D-Pad, Circle Pad and touch, results read by query or callback, drawing through public gfx calls on one layer.
/// The same rectangles, focus and draws on every system. Optional: a game that doesn't include it links nothing.
/// See docs/api/ui.md and docs/guides/menus.md.
/// @code
/// nnh::Ui ui = nnh::Ui::create({.widgets = 32}).value();
/// const nnh::UiWidget menu = ui.vbox(ui.root(nnh::Screen::Bottom), {.layout = {.place = {.anchor = nnh::UiAnchor::Center}}});
/// ui.button(menu, {.text = "New game", .key = Item::NewGame});
/// ui.button(menu, {.text = "Options", .key = Item::Options});
/// ui.bind(*this);                                   // in a scene: updated before update(), reset after on_exit()
/// // update():  if (ui.activated(Item::Options)) nnh::scenes::push(options);
/// // draw():    ui.draw();
/// @endcode

#include <cstdint>
#include <type_traits>

#include "nnh/detail/handle.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/detail/small_function.hpp"
#include "nnh/detail/ui_style.hpp"
#include "nnh/gfx.hpp"
#include "nnh/input.hpp"
#include "nnh/math.hpp"
#include "nnh/result.hpp"

namespace nnh {

class Scene;
class Texture;
class ActionSet;
struct MemTag;

namespace detail {
struct UiBlock;
struct UiWidgetTag;
} // namespace detail

inline constexpr int kMaxUiWidgets = 4096;   ///< widgets of one UI (its roots included)
inline constexpr int kMaxUiModalDepth = 8;   ///< modals open at once
inline constexpr int kMaxUiTextures = 8;     ///< texture slots a UI's styles and images refer to
inline constexpr int kMaxUiItems = 65535;    ///< items of a list or grid
inline constexpr int kUiDefaultLayer = 14;   ///< below spec 023's transition overlay (15)
inline constexpr int kUiPressedFrames = 6;   ///< a confirmed widget shows its pressed look at least this long
inline constexpr int kUiDragThreshold = 8;   ///< pixels a touch must travel past to scroll a list, grid or scroll box
inline constexpr int kUiNoNumber = INT32_MIN; ///< UiRow::number: no number column

/// A widget of a Ui: spec 020's 32-bit handle (slot + generation; 0 = none). A destroyed widget's handle is stale at
/// once: using it asserts in Debug/Development and does nothing in Release. Handles of another Ui are not detected.
using UiWidget = Handle<detail::UiWidgetTag>;

/// The game's own name for a widget: an enum value (or an int) 0..65534, unique in its Ui. Results carry it.
struct UiKey {
    constexpr UiKey() = default;
    template <typename E>
        requires std::is_enum_v<E>
    constexpr UiKey(E e) : value(static_cast<std::uint16_t>(e)) {} // NOLINT: implicit on purpose
    constexpr explicit UiKey(int v) : value(static_cast<std::uint16_t>(v)) {}
    constexpr bool none() const { return value == 0xFFFF; }
    constexpr bool operator==(const UiKey&) const = default;
    std::uint16_t value = 0xFFFF; ///< 0xFFFF: no key
};

/// A rectangle of whole pixels in its screen's coordinates.
struct UiRect {
    int x = 0, y = 0, w = 0, h = 0;
    constexpr bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    constexpr Rect rect() const {
        return {static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)};
    }
    constexpr bool operator==(const UiRect&) const = default;
};

enum class UiKind : std::uint8_t {
    Root, Label, Image, Progress, Panel, Spacer, Button, Toggle, Slider, Choice,
    List, Grid, VBox, HBox, GridBox, Stack, ScrollBox,
};
/// A widget's look state, the index of UiStyle::states. Precedence when drawn: Disabled, Pressed, Focused, Checked.
enum class UiState : std::uint8_t { Normal, Focused, Pressed, Disabled, Checked };
enum class UiDir : std::uint8_t { Up, Down, Left, Right };
/// Cross-axis alignment of a box's child (Fill: the whole width or height).
enum class UiAlign : std::uint8_t { Start, Center, End, Fill };

/// Where a root's or stack's child goes (FR-10): its `anchor` point on the parent's same point + (x, y); fill_x /
/// fill_y stretch it between the parent's edges with margins x and right / y and bottom.
struct UiPlace {
    UiAnchor anchor = UiAnchor::TopLeft;
    int x = 0, y = 0;
    bool fill_x = false, fill_y = false;
    int right = 0, bottom = 0;
};

/// Size and size flags (FR-7, FR-9). width/height 0: from content. `expand` shares a box's extra space along its axis
/// by `stretch` (1..255); `align` places it on the cross axis; `place` is used under a root or a stack.
struct UiLayout {
    int width = 0, height = 0;
    bool expand = false;
    int stretch = 1;
    UiAlign align = UiAlign::Fill;
    UiPlace place{};
};

/// A text source (FR-24): a copy into the UI's text bytes (a string literal converts to one), a borrowed pointer the
/// game keeps alive (call Ui::mark_text_changed after editing it), or a key the UI's resolver turns into text.
class UiText {
public:
    enum class Kind : std::uint8_t { None, Copy, Borrow, Key };
    constexpr UiText() = default;
    UiText(const char* utf8) : kind_(utf8 != nullptr ? Kind::Copy : Kind::None), text_(utf8) {} // NOLINT: implicit
    static UiText copy(const char* utf8) { return UiText(utf8); }
    static UiText borrow(const char* utf8) {
        UiText t;
        t.kind_ = utf8 != nullptr ? Kind::Borrow : Kind::None;
        t.text_ = utf8;
        return t;
    }
    static constexpr UiText key(std::uint32_t key) {
        UiText t;
        t.kind_ = Kind::Key;
        t.key_ = key;
        return t;
    }
    template <typename E>
        requires std::is_enum_v<E>
    static constexpr UiText key(E e) {
        return key(static_cast<std::uint32_t>(e));
    }
    /// key = FNV-1a 32 of the name ("menu.new_game").
    static constexpr UiText name(const char* name) { return key(detail::fnv1a32(name)); }

    constexpr Kind kind() const { return kind_; }
    constexpr const char* text() const { return text_; }
    constexpr std::uint32_t key_value() const { return key_; }

private:
    Kind kind_ = Kind::None;
    const char* text_ = nullptr;
    std::uint32_t key_ = 0;
};

/// Text for a key: UTF-8 the game keeps alive until the next Ui::text_changed(). nullptr draws nothing.
using UiResolver = detail::SmallFunction<const char*(std::uint32_t key)>;

/// One row of a list or grid, filled by the game (FR-11). Text is borrowed or written into `buffer` (point `text` at it).
struct UiRow {
    const char* text = nullptr;
    int number = kUiNoNumber;        ///< drawn right-aligned (FR-25), e.g. a count
    const char* number_prefix = "";  ///< "x" → "x12"
    int icon_texture = -1;           ///< texture slot of the icon, -1: none
    Region icon{};
    bool enabled = true;
    char buffer[32] = {};
};
/// Fills the row of item `index` (called only when a row comes into view or its item is marked changed).
using UiRowFill = detail::SmallFunction<void(int index, UiRow& row)>;
/// Text for a value (FR-25): write up to `capacity` bytes of UTF-8 into `out` (no NUL needed), return the length.
using UiFormat = detail::SmallFunction<int(int value, char* out, int capacity)>;

/// Options of each widget kind (designated initializers). Every one has a key, a style and a layout.
struct UiLabelDesc {
    UiText text;
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
    bool wrap = false;                ///< word wrap at the label's width (fixed width, or the width its box gives it)
    int max_lines = 0;                ///< 0: any; else the last line ends with "..." when the text is longer
    TextAlign align = TextAlign::Left;
    UiAlign valign = UiAlign::Start;  ///< Start, Center or End
    float reveal_rate = -1;           ///< < 0: all shown; else characters per frame (typewriter, FR-23)
};
struct UiImageDesc {
    int texture = -1;                 ///< texture slot (Ui::set_texture), -1: nothing drawn
    Region region{};
    int scale = 1;                    ///< 1..8
    Color tint = colors::white;
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
struct UiProgressDesc {
    int value = 0, max = 100;
    bool vertical = false;            ///< fills from the bottom
    bool show_text = false;           ///< "value/max" or the formatter's text
    UiFormat format{};
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
/// VBox, HBox, GridBox, Stack.
struct UiBoxDesc {
    int spacing = 4;
    int padding = 0;
    int columns = 2;                  ///< GridBox only
    bool wrap = true;                 ///< navigation along the box wraps at its ends (on a fresh press)
    bool wrap_on_repeat = false;      ///< ... also on a held repeat
    bool disabled_focusable = false;  ///< disabled widgets inside take focus and report Refused when activated
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
/// A background with the style's padding around one child, or (as_box) a VBox/HBox with that background.
struct UiPanelDesc {
    bool as_box = false;
    UiKind box = UiKind::VBox;
    int spacing = 4;
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
struct UiButtonDesc {
    UiText text;
    int icon_texture = -1;
    Region icon{};
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
struct UiToggleDesc {
    UiText text;
    bool checked = false;
    bool left_right_set = true;       ///< Left sets off, Right sets on
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
struct UiSliderDesc {
    UiText text;
    int min = 0, max = 10, step = 1, value = 0;
    bool show_value = true;
    UiFormat format{};
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
struct UiChoiceDesc {
    UiText text;
    const UiText* options = nullptr;  ///< `count` text sources, copied into the UI
    int count = 0;
    int index = 0;
    bool wrap = true;
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
/// A list (columns 1) or a grid (columns > 1) of `count` items, `rows` visible rows (FR-11).
struct UiListDesc {
    int rows = 8;
    int columns = 1;
    int count = 0;
    UiRowFill fill{};
    int margin = 1;                   ///< rows kept visible above and below the focused item
    bool wrap = true;
    bool tap_selects_first = false;   ///< a tap on an unfocused row only focuses it; a second tap activates
    int row_height = 0;               ///< 0: the style's
    int cell_width = 0;               ///< 0: 64 (lists) — rows are filled lazily, so they can't size the list
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
struct UiScrollDesc {
    int spacing = 4;
    int padding = 0;
    int margin = 1;                   ///< children kept visible above and below the focused one
    UiKey key{};
    UiStyleId style{};
    UiLayout layout{};
};
struct UiModalOptions {
    bool cancel_closes = true;
    bool outside_touch_cancels = false;
};
/// A modal prompt (FR-8): a message and 1..3 buttons, opened modally on `screen`.
struct UiPromptDesc {
    UiText message;
    UiText buttons[3];
    UiKey keys[3];
    int count = 2;
    int default_button = 0;
    int cancel_button = 1;            ///< activated by cancel; -1: cancel does nothing
    bool close_on_answer = true;
    int width = 0;                    ///< 0: 3/4 of the screen
};

/// The controls of a UiInput (bit = 1 << control).
enum class UiControl : std::uint8_t { Up, Down, Left, Right, Confirm, Cancel, PageBack, PageForward };
constexpr std::uint8_t ui_bit(UiControl c) { return static_cast<std::uint8_t>(1u << static_cast<unsigned>(c)); }

/// One frame of UI input (FR-22).
struct UiInput {
    std::uint8_t held = 0, pressed = 0; ///< ui_bit(control)
    std::uint8_t repeated = 0;          ///< used only when UiInputConfig::repeat_delay == 0 (the game's own repeat)
    bool touching = false, touch_started = false, touch_ended = false;
    std::int16_t touch_x = 0, touch_y = 0;
};
using UiInputSource = detail::SmallFunction<void(UiInput& out)>;

struct UiInputConfig {
    int repeat_delay = 24, repeat_interval = 6; ///< frames; delay 0: take UiInput::repeated as given
    bool circle_pad = true;
    float circle_press = 0.5f, circle_release = 0.4f;
    Button confirm = Button::A, cancel = Button::B, page_back = Button::L, page_forward = Button::R;
    bool hide_cursor_after_touch = false;
};

/// The game's spec 017 action (an enum value of its ActionSet) for a control, or none.
struct UiActionRef {
    constexpr UiActionRef() = default;
    template <typename E>
        requires std::is_enum_v<E>
    constexpr UiActionRef(E e) : index(static_cast<std::uint16_t>(e)) {} // NOLINT: implicit on purpose
    std::uint16_t index = 0xFFFF;
};
struct UiActionMap {
    UiActionRef up, down, left, right;
    UiActionRef move; ///< a vector action read as four directions (0.5 / 0.4), optional
    UiActionRef confirm, cancel, page_back, page_forward;
    bool touch = true; ///< the bottom screen, raw
};

/// What happened this frame (FR-20), in this order.
enum class UiResultKind : std::uint8_t { FocusChanged, Activated, ValueChanged, Refused, Blocked, Cancelled, Page };
struct UiResult {
    UiResultKind kind = UiResultKind::FocusChanged;
    UiKey key{};
    UiWidget widget{};
    UiWidget other{};         ///< FocusChanged: the widget focused before
    std::int32_t value = 0;   ///< ValueChanged: the new value; Page: -1 / +1
    std::int32_t index = -1;  ///< a list's item
};
struct UiResults {
    const UiResult* data = nullptr;
    int size = 0;
    const UiResult* begin() const { return data; }
    const UiResult* end() const { return data + size; }
};
using UiCallback = detail::SmallFunction<void(const UiResult& result)>;

/// Bus events, published only when a Ui opts in (Ui::set_events); `ui` is the UI's serial.
struct UiActivated { std::uint16_t ui; UiKey key; std::uint32_t widget; std::int32_t index; };
struct UiValueChanged { std::uint16_t ui; UiKey key; std::uint32_t widget; std::int32_t value; };
struct UiFocusChanged { std::uint16_t ui; UiKey key; std::uint32_t widget; std::uint32_t from; };
struct UiCancelled { std::uint16_t ui; UiKey key; };

/// A UI's capacities and settings (FR-1). The block's size is in docs/api/ui.md.
struct UiDesc {
    int widgets = 64;          ///< 1..4096, its roots included
    int text_bytes = 1024;     ///< 0..65535: copied text (each copy + 1 byte)
    int texts = 0;             ///< 0..4096: extra text sources (choice options)
    int styles = 0;            ///< 0..240: custom styles (add_style)
    int rows = 0;              ///< 0..1024: pooled list and grid rows
    int functions = 8;         ///< 0..255: callbacks, row fills, formatters, the resolver and the source
    int results = 32;          ///< 8..256 per frame
    bool top = true, bottom = true; ///< roots
    Screen first_focus = Screen::Bottom; ///< the root focus enters first
    UiInputConfig input{};
    MemTag* tag = nullptr;     ///< default: the engine tag "ui"
};
struct UiListInfo {
    int count = 0, focused = -1, first_visible = 0, visible_rows = 0;
};
struct UiStats {
    int widgets = 0, capacity = 0, text_bytes = 0, text_capacity = 0, rows = 0, functions = 0;
    int layout_visits = 0;           ///< nodes visited by the last layout
    int layouts = 0;                 ///< layouts run since creation
    int draws = 0;                   ///< draws the last draw() recorded (glyphs counted as gfx counts them)
    std::uint32_t draw_hash = 0;     ///< FNV-1a of the last draw()'s gfx calls (AC-2)
    int dropped_results = 0;
    std::uint32_t block_bytes = 0;
};

/// A retained widget tree in one block (D48). Movable, not copyable; destroying it frees the block. Never allocates
/// after create(). Every call with a stale widget asserts in Debug/Development and does nothing (queries answer
/// zero/none) in Release.
class Ui {
public:
    /// One block of tagged memory. Errors: InvalidArgument (naming the field), OutOfMemory.
    static Result<Ui> create(const UiDesc& desc);
    Ui() = default;
    ~Ui();
    Ui(Ui&& other) noexcept;
    Ui& operator=(Ui&& other) noexcept;
    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
    bool valid() const;
    UiWidget root(Screen screen) const; ///< null if the UI has no root there

    // Building (FR-2, FR-5, FR-6, FR-9, FR-12): appended as the parent's last child. A full UI (widgets, text bytes,
    // texts, rows, functions) or a bad desc gives a null handle and asserts (Release: one error log per UI).
    UiWidget label(UiWidget parent, const UiLabelDesc& d);
    UiWidget image(UiWidget parent, const UiImageDesc& d);
    UiWidget progress(UiWidget parent, const UiProgressDesc& d);
    UiWidget panel(UiWidget parent, const UiPanelDesc& d = {});
    UiWidget spacer(UiWidget parent, const UiLayout& layout = {.expand = true});
    UiWidget button(UiWidget parent, const UiButtonDesc& d);
    UiWidget toggle(UiWidget parent, const UiToggleDesc& d);
    UiWidget slider(UiWidget parent, const UiSliderDesc& d);
    UiWidget choice(UiWidget parent, const UiChoiceDesc& d);
    UiWidget list(UiWidget parent, const UiListDesc& d); ///< columns > 1 makes a grid
    UiWidget vbox(UiWidget parent, const UiBoxDesc& d = {});
    UiWidget hbox(UiWidget parent, const UiBoxDesc& d = {});
    UiWidget grid_box(UiWidget parent, const UiBoxDesc& d);
    UiWidget stack(UiWidget parent, const UiBoxDesc& d = {});
    UiWidget scroll_box(UiWidget parent, const UiScrollDesc& d = {});
    /// Destroys `w` and its subtree (focus repaired, text bytes compacted). A root can't be destroyed.
    void destroy(UiWidget w);
    /// FR-8 (ui_prompt.cpp, linked only when called): a centred panel with the message and the buttons, opened modally.
    /// The answer is an Activated result with the button's key. @return the prompt (close it with close_prompt).
    UiWidget prompt(Screen screen, const UiPromptDesc& d);
    void close_prompt(UiWidget prompt);

    // Changing (FR-7, FR-11, FR-23, FR-24). Each marks what it affects for the next layout.
    void set_text(UiWidget w, const UiText& t);
    void mark_text_changed(UiWidget w);
    void set_resolver(UiResolver r);
    void text_changed();                          ///< every key re-resolved, every text measured again
    void set_visible(UiWidget w, bool on);
    void set_enabled(UiWidget w, bool on);
    void set_value(UiWidget w, int v);            ///< slider, progress, toggle (0/1), choice (index): no result
    void set_progress_max(UiWidget w, int max);
    void set_style(UiWidget w, UiStyleId s);
    void set_layout(UiWidget w, const UiLayout& l);
    void set_render_offset(UiWidget w, Vec2 offset); ///< rounded to whole pixels; moves it when drawn, not its layout
    void set_opacity(UiWidget w, float opacity);     ///< 0..1, rounded to 1/255; multiplies the subtree's alpha
    void set_key(UiWidget w, UiKey k);
    void set_neighbour(UiWidget w, UiDir d, UiWidget target); ///< null clears; may name the other root's widget
    void set_default(UiWidget container, UiWidget child);
    void set_back(UiWidget container, UiWidget back);         ///< activated by cancel inside the container
    void set_item_count(UiWidget list, int count);
    void mark_items_changed(UiWidget list, int first = 0, int count = -1);
    void set_scroll(UiWidget list_or_scroll_box, int first);
    void set_item(UiWidget list, int index);      ///< the focused item
    void set_reveal(UiWidget label, int chars);
    void set_reveal_rate(UiWidget label, float chars_per_frame);
    void show_all(UiWidget label);
    void on_activate(UiWidget w, UiCallback cb);
    void on_change(UiWidget w, UiCallback cb);

    // Focus (FR-14 to FR-18)
    void focus(UiWidget w);                       ///< a container: its remembered, default or first target
    void focus(UiKey k);
    UiWidget focused() const;
    UiKey focused_key() const;
    void open_modal(UiWidget container, const UiModalOptions& o = {});
    void close_modal();
    int modal_depth() const;

    // Input and update (FR-21, FR-22)
    void update();                                ///< from the input source (default: raw buttons and touch)
    void update(const UiInput& input);
    void set_input_source(UiInputSource source);  ///< empty: back to raw input
    void use_actions(const ActionSet& set, const UiActionMap& map); ///< ui_actions.cpp: linked only if called
    void reset_input();                           ///< transient state cleared; held controls ignored until released
    void bind(Scene& scene);                      ///< D30: BeforeUpdate → update(), AfterExit → reset_input()
    void unbind();
    void set_events(bool publish);                ///< ui_events.cpp: linked only if called

    // Results (FR-20): valid until the next update.
    UiResults results() const;
    bool activated(UiKey k) const;
    bool activated(UiWidget w) const;
    bool changed(UiKey k) const;
    bool cancelled() const;
    int value(UiKey k) const;
    int value(UiWidget w) const;                  ///< slider/progress value, toggle 0/1, choice index, list item

    // Styles (FR-26, FR-27) and drawing (FR-28)
    void set_theme(const UiTheme& theme);         ///< the game keeps it alive; default ui::default_theme()
    UiStyleId add_style(const UiStyle& style);    ///< copied into the block; {} when full (asserts)
    void set_style_value(UiStyleId id, const UiStyle& style);
    void set_texture(int slot, const Texture& texture); ///< 0..7; the game keeps it alive
    void set_layer(Screen screen, int layer);
    void set_root_visible(Screen screen, bool on);
    /// Records the visible roots with gfx calls (ignore_camera, on each root's layer). Completes a pending layout first;
    /// changes nothing else (focus, values, results).
    void draw() const;

    // Queries and debugging (FR-13, FR-30). Rectangles complete a pending layout first.
    bool alive(UiWidget w) const;
    UiWidget find(UiKey k) const;                 ///< a linear scan: not for inner loops
    UiKind kind(UiWidget w) const;
    UiKey key(UiWidget w) const;
    UiRect rect(UiWidget w) const;
    Screen screen(UiWidget w) const;
    bool visible(UiWidget w) const;               ///< it and every ancestor
    bool enabled(UiWidget w) const;
    UiState state(UiWidget w) const;
    const char* text(UiWidget w) const;           ///< "" when none
    UiListInfo list_info(UiWidget list) const;
    bool fully_shown(UiWidget label) const;
    int revealed(UiWidget label) const;           ///< characters shown
    int text_cells(UiWidget label) const;         ///< characters of the wrapped text
    UiStats stats() const;
    void debug_draw(Screen screen) const;         ///< Debug/Development: rectangles and the focus path; nothing in Release
    void dump() const;                            ///< Debug/Development: the tree to the log; nothing in Release

private:
    friend struct UiAccess;
    detail::UiBlock* block_ = nullptr;
};

namespace ui {
/// Solid colours, no texture, readable on both screens with the built-in font.
const UiTheme& default_theme();
} // namespace ui

} // namespace nnh
