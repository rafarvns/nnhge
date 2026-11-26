#include <nnh/input.hpp>

#include "scenes.hpp"

using nnh::UiText;

bool DialogScene::create() {
    if (!make_ui({.widgets = 12, .text_bytes = 0, .styles = 1, .functions = 2})) return false;
    const nnh::UiStyleId title = ui_.add_style(title_style());
    const nnh::UiWidget top = ui_.root(nnh::Screen::Top);
    const nnh::UiWidget panel = ui_.panel(top, {.as_box = true, .spacing = 6,
                                                .layout = {.width = 340, .place = {.anchor = nnh::UiAnchor::Center}}});
    ui_.label(panel, {.text = UiText::name("dialog.speaker"), .style = title});
    text_ = ui_.label(panel, {.text = UiText::name("dialog.text"), .wrap = true, .max_lines = 6});
    ui_.label(ui_.root(nnh::Screen::Bottom), {.text = UiText::name("dialog.hint"),
                                              .layout = {.place = {.anchor = nnh::UiAnchor::Center}}});
    return true;
}

void DialogScene::on_enter() {
    LocScene::on_enter();
    ui_.set_reveal(text_, 0);
    ui_.set_reveal_rate(text_, 0.5f); // a character every two frames
    logged_ = false;
}

void DialogScene::step() {
    if (nnh::input::pressed(nnh::Button::A)) ui_.show_all(text_);
    if (!logged_ && ui_.fully_shown(text_)) {
        logged_ = true;
        NNH_LOG_INFO(example, "loc: dialog shown %d/%d", ui_.revealed(text_), ui_.text_cells(text_));
    }
    if (nnh::input::pressed(nnh::Button::B)) nnh::scenes::pop();
}
