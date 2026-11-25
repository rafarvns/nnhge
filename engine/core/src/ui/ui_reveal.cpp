// Typewriter reveal (spec 028 FR-23): a label's visible characters advance by a rate kept exactly in a 16.16
// accumulator (exact for dyadic rates such as 0.5 or 0.25); the wrapping is the whole text's, so words never jump
// lines. Only labels in the revealing list (at most kUiMaxRevealing) cost anything per update.

#include "ui/ui_block.hpp"

namespace nnh {
namespace detail {

bool reveal_rate_bits(float rate, std::uint32_t& out); // ui_widgets_display.cpp

void reveal_track(UiBlock& b, int s) {
    if ((b.hot[s].flags & kUiRevealing) != 0) return;
    if (b.revealing_count >= kUiMaxRevealing) { // documented limit: shown at once
        b.cold[s].data.label.visible = 0xFFFF;
        return;
    }
    b.revealing[b.revealing_count++] = static_cast<std::uint16_t>(s);
    b.hot[s].flags |= kUiRevealing;
}

void reveal_forget(UiBlock& b, int s) {
    if ((b.hot[s].flags & kUiRevealing) == 0) return;
    b.hot[s].flags = static_cast<std::uint8_t>(b.hot[s].flags & ~kUiRevealing);
    for (int i = 0; i < b.revealing_count; ++i) {
        if (b.revealing[i] != s) continue;
        b.revealing[i] = b.revealing[--b.revealing_count];
        return;
    }
}

void reveal_step(UiBlock& b) {
    for (int i = 0; i < b.revealing_count;) {
        const int s = b.revealing[i];
        UiLabelData& l = b.cold[s].data.label;
        l.accumulator += l.rate;
        const std::uint32_t shown = l.accumulator >> 16;
        // the cells are those of the last layout; a label not laid out yet keeps revealing
        if (b.dirty[root_index_of(b, s)] || shown < l.cells) {
            l.visible = static_cast<std::uint16_t>(shown > 0xFFFE ? 0xFFFE : shown);
            ++i;
            continue;
        }
        l.visible = 0xFFFF;
        reveal_forget(b, s); // fully shown: off the list (swaps the last one in)
    }
}

} // namespace detail

namespace {

int label_slot(detail::UiBlock* b, UiWidget w, [[maybe_unused]] const char* who) {
    if (b == nullptr) return -1;
    const int s = detail::slot_of(*b, w);
    if (s < 0 || detail::kind_of(*b, s) != UiKind::Label) {
        NNH_UI_MISUSE(*b, who);
        return -1;
    }
    return s;
}

} // namespace

void Ui::set_reveal(UiWidget w, int chars) {
    const int s = label_slot(block_, w, "Ui::set_reveal: a label");
    if (s < 0) return;
    detail::UiLabelData& l = block_->cold[s].data.label;
    const int c = chars < 0 ? 0 : (chars > 0xFFFE ? 0xFFFE : chars);
    l.visible = static_cast<std::uint16_t>(c);
    l.accumulator = static_cast<std::uint32_t>(c) << 16;
    if (l.rate != 0) detail::reveal_track(*block_, s);
}

void Ui::set_reveal_rate(UiWidget w, float chars_per_frame) {
    const int s = label_slot(block_, w, "Ui::set_reveal_rate: a label");
    if (s < 0) return;
    std::uint32_t rate = 0;
    if (!detail::reveal_rate_bits(chars_per_frame, rate)) {
        NNH_UI_MISUSE(*block_, "Ui::set_reveal_rate: a rate that isn't finite");
        return;
    }
    detail::UiLabelData& l = block_->cold[s].data.label;
    l.rate = rate;
    if (chars_per_frame < 0) {
        l.visible = 0xFFFF;
        detail::reveal_forget(*block_, s);
        return;
    }
    if (l.visible == 0xFFFF) { // starts from the beginning
        l.visible = 0;
        l.accumulator = 0;
    }
    detail::reveal_track(*block_, s);
}

void Ui::show_all(UiWidget w) {
    const int s = label_slot(block_, w, "Ui::show_all: a label");
    if (s < 0) return;
    block_->cold[s].data.label.visible = 0xFFFF;
    detail::reveal_forget(*block_, s);
}

bool Ui::fully_shown(UiWidget w) const {
    const int s = label_slot(block_, w, "Ui::fully_shown: a label");
    if (s < 0) return false;
    detail::layout_if_dirty(*block_);
    const detail::UiLabelData& l = block_->cold[s].data.label;
    return l.visible == 0xFFFF || l.visible >= l.cells;
}

int Ui::revealed(UiWidget w) const {
    const int s = label_slot(block_, w, "Ui::revealed: a label");
    if (s < 0) return 0;
    detail::layout_if_dirty(*block_);
    const detail::UiLabelData& l = block_->cold[s].data.label;
    return l.visible == 0xFFFF || l.visible > l.cells ? l.cells : l.visible;
}

int Ui::text_cells(UiWidget w) const {
    const int s = label_slot(block_, w, "Ui::text_cells: a label");
    if (s < 0) return 0;
    detail::layout_if_dirty(*block_);
    return block_->cold[s].data.label.cells;
}

} // namespace nnh
