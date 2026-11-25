#pragma once
// The UI module's internal functions, one file each (spec 028, D48): declared here, shared by the module's sources
// through ui_block.hpp (which includes this file after the block's layout).

#include <cstdint>

#include "nnh/ui.hpp"

namespace nnh::detail {

struct UiBlock;
struct UiTextRef;

/// Misuse: an error log once per UI in Release, an assert in Debug/Development. Never undefined behaviour.
void ui_misuse(UiBlock& b, const char* what);
/// The engine's default tag ("ui").
MemTag& ui_memory_tag();
/// The block's bytes for `desc` (header and every table, each 16-byte aligned).
std::uint32_t ui_block_bytes(const UiDesc& desc);
/// The style of slot `s` (its custom style, else its kind's theme style).
const UiStyle& style_of(const UiBlock& b, int s);
const UiStyle& theme_style(const UiTheme& theme, UiKind kind);
/// The text of slot `s` (resolved and cached for keys), "" when none.
const char* text_of(UiBlock& b, int s);
const char* text_of_ref(UiBlock& b, std::uint8_t kind, UiTextRef& ref, bool& resolved);
/// Stores `t` as a text source (copies a copy). @return false when the text bytes are full.
bool store_text(UiBlock& b, const UiText& t, std::uint8_t& kind, UiTextRef& ref);
/// Releases a stored copy (compacting the text bytes).
void release_text(UiBlock& b, std::uint8_t kind, const UiTextRef& ref);
/// Function slots: store bytes of a SmallFunction, release, call.
int fn_store(UiBlock& b, const void* fn);
void fn_release(UiBlock& b, std::uint8_t slot);
template <typename F>
const F& fn_at(const UiBlock& b, std::uint8_t slot) {
    return *reinterpret_cast<const F*>(b.fns[slot].bytes);
}

/// Tree (ui_tree.cpp).
int tree_take(UiBlock& b, UiKind kind, int parent);   ///< a new slot appended under `parent`, -1 when full
void tree_destroy(UiBlock& b, int s);
bool key_taken(const UiBlock& b, std::uint16_t key, int except);
bool visible_chain(const UiBlock& b, int s);           ///< it and every ancestor visible
int preorder_next(const UiBlock& b, int root, int s);  ///< the next node in pre-order under `root`, -1 at the end
int preorder_next_visible(const UiBlock& b, int root, int s);

/// Layout (ui_layout_*.cpp).
void layout_root(UiBlock& b, int root_index);
void layout_if_dirty(UiBlock& b);
void layout_scroll_window(UiBlock& b, int scroll_box); ///< translation + window flags after a scroll change

/// Focus (ui_focus.cpp, ui_navigate.cpp, ui_modal.cpp).
bool is_target(const UiBlock& b, int s, bool from_neighbour = false);
int enter_target(const UiBlock& b, int s);             ///< the target focusing `s` reaches (containers entered), -1
void set_focus(UiBlock& b, int s, bool report);        ///< changes focus, remembers it in the ancestors
bool in_modal_scope(const UiBlock& b, int s);
void navigate(UiBlock& b, UiDir d, bool fresh);
void modal_close(UiBlock& b);                          ///< the top modal (one is open)
void ui_step(UiBlock& b, const UiInput& in);           ///< one update (ui_update.cpp)
void ui_free(UiBlock& b);                              ///< unbinds and frees the block (ui_create.cpp)
void ui_read_input(UiBlock& b, UiInput& in);           ///< the source, the actions adapter or raw input
void ui_reset_input(UiBlock& b);

/// Results (ui_results.cpp).
void add_result(UiBlock& b, UiResultKind kind, int s, std::int32_t value = 0, std::int32_t index = -1, int other = -1);
void finish_results(UiBlock& b);                       ///< sort, then callbacks

/// Values (ui_values.cpp): direction use of sliders/choices/toggles (true: consumed); confirm.
bool value_direction(UiBlock& b, int s, UiDir d, bool fresh);
void value_set(UiBlock& b, int s, int v, bool report);
void activate(UiBlock& b, int s, int index = -1);

/// Lists (ui_list.cpp).
bool list_direction(UiBlock& b, int s, UiDir d, bool fresh);  ///< consumed
void list_page(UiBlock& b, int s, int pages);
void list_set_item(UiBlock& b, int s, int item);
void list_set_scroll(UiBlock& b, int s, int first);
void list_fill_rows(UiBlock& b, int s);                       ///< rows coming into view
UiRect list_item_rect(const UiBlock& b, int s, int item);     ///< empty when not visible
int list_item_at(const UiBlock& b, int s, int x, int y);      ///< -1 when none
bool list_row_enabled(UiBlock& b, int s, int item);
int list_row_height(const UiBlock& b, int s);
int list_visible_rows(const UiBlock& b, int s);

/// Scroll boxes (ui_scroll_box.cpp).
void scroll_reveal(UiBlock& b, int scroll_box, int child);
void scroll_set_first(UiBlock& b, int scroll_box, int first);
int scroll_child_count(const UiBlock& b, int scroll_box);

/// Text measuring (ui_text_seam.cpp).
int label_lines(UiBlock& b, int s, int width, int& cells);

/// Reveal (ui_reveal.cpp).
void reveal_step(UiBlock& b);
void reveal_forget(UiBlock& b, int s);

/// Cursor (ui_cursor.cpp).
void cursor_step(UiBlock& b);

/// Update pieces (ui_touch.cpp, ui_input_raw.cpp).
void touch_step(UiBlock& b, const UiInput& in);
void touch_reset(UiBlock& b);
void read_raw_input(UiBlock& b, UiInput& out);

} // namespace nnh::detail
