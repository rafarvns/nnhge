#pragma once
// A UI's block (spec 028 FR-1, D48): a header, then `widgets` hot nodes (32 B: what layout, navigation, hit tests and
// drawing walk), `widgets` cold records (56 B on the 3DS, 64 B on 64-bit hosts), the copied text bytes, the extra text
// sources (choice options), custom styles, the list row pool, function slots and the results of the frame. Records
// are plain structs switched on a kind byte: no virtual, no allocation after create().

#include <cstddef>
#include <cstdint>

#include "nnh/detail/target_layout.hpp"
#include "nnh/ui.hpp"
#include "scene/scene_attachments.hpp"
#include "ui/ui_repeat.hpp"

namespace nnh::detail {

inline constexpr std::uint16_t kUiNone = 0xFFFF;   ///< no slot
inline constexpr std::uint8_t kUiFreeKind = 0xFF;  ///< kind of a free slot
inline constexpr std::uint8_t kUiNoSlot = 0xFF;    ///< no function slot
inline constexpr int kUiMaxRevealing = 16;         ///< labels revealing at once (documented)
inline constexpr int kUiRunBytes = 256;            ///< longest text run drawn in one call (draw_run's buffer)

/// Hot node flags.
enum UiNodeFlag : std::uint8_t {
    kUiVisible = 1u << 0,
    kUiEnabled = 1u << 1,
    kUiWarned = 1u << 2,     ///< the overflow warning was logged
    kUiBottom = 1u << 3,     ///< in the bottom root's tree
    kUiRevealing = 1u << 4,  ///< in the revealing list
    kUiTouched = 1u << 5,    ///< held by a touch (pressed look)
    kUiScratch = 1u << 6,    ///< layout scratch: an expanding child fixed at its minimum
    kUiInWindow = 1u << 7,   ///< inside its scroll box's window (or not in a scroll box)
};

/// What every walk reads: 32 bytes, one cache line.
struct UiHot {
    std::uint16_t parent = kUiNone, first_child = kUiNone, next = kUiNone, prev = kUiNone; ///< first child's prev: the last
    std::uint16_t generation = 0;
    std::uint8_t kind = kUiFreeKind;
    std::uint8_t flags = 0;
    std::int16_t x = 0, y = 0, w = 0, h = 0;
    std::int16_t min_w = 0, min_h = 0;
    std::uint16_t key = 0xFFFF;
    std::uint8_t style = 0;
    std::uint8_t spare = 0;
    std::uint8_t align = 0;     ///< bits 0-1 UiAlign, bit 2 expand
    std::uint8_t stretch = 1;
    std::uint16_t order = 0;    ///< pre-order index in its root, from the last layout
};
static_assert(sizeof(UiHot) == 32, "a hot node is one 32-byte line");

inline constexpr std::uint8_t kUiExpandBit = 1u << 2;

/// A text source as stored: Copy {word = byte offset}, Borrow {ptr}, Key {word = key, ptr = cached resolution}.
struct UiTextRef {
    std::uint32_t word = 0;
    const char* ptr = nullptr;
};

/// Kind-specific data (16 bytes).
struct UiLabelData {
    std::uint16_t visible = 0xFFFF; ///< characters shown (0xFFFF: all)
    std::uint16_t cells = 0;        ///< characters of the wrapped text (cached at layout)
    std::uint32_t accumulator = 0;  ///< 16.16 characters revealed
    std::uint32_t rate = 0;         ///< 16.16 per frame; 0: no reveal
    std::uint8_t lines = 0;         ///< lines at the last layout
    std::uint8_t max_lines = 0;
    std::uint8_t bits = 0;          ///< bit 0 wrap, bits 1-2 TextAlign, bits 3-4 valign
    std::uint8_t spare = 0;
};
struct UiSliderData {
    std::int32_t value = 0, min = 0, max = 0;
    std::int16_t step = 1;
    std::uint8_t format = kUiNoSlot;
    std::uint8_t show_value = 1;
};
struct UiToggleData {
    std::uint8_t checked = 0;
    std::uint8_t left_right = 1;
};
struct UiChoiceData {
    std::uint16_t index = 0, count = 0, first_text = 0;
    std::uint8_t wrap = 1;
};
struct UiListData {
    std::uint16_t count = 0, focused = 0, first = 0; ///< items, the focused item, the first visible item
    std::uint16_t first_row = 0;                     ///< first pooled row
    std::uint8_t rows = 0, columns = 1;
    std::uint8_t fill = kUiNoSlot;
    std::uint8_t margin = 0;
    std::uint8_t row_height = 0;
    std::uint8_t bits = 0;                           ///< bit 0 wrap, bit 1 tap_selects_first
    std::int16_t cell_width = 0;
};
/// VBox, HBox, GridBox, Stack, ScrollBox, Panel, Root.
struct UiBoxData {
    std::uint8_t padding = 0, spacing = 0;
    std::uint8_t extra = 0;     ///< GridBox columns, ScrollBox margin, Panel: its box kind (0 none, VBox, HBox)
    std::uint8_t bits = 0;      ///< bit 0 wrap, bit 1 wrap on repeat, bit 2 disabled focusable
    std::uint16_t def = kUiNone, last = kUiNone, back = kUiNone;
    std::uint16_t first = 0;    ///< ScrollBox: index of the first visible child
    std::int16_t scroll_px = 0; ///< ScrollBox: pixels its children are moved up
};
struct UiImageData {
    std::int8_t texture = -1;
    std::uint8_t scale = 1;
    std::int16_t rx = 0, ry = 0, rw = 0, rh = 0;
    Color tint{};
};
struct UiProgressData {
    std::int32_t value = 0, max = 100;
    std::uint8_t bits = 0; ///< bit 0 vertical, bit 1 text
    std::uint8_t format = kUiNoSlot;
};
struct UiButtonData {
    std::int8_t icon_texture = -1;
    std::int16_t ix = 0, iy = 0, iw = 0, ih = 0;
    std::uint8_t lines = 0; ///< text lines at the last layout's pass 1
};
union UiKindData {
    UiLabelData label{};
    UiSliderData slider;
    UiToggleData toggle;
    UiChoiceData choice;
    UiListData list;
    UiBoxData box;
    UiImageData image;
    UiProgressData progress;
    UiButtonData button;
    unsigned char raw[16];
};
static_assert(sizeof(UiKindData) == 16, "16 bytes of kind data");

/// Text source kinds as stored (UiText::Kind values).
enum : std::uint8_t { kUiTextNone = 0, kUiTextCopy = 1, kUiTextBorrow = 2, kUiTextKey = 3 };
/// Cold flags.
enum : std::uint8_t { kUiResolved = 1u << 0, kUiFillX = 1u << 1, kUiFillY = 1u << 2 };

/// The rest of a widget.
struct UiCold {
    std::int16_t fixed_w = 0, fixed_h = 0;
    std::uint8_t anchor = 0;
    std::uint8_t cflags = 0;
    std::int16_t px = 0, py = 0, pright = 0, pbottom = 0;
    std::int16_t offset_x = 0, offset_y = 0;
    std::uint8_t opacity = 255;
    std::uint8_t on_activate = kUiNoSlot, on_change = kUiNoSlot;
    std::uint8_t text_kind = kUiTextNone;
    std::uint16_t neighbours[4] = {kUiNone, kUiNone, kUiNone, kUiNone};
    std::uint16_t spare = 0;
    UiTextRef text{};
    UiKindData data{};
};
static_assert(sizeof(UiCold) == 32 + sizeof(UiTextRef) + 16, "the cold record's layout");
static_assert(!kThreeDsLayout || sizeof(UiCold) == 56, "56 bytes per cold record on the 3DS");
static_assert(sizeof(UiCold) <= 64, "at most 64 bytes per cold record");

/// A pooled row of a list or grid: the game's row and the item it holds (-1: none).
struct UiRowSlot {
    UiRow row{};
    std::int32_t index = -1;
};
static_assert(!kThreeDsLayout || sizeof(UiRowSlot) == 72, "72 bytes per pooled row on the 3DS");

/// A function slot: one of the SmallFunction types (same layout), copied as bytes.
struct UiFnSlot {
    alignas(UiCallback) unsigned char bytes[sizeof(UiCallback)];
};
static_assert(sizeof(UiCallback) == sizeof(UiRowFill) && sizeof(UiCallback) == sizeof(UiFormat),
              "function slots hold any of the three");

/// The touch gesture in progress.
struct UiTouch {
    std::uint16_t widget = kUiNone; ///< the touched widget (kUiNone: none or cancelled)
    std::uint16_t scroller = kUiNone; ///< the list, grid or scroll box a drag would scroll
    std::int16_t start_x = 0, start_y = 0;
    std::int32_t start_scroll = 0;
    std::int32_t row = -1;          ///< the touched list item
    std::uint8_t part = 0;          ///< 1 left arrow, 2 right arrow, 3 slider track
    bool active = false;            ///< a touch is held
    bool scrolling = false;
    bool was_focused = false;       ///< the touched item had the focus before
};

/// The focus cursor's animation.
struct UiCursor {
    std::int16_t from_x = 0, from_y = 0, to_x = 0, to_y = 0, x = 0, y = 0;
    std::uint16_t frame = 0;   ///< frames since the move started
    std::uint16_t pulse = 0;   ///< pulse phase
    std::uint16_t target = kUiNone;
    bool placed = false;
    bool hidden = false;       ///< hidden after a touch (hide_cursor_after_touch)
};

struct UiModal {
    std::uint16_t container = kUiNone;
    std::uint16_t focus_before = kUiNone;
    std::uint16_t focus_before_gen = 0;
    bool cancel_closes = true;
    bool outside_touch_cancels = false;
};

struct UiBlock {
    // tables
    UiHot* hot = nullptr;
    UiCold* cold = nullptr;
    char* text = nullptr;
    UiTextRef* texts = nullptr;
    std::uint8_t* text_kinds = nullptr;
    UiStyle* styles = nullptr;
    UiRowSlot* rows = nullptr;
    UiFnSlot* fns = nullptr;
    std::uint8_t* fn_used = nullptr;
    UiResult* results = nullptr;
    // capacities and use
    int capacity = 0, text_capacity = 0, texts_capacity = 0, styles_capacity = 0, rows_capacity = 0;
    int fns_capacity = 0, results_capacity = 0;
    int live = 0, text_used = 0, texts_used = 0, styles_used = 0, rows_used = 0, fns_used = 0;
    int result_count = 0, dropped_results = 0;
    std::uint32_t bytes = 0;
    MemTag* tag = nullptr;
    std::uint16_t serial = 0;
    // roots
    std::uint16_t roots[2] = {kUiNone, kUiNone}; ///< [Screen]
    std::uint8_t layers[2] = {kUiDefaultLayer, kUiDefaultLayer};
    bool root_visible[2] = {true, true};
    bool dirty[2] = {true, true};
    std::uint16_t wrap_labels[2] = {0, 0}; ///< wrapping labels per root: none → heights measured with widths
    Screen first_focus = Screen::Bottom;
    // focus
    std::uint16_t focus = kUiNone;
    UiModal modals[kMaxUiModalDepth]{};
    int modal_depth = 0;
    // input
    UiInputConfig input{};
    UiInputSource source{};
    const ActionSet* actions = nullptr;
    UiActionMap action_map{};
    void (*adapter)(UiBlock& b, UiInput& out) = nullptr; ///< ui_actions.cpp, set by use_actions
    RepeatState repeat{};
    std::uint8_t raw_held = 0;          ///< the default source's previous held bits
    std::uint8_t suppressed = 0;        ///< controls ignored until released
    bool touch_suppressed = false;
    bool circle[4] = {};                ///< Circle Pad directions held (hysteresis)
    bool active = false;                ///< updated before (false: the next update is an activation)
    bool activate_pending = false;      ///< a modal opened since the last update
    std::uint64_t last_frame = 0;
    bool ignore_window = false;         ///< a scroll box's own step may reach children outside its window
    UiTouch touch{};
    std::uint16_t pressed_slot = kUiNone; ///< the widget showing its pressed look after a confirm
    std::uint16_t pressed_frames = 0;     ///< frames of it left
    // text
    UiResolver resolver{};
    std::uint16_t revealing[kUiMaxRevealing] = {};
    int revealing_count = 0;
    // look
    const UiTheme* theme = nullptr;
    const Texture* textures[kMaxUiTextures] = {};
    FontRef fonts[kMaxUiFonts] = {}; ///< Ui::set_font (spec 029); 32 bytes
    UiCursor cursor{};
    // scene binding, events
    Scene* scene = nullptr;
    SceneAttachment attachment{};
    void (*publish)(const UiBlock& b) = nullptr; ///< ui_events.cpp, set by set_events(true)
    void (*prompt_hook)(UiBlock& b) = nullptr;   ///< ui_prompt.cpp: closes answered prompts
    // state
    int busy = 0;                       ///< > 0 while game callbacks run
    bool orphaned = false;              ///< its Ui was destroyed from a callback: freed when the update ends
    bool error_logged = false;          ///< Release: one error log per UI
    int layout_visits = 0, layouts = 0;
    mutable int draws = 0;
    mutable std::uint32_t draw_hash = 0;
};

// ---- slots and handles -----------------------------------------------------------------------------------------

inline bool slot_live(const UiBlock& b, int s) {
    return s >= 0 && s < b.capacity && b.hot[s].kind != kUiFreeKind;
}
inline UiWidget handle_of(const UiBlock& b, int s) {
    return s < 0 || s >= b.capacity || b.hot[s].kind == kUiFreeKind
               ? UiWidget{}
               : UiWidget::from_bits(static_cast<std::uint32_t>(s) | (std::uint32_t{b.hot[s].generation} << 16));
}
/// The slot of a live handle, -1 otherwise.
inline int slot_of(const UiBlock& b, UiWidget w) {
    if (!w) return -1;
    const int s = w.slot();
    if (s >= b.capacity || b.hot[s].kind == kUiFreeKind || b.hot[s].generation != w.generation()) return -1;
    return s;
}
inline UiKind kind_of(const UiBlock& b, int s) { return static_cast<UiKind>(b.hot[s].kind); }
inline int screen_index(Screen s) { return s == Screen::Bottom ? 1 : 0; }
inline int root_index_of(const UiBlock& b, int s) { return (b.hot[s].flags & kUiBottom) != 0 ? 1 : 0; }

/// Kinds that hold children.
inline bool is_container(UiKind k) {
    return k == UiKind::Root || k == UiKind::Panel || k == UiKind::VBox || k == UiKind::HBox ||
           k == UiKind::GridBox || k == UiKind::Stack || k == UiKind::ScrollBox;
}
/// Kinds that take focus (FR-6).
inline bool is_interactive(UiKind k) {
    return k == UiKind::Button || k == UiKind::Toggle || k == UiKind::Slider || k == UiKind::Choice ||
           k == UiKind::List || k == UiKind::Grid;
}
inline bool is_list(UiKind k) { return k == UiKind::List || k == UiKind::Grid; }
inline UiRect rect_of(const UiHot& n) { return {n.x, n.y, n.w, n.h}; }

/// Marks the root of slot `s` for layout.
inline void mark_dirty(UiBlock& b, int s) { b.dirty[root_index_of(b, s)] = true; }

/// Every key resolved again and every text measured again (Ui::text_changed(); spec 031: after a hot reload).
inline void forget_text(UiBlock& b) {
    for (int s = 0; s < b.capacity; ++s) b.cold[s].cflags &= static_cast<std::uint8_t>(~kUiResolved);
    for (int i = 0; i < b.texts_used; ++i) b.text_kinds[i] &= 0x7F;
    b.dirty[0] = b.dirty[1] = true;
}

} // namespace nnh::detail

#if defined(NNH_FLAVOR_RELEASE)
#define NNH_UI_MISUSE(b, what) ::nnh::detail::ui_misuse(b, nullptr)
#else
#define NNH_UI_MISUSE(b, what) ::nnh::detail::ui_misuse(b, what)
#endif

#include "ui/ui_internal.hpp"

namespace nnh {
/// Reaches a Ui's block (friend of Ui).
struct UiAccess {
    static detail::UiBlock* block(const Ui& ui) { return ui.block_; }
};
} // namespace nnh
