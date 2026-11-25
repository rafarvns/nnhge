// The dialog helper (spec 028 FR-8): a modal prompt built from the primitives — a panel (a VBox) centred on the
// screen's root, a wrapping message and an HBox of 1..3 buttons — opened modally on its default button; cancel activates
// the cancel button. The answer is the button's Activated result; with close_on_answer the prompt closes and is
// destroyed at the end of that update (a hook set only when prompt() is linked).

#include "ui/ui_block.hpp"

namespace nnh {
namespace detail {
namespace {

constexpr std::uint8_t kPromptBit = 1u << 3; // cflags of the panel: a prompt
constexpr std::uint8_t kCloseBit = 1u << 4;  // ... closed by its answer

void close_answered(UiBlock& b) {
    for (int i = 0; i < b.result_count; ++i) {
        if (b.results[i].kind != UiResultKind::Activated) continue;
        const int s = slot_of(b, b.results[i].widget);
        for (int n = s < 0 ? kUiNone : b.hot[s].parent; n != kUiNone; n = b.hot[n].parent) {
            const std::uint8_t f = b.cold[n].cflags;
            if ((f & kPromptBit) == 0) continue;
            if ((f & kCloseBit) != 0) {
                if (b.modal_depth > 0 && b.modals[b.modal_depth - 1].container == n) modal_close(b);
                tree_destroy(b, n);
            }
            break;
        }
    }
}

} // namespace
} // namespace detail

UiWidget Ui::prompt(Screen screen, const UiPromptDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    const UiWidget root = this->root(screen);
    if (!root || d.count < 1 || d.count > 3 || d.default_button < 0 || d.default_button >= d.count ||
        d.cancel_button < -1 || d.cancel_button >= d.count) {
        NNH_UI_MISUSE(*b, "Ui::prompt: a screen with a root, 1..3 buttons, default and cancel among them");
        return {};
    }
    const int width = d.width > 0 ? d.width : screen_width(screen) * 3 / 4;
    const UiWidget panel = this->panel(
        root, {.as_box = true, .box = UiKind::VBox, .spacing = 8,
               .layout = {.width = width, .place = {.anchor = UiAnchor::Center}}});
    if (!panel) return {};
    const UiWidget message = label(panel, {.text = d.message, .wrap = true, .align = TextAlign::Center});
    const UiWidget row = hbox(panel, {.spacing = 8, .layout = {.align = UiAlign::Center}});
    UiWidget buttons[3];
    for (int i = 0; i < d.count; ++i) buttons[i] = button(row, {.text = d.buttons[i], .key = d.keys[i]});
    for (int i = 0; i < d.count; ++i) {
        if (!buttons[i] || !message || !row) { // full: nothing half-built stays
            destroy(panel);
            return {};
        }
    }
    const int s = detail::slot_of(*b, panel);
    b->cold[s].cflags |= detail::kPromptBit | (d.close_on_answer ? detail::kCloseBit : 0);
    set_default(panel, buttons[d.default_button]);
    if (d.cancel_button >= 0) set_back(panel, buttons[d.cancel_button]);
    b->prompt_hook = &detail::close_answered;
    open_modal(panel, {.cancel_closes = false});
    focus(buttons[d.default_button]);
    return panel;
}

void Ui::close_prompt(UiWidget prompt) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    const int s = detail::slot_of(*b, prompt);
    if (s < 0 || (b->cold[s].cflags & detail::kPromptBit) == 0) {
        NNH_UI_MISUSE(*b, "Ui::close_prompt: not a prompt");
        return;
    }
    for (int i = b->modal_depth - 1; i >= 0; --i) {
        if (b->modals[i].container == s) {
            while (b->modal_depth > i) detail::modal_close(*b);
            break;
        }
    }
    destroy(prompt);
}

} // namespace nnh
