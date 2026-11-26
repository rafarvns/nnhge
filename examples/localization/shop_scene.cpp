#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

#include "scenes.hpp"

using nnh::UiText;

namespace {

constexpr int kCounts[5] = {0, 1, 2, 5, 21};

/// The line for the log: as is when it is ASCII, else its bytes in hex (logs stay ASCII on every console).
void log_line(int count, const char* text) {
    bool ascii = true;
    for (const char* p = text; *p != 0; ++p) ascii = ascii && static_cast<unsigned char>(*p) < 0x80;
    if (ascii) {
        NNH_LOG_INFO(example, "loc: shop %d %s", count, text);
        return;
    }
    char hex[200];
    int at = 0;
    for (const char* p = text; *p != 0 && at < static_cast<int>(sizeof hex) - 3; ++p)
        at += std::snprintf(hex + at, sizeof hex - static_cast<std::size_t>(at), "%02X", static_cast<unsigned char>(*p));
    NNH_LOG_INFO(example, "loc: shop %d %s %s", count, nnh::loc::code(), hex);
}

} // namespace

bool ShopScene::create() {
    if (!make_ui({.widgets = 8, .text_bytes = 0, .styles = 1, .functions = 2})) return false;
    const nnh::UiStyleId title = ui_.add_style(title_style());
    ui_.label(ui_.root(nnh::Screen::Top), {.text = UiText::name("shop.title"), .style = title,
                                           .layout = {.place = {.anchor = nnh::UiAnchor::Top, .y = 12}}});
    const nnh::UiWidget buy = ui_.button(ui_.root(nnh::Screen::Bottom),
                                         {.text = UiText::name("shop.buy"),
                                          .layout = {.width = 160, .place = {.anchor = nnh::UiAnchor::Center}}});
    ui_.focus(buy);
    return true;
}

void ShopScene::on_enter() {
    logged_ = 0;
    LocScene::on_enter();
}

void ShopScene::language_changed() {
    nnh::loc::format(lines_[0], sizeof lines_[0], "shop.greeting", nnh::loc::arg("name", "Ana"));
    for (int i = 0; i < 5; ++i)
        nnh::loc::format(lines_[i + 1], sizeof lines_[i + 1], "shop.items", nnh::loc::arg("count", kCounts[i]));
    if (nnh::scenes::top() == this && logged_ != nnh::loc::serial()) { // once per language shown
        logged_ = nnh::loc::serial();
        for (int i = 0; i < 5; ++i) log_line(kCounts[i], lines_[i + 1]);
    }
}

void ShopScene::step() {
    if (nnh::input::pressed(nnh::Button::B)) nnh::scenes::pop();
}

void ShopScene::paint() const {
    const nnh::FontRef body = nnh::loc::font("body");
    char price[96];
    nnh::loc::format(price, sizeof price, "shop.price", nnh::loc::arg("gold", 1234567));
    nnh::gfx::draw_text(nnh::Screen::Top, {200, 56}, lines_[0], {.align = nnh::TextAlign::Center, .font = body});
    for (int i = 0; i < 5; ++i) {
        nnh::gfx::draw_text(nnh::Screen::Top, {200, 84.0f + 20.0f * static_cast<float>(i)}, lines_[i + 1],
                            {.align = nnh::TextAlign::Center, .font = body});
    }
    nnh::gfx::draw_text(nnh::Screen::Top, {200, 196}, price,
                        {.color = nnh::rgb(255, 214, 120), .align = nnh::TextAlign::Center, .font = body});
}
