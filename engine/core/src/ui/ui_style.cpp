// Styles and themes (spec 028 FR-26): the engine's default theme (solid colours, no texture, readable on both screens:
// text contrast at least 4.5 on its backgrounds), the lookup by kind and UiStyleId, custom styles copied into the
// block (a change of metrics relayouts, a change of colours doesn't), texture slots, layers and root visibility.

#include <cstring>

#include "ui/ui_block.hpp"

namespace nnh {
namespace {

UiBackground solid(Color c, bool border = false, Color border_color = {}) {
    UiBackground bg;
    bg.kind = UiBackgroundKind::Solid;
    bg.color = c;
    bg.border = border;
    bg.border_color = border_color;
    return bg;
}

constexpr Color kInk = rgb(236, 238, 246);       // text on dark
constexpr Color kDarkInk = rgb(22, 22, 32);      // text on the focus colour
constexpr Color kFace = rgb(44, 52, 84);         // a button's face
constexpr Color kFocus = rgb(244, 204, 72);      // the focus colour
constexpr Color kPress = rgb(255, 240, 170);
constexpr Color kOff = rgb(38, 40, 50);          // disabled face
constexpr Color kOffInk = rgb(164, 164, 176);
constexpr Color kEdge = rgb(96, 108, 150);

/// A focusable widget's looks: a face per state.
void control(UiStyle& s) {
    s.states[0] = {solid(kFace), kInk, colors::white};
    s.states[1] = {solid(kFocus), kDarkInk, colors::white};
    s.states[2] = {solid(kPress), kDarkInk, colors::white};
    s.states[3] = {solid(kOff), kOffInk, rgba(255, 255, 255, 128)};
    s.states[4] = s.states[0];
    s.padding[0] = 6;
    s.padding[1] = 3;
    s.padding[2] = 6;
    s.padding[3] = 3;
}

UiTheme make_default() {
    UiTheme t;
    UiStyle plain;
    for (UiLook& l : plain.states) l = {UiBackground{}, kInk, colors::white};
    plain.states[3].text = kOffInk;
    std::memset(plain.padding, 0, sizeof plain.padding);
    t.label = plain;
    t.image = plain;
    t.scroll_box = plain;
    t.panel = plain;
    for (UiLook& l : t.panel.states) l.background = solid(rgb(24, 28, 44), true, kEdge);
    for (std::uint8_t& p : t.panel.padding) p = 6;
    t.progress = plain;
    for (UiLook& l : t.progress.states) l.background = solid(rgb(30, 34, 50), true, kEdge);
    for (std::uint8_t& p : t.progress.padding) p = 1;
    t.progress.progress.fill = solid(rgb(84, 196, 120));
    control(t.button);
    control(t.toggle);
    control(t.choice);
    control(t.slider);
    t.slider.slider.track = solid(rgb(90, 100, 140));
    t.slider.slider.fill = solid(rgb(120, 180, 255));
    t.slider.slider.knob = solid(colors::white);
    t.list = plain;
    for (UiLook& l : t.list.states) l.background = solid(rgb(28, 32, 50));
    t.list.states[1].text = kDarkInk; // the focused row's text (on the focus colour)
    t.list.states[3].text = kOffInk;
    t.list.list.row[1] = solid(kFocus);
    t.list.list.track = solid(rgb(40, 44, 64));
    t.list.list.thumb = solid(rgb(150, 160, 200));
    t.grid = t.list;
    t.cursor = UiCursorStyle{};
    return t;
}

} // namespace

const UiTheme& ui::default_theme() {
    static const UiTheme theme = make_default();
    return theme;
}

namespace detail {

const UiStyle& theme_style(const UiTheme& t, UiKind kind) {
    switch (kind) {
    case UiKind::Label: return t.label;
    case UiKind::Image: return t.image;
    case UiKind::Progress: return t.progress;
    case UiKind::Button: return t.button;
    case UiKind::Toggle: return t.toggle;
    case UiKind::Slider: return t.slider;
    case UiKind::Choice: return t.choice;
    case UiKind::List: return t.list;
    case UiKind::Grid: return t.grid;
    case UiKind::ScrollBox: return t.scroll_box;
    case UiKind::Panel: return t.panel;
    default: return t.label; // boxes, stacks, spacers, roots: no look of their own but padding 0
    }
}

const UiStyle& style_of(const UiBlock& b, int s) {
    const int id = b.hot[s].style;
    if (id > 0 && id <= b.styles_used) return b.styles[id - 1];
    return theme_style(*b.theme, kind_of(b, s));
}

/// True when two styles lay out alike (text scale and font, padding, list row metrics, slider knob, choice arrows).
bool same_metrics(const UiStyle& a, const UiStyle& b) {
    return a.text.scale == b.text.scale && a.text.font == b.text.font &&
           std::memcmp(a.padding, b.padding, sizeof a.padding) == 0 && a.list.row_height == b.list.row_height &&
           a.list.scroll_width == b.list.scroll_width && a.choice.text_arrows == b.choice.text_arrows &&
           a.choice.left.region.w == b.choice.left.region.w && a.choice.right.region.w == b.choice.right.region.w &&
           a.choice.left.texture == b.choice.left.texture && a.choice.right.texture == b.choice.right.texture;
}

bool theme_same_metrics(const UiTheme& a, const UiTheme& b) {
    const UiStyle* x[11] = {&a.label, &a.image, &a.progress, &a.panel, &a.button, &a.toggle,
                            &a.slider, &a.choice, &a.list, &a.grid, &a.scroll_box};
    const UiStyle* y[11] = {&b.label, &b.image, &b.progress, &b.panel, &b.button, &b.toggle,
                            &b.slider, &b.choice, &b.list, &b.grid, &b.scroll_box};
    for (int i = 0; i < 11; ++i)
        if (!same_metrics(*x[i], *y[i])) return false;
    return true;
}

} // namespace detail

void Ui::set_theme(const UiTheme& theme) {
    if (block_ == nullptr) return;
    if (!detail::theme_same_metrics(*block_->theme, theme)) block_->dirty[0] = block_->dirty[1] = true;
    block_->theme = &theme;
}

UiStyleId Ui::add_style(const UiStyle& style) {
    if (block_ == nullptr) return {};
    if (block_->styles_used >= block_->styles_capacity) {
        NNH_UI_MISUSE(*block_, "Ui::add_style: the styles are full (UiDesc::styles)");
        return {};
    }
    block_->styles[block_->styles_used++] = style;
    return UiStyleId{static_cast<std::uint8_t>(block_->styles_used)};
}

void Ui::set_style_value(UiStyleId id, const UiStyle& style) {
    if (block_ == nullptr) return;
    if (id.value == 0 || id.value > block_->styles_used) {
        NNH_UI_MISUSE(*block_, "Ui::set_style_value: an unknown style");
        return;
    }
    UiStyle& s = block_->styles[id.value - 1];
    if (!detail::same_metrics(s, style)) block_->dirty[0] = block_->dirty[1] = true;
    s = style;
}

void Ui::set_texture(int slot, const Texture& texture) {
    if (block_ == nullptr) return;
    if (slot < 0 || slot >= kMaxUiTextures) {
        NNH_UI_MISUSE(*block_, "Ui::set_texture: a slot outside 0..7");
        return;
    }
    block_->textures[slot] = &texture;
}

void Ui::set_layer(Screen screen, int layer) {
    if (block_ == nullptr) return;
    if (layer < 0 || layer > 15) {
        NNH_UI_MISUSE(*block_, "Ui::set_layer: a layer outside 0..15");
        return;
    }
    block_->layers[detail::screen_index(screen)] = static_cast<std::uint8_t>(layer);
}

void Ui::set_root_visible(Screen screen, bool on) {
    if (block_ == nullptr) return;
    block_->root_visible[detail::screen_index(screen)] = on;
}

} // namespace nnh
