#include "hand_options.hpp"

#include <nnh/gfx.hpp>

namespace {

const char* const kRowTexts[OptionsScreen::kRows] = {
    "Music volume ............... 7", "Sound effects .............. 5", "Voices ..................... 9",
    "Invert Y axis ............ off", "Difficulty ............ normal", "Text speed .............. fast",
    "Language ............. English", "Vibration ................. on", "Subtitles ................. on",
    "Hints .................... off", "Autosave .................. on", "Colour assist ............ off",
};

constexpr nnh::Region kFrame{0, 0, 24, 24};
constexpr int kInset = 6;
constexpr nnh::Color kInk = nnh::rgb(236, 238, 246);

/// One 9-slice frame drawn by hand: corners kept, edges and centre stretched (insets fit as long as the panel is
/// larger than twice them, which these panels are).
void frame_by_hand(nnh::Screen screen, const nnh::Texture& art, const nnh::UiRect& r) {
    const int xs[3] = {r.x, r.x + kInset, r.x + r.w - kInset}, ws[3] = {kInset, r.w - 2 * kInset, kInset};
    const int ys[3] = {r.y, r.y + kInset, r.y + r.h - kInset}, hs[3] = {kInset, r.h - 2 * kInset, kInset};
    const int sx[3] = {0, kInset, kFrame.w - kInset}, sw[3] = {kInset, kFrame.w - 2 * kInset, kInset};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const nnh::Region src{sx[col], sx[row], sw[col], sw[row]};
            nnh::gfx::draw(screen, art, {static_cast<float>(xs[col]), static_cast<float>(ys[row])},
                           {.region = src,
                            .scale = {static_cast<float>(ws[col]) / static_cast<float>(sw[col]),
                                      static_cast<float>(hs[row]) / static_cast<float>(sw[row])},
                            .layer = 14, .ignore_camera = true});
        }
    }
}

nnh::UiStyle framed_panel() {
    nnh::UiStyle s = nnh::ui::default_theme().panel;
    for (nnh::UiLook& l : s.states) {
        l.background = {.kind = nnh::UiBackgroundKind::NineSlice, .texture = 0, .region = kFrame, .left = kInset,
                        .top = kInset, .right = kInset, .bottom = kInset};
    }
    return s;
}

} // namespace

const char* options_row_text(int i) { return i >= 0 && i < OptionsScreen::kRows ? kRowTexts[i] : ""; }

bool OptionsScreen::build(const nnh::Texture* art) {
    auto r = nnh::Ui::create({.widgets = 24, .text_bytes = 512, .styles = 1});
    if (!r) return false;
    ui_ = static_cast<nnh::Ui&&>(r.value());
    art_ = art;
    if (art != nullptr) ui_.set_texture(0, *art);
    const nnh::UiStyleId framed = ui_.add_style(framed_panel());
    header_ = ui_.panel(ui_.root(nnh::Screen::Top), {.style = framed,
                                                     .layout = {.width = 240, .place = {.anchor = nnh::UiAnchor::Top, .y = 30}}});
    title_ = ui_.label(header_, {.text = "OPTIONS - the bench's screen", .align = nnh::TextAlign::Center});
    panel_ = ui_.panel(ui_.root(nnh::Screen::Bottom),
                       {.as_box = true, .spacing = 2, .style = framed,
                        .layout = {.width = 260, .place = {.anchor = nnh::UiAnchor::Center}}});
    for (int i = 0; i < kRows; ++i) rows_[i] = ui_.label(panel_, {.text = kRowTexts[i]});
    capture();
    return true;
}

void OptionsScreen::capture() {
    header_r_ = ui_.rect(header_);
    title_r_ = ui_.rect(title_);
    panel_r_ = ui_.rect(panel_);
    for (int i = 0; i < kRows; ++i) rows_r_[i] = ui_.rect(rows_[i]);
}

void OptionsScreen::draw_by_hand() const {
    const nnh::TextOptions text{.color = kInk, .layer = 14, .ignore_camera = true};
    if (art_ != nullptr) frame_by_hand(nnh::Screen::Top, *art_, header_r_);
    const int title_w = 28 * 8;
    nnh::gfx::draw_text(nnh::Screen::Top,
                        {static_cast<float>(title_r_.x + ((title_r_.w - title_w) >> 1)), static_cast<float>(title_r_.y)},
                        "OPTIONS - the bench's screen", text);
    if (art_ != nullptr) frame_by_hand(nnh::Screen::Bottom, *art_, panel_r_);
    for (int i = 0; i < kRows; ++i) {
        nnh::gfx::draw_text(nnh::Screen::Bottom, {static_cast<float>(rows_r_[i].x), static_cast<float>(rows_r_[i].y)},
                            kRowTexts[i], text);
    }
}
