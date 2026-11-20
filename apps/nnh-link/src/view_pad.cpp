// The number pad screens: the pairing code and a server address typed by hand (views.hpp).

#include "views.hpp"

namespace link_app::view {

using nnh::Screen;
using nnh::gfx::draw_text;
using nnh::gfx::fill_rect;

namespace {

const char* label_of(char c) {
    static char digit[2] = {0, 0};
    if (c == Pad::kErase) return "DEL";
    if (c == Pad::kOk) return "OK";
    digit[0] = c;
    return digit;
}

void keys(const Pad& pad, bool locked) {
    for (int i = 0; i < Pad::kKeyCount; ++i) {
        const Pad::Key& k = Pad::key(i);
        const nnh::Rect r{static_cast<float>(Pad::kLeft + k.column * Pad::kKeyWidth + 2),
                          static_cast<float>(Pad::kTop + k.row * Pad::kKeyHeight + 2),
                          static_cast<float>(Pad::kKeyWidth - 4), static_cast<float>(k.rows * Pad::kKeyHeight - 4)};
        if (!pad.enabled(i)) continue;
        fill_rect(Screen::Bottom, r, locked ? nnh::rgb(36, 40, 52) : nnh::rgb(48, 56, 80));
        if (i == pad.focus() && !locked) nnh::gfx::draw_rect(Screen::Bottom, r, kAccent);
        draw_text(Screen::Bottom, {r.x + r.w / 2, r.y + r.h / 2 - 8}, label_of(k.c),
                  {.color = locked ? kDim : nnh::colors::white, .scale = 2, .align = nnh::TextAlign::Center});
    }
}

} // namespace

void draw_pad(const LinkState& link, const Pad& pad) {
    const bool pairing = link.screen() == link_app::Screen::Code;
    header(pairing ? link.server().name : "address");
    if (pairing) {
        draw_text(Screen::Top, {8, 40}, "Pair with this PC");
        draw_text(Screen::Top, {8, 60}, "Enter the 6-digit code that  nnh serve  shows.", {.color = kDim});
        draw_text(Screen::Top, {8, 74}, "You do this once per PC.", {.color = kDim});
        if (link.checking()) draw_text(Screen::Top, {8, 110}, "Checking...", {.color = kAccent});
        help("Touch or D-Pad + A to type   B cancel");
    } else {
        draw_text(Screen::Top, {8, 40}, "The PC's address on your network");
        draw_text(Screen::Top, {8, 60}, "For example 192.168.0.10 (see the network", {.color = kDim});
        draw_text(Screen::Top, {8, 74}, "settings of the PC that runs  nnh serve).", {.color = kDim});
        if (!link.message().empty()) draw_text(Screen::Top, {8, 110}, link.message().c_str(), {.color = kWarn});
        help("Touch or D-Pad + A to type   B back");
    }
    // The typed text, boxed; the code shows its six places.
    fill_rect(Screen::Bottom, {40, 14, 240, 44}, nnh::rgb(12, 14, 22));
    std::string shown = link.typed();
    if (pairing) {
        while (shown.size() < LinkState::kCodeLength) shown += '_';
    }
    draw_text(Screen::Bottom, {160, 26}, shown.c_str(), {.color = kAccent, .scale = pairing ? 3 : 2,
                                                         .align = nnh::TextAlign::Center});
    keys(pad, link.checking());
}

} // namespace link_app::view
