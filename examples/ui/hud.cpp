#include "hud.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

#include "themes.hpp"

bool Hud::create(const Demo& demo) {
    auto r = nnh::Ui::create({.widgets = 8, .text_bytes = 32, .bottom = false, .first_focus = nnh::Screen::Top});
    if (!r) return false;
    ui_ = static_cast<nnh::Ui&&>(r.value());
    ui_.set_layer(nnh::Screen::Top, 13);
    apply_look(ui_, demo);
    const nnh::UiWidget top = ui_.root(nnh::Screen::Top);
    heart_ = ui_.image(top, {.texture = 0, .region = kHeart, .layout = {.place = {.x = 4, .y = 4}}});
    health_ = ui_.progress(top, {.value = 80, .max = 100,
                                 .layout = {.width = 96, .height = 10, .place = {.x = 20, .y = 5}}});
    score_ = ui_.label(top, {.text = nnh::UiText::borrow(score_text_),
                             .layout = {.place = {.anchor = nnh::UiAnchor::TopRight, .x = -6, .y = 6}}});
    return true;
}

void Hud::step(int frame) {
    if (frame % 60 != 0) return;
    const int second = frame / 60;
    ui_.set_value(health_, 100 - (second * 13) % 90);
    std::snprintf(score_text_, sizeof score_text_, "SCORE %d", second * 150);
    ui_.mark_text_changed(score_);
}

void Hud::draw(int frame) const {
    // stripes scrolling left on the top screen (the game's own drawing, below the UI's layers)
    for (int i = -1; i < 12; ++i) {
        const int x = i * 40 - (frame / 2) % 40;
        nnh::gfx::fill_rect(nnh::Screen::Top, {static_cast<float>(x), 0, 20, 240}, nnh::rgb(22, 28, 52));
    }
    ui_.draw();
}
