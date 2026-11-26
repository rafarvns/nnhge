// Ui::set_font (spec 029 FR-14): the UI's font slots. Its own object: a UI game that never calls it links none of it
// (the seam reaches slot fonts through text/font_resolve.cpp's pointer).

#include "ui/ui_block.hpp"
#include "ui/ui_internal.hpp"

namespace nnh {

void Ui::set_font(int slot, FontRef font) {
    if (block_ == nullptr) return;
    if (slot < 0 || slot >= kMaxUiFonts) {
        NNH_UI_MISUSE(*block_, "Ui::set_font: a slot outside 0..7");
        return;
    }
    block_->fonts[slot] = font;
    block_->dirty[0] = block_->dirty[1] = true; // every text measured again with the slot's font
}

} // namespace nnh
