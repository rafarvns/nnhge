#pragma once
// Creating widgets (ui_widget_new.cpp): shared by the widget kind files.

#include "ui/ui_block.hpp"

namespace nnh::detail {

/// A new slot of `kind` under `parent` with its key, style and layout; -1 (and a misuse report) when refused.
int widget_new(UiBlock& b, UiWidget parent, UiKind kind, UiKey key, UiStyleId style, const UiLayout& layout);
/// Writes the layout values (also Ui::set_layout).
void apply_layout(UiBlock& b, int s, const UiLayout& l);
/// Destroys a half-made widget and reports why (always a null handle for the caller).
void widget_drop(UiBlock& b, int s, const char* why);

int texts_take(UiBlock& b, int count); ///< ui_release.cpp: -1 when full
int rows_take(UiBlock& b, int count);
const char* option_text(UiBlock& b, int i); ///< ui_text_source.cpp

/// A stored formatter / fill / callback: its slot, kUiNoSlot when empty, -1 when the slots are full.
template <typename F>
int store_fn(UiBlock& b, const F& fn) {
    if (!fn) return kUiNoSlot;
    return fn_store(b, &fn);
}

} // namespace nnh::detail

#if defined(NNH_FLAVOR_RELEASE)
#define NNH_UI_DROP(b, s, why) ::nnh::detail::widget_drop(b, s, nullptr)
#else
#define NNH_UI_DROP(b, s, why) ::nnh::detail::widget_drop(b, s, why)
#endif
