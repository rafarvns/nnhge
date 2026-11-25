#include "dialog_scene.hpp"

#include <nnh/input.hpp>

namespace {

const char* const kLine =
    "Welcome, traveller! The north gate opens at dawn. Bring your lantern and twelve coins, and mind the river.";

} // namespace

bool DialogScene::create() {
    if (!make_ui({.widgets = 8, .text_bytes = 160, .top = false})) return false;
    const nnh::UiWidget box = ui_.panel(ui_.root(nnh::Screen::Bottom),
                                        {.layout = {.height = 84,
                                                    .place = {.anchor = nnh::UiAnchor::Bottom, .x = 6, .y = -6,
                                                              .fill_x = true, .right = 6}}});
    text_ = ui_.label(box, {.text = kLine, .wrap = true, .reveal_rate = 1.0f});
    return true;
}

void DialogScene::on_enter() {
    ui_.set_reveal(text_, 0);
    shown_ = false;
    if (shot_is("dialog")) ui_.set_reveal(text_, 45); // half way, for the screenshot
}

void DialogScene::step() {
    const bool pressed = nnh::input::pressed(nnh::Button::A);
    if (pressed && shown_) {
        nnh::scenes::pop();
        return;
    }
    if (pressed) ui_.show_all(text_);
    if (!shown_ && ui_.fully_shown(text_)) {
        shown_ = true;
        NNH_LOG_INFO(example, "ui: dialog shown %d/%d", ui_.revealed(text_), ui_.text_cells(text_));
    }
}
