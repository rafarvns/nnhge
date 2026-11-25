#pragma once
// Drawing internals (ui_draw.cpp, ui_draw_widgets.cpp).

#include "ui/ui_block.hpp"
#include "ui/ui_draw_out.hpp"

namespace nnh::detail {

/// The screen, layer, accumulated render offset and opacity of slot `s`.
DrawCtx context_of(const UiBlock& b, int s);
/// One widget: its state's background, then its content (not its children).
void draw_widget(const UiBlock& b, const DrawCtx& d, int s);
void draw_root(const UiBlock& b, int root_index);

} // namespace nnh::detail
