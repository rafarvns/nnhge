#include "themes.hpp"

#include "demos.hpp"

namespace {

using nnh::UiBackground;
using nnh::UiBackgroundKind;

UiBackground slice(nnh::Region r, int inset) {
    UiBackground b;
    b.kind = UiBackgroundKind::NineSlice;
    b.texture = 0;
    b.region = r;
    b.left = b.top = b.right = b.bottom = static_cast<std::uint8_t>(inset);
    return b;
}

constexpr nnh::Color kInk = nnh::rgb(240, 242, 255);
constexpr nnh::Color kDark = nnh::rgb(30, 24, 10);

/// A framed control: normal, focused, pressed, disabled faces of the sheet.
void framed(nnh::UiStyle& s) {
    s.states[0] = {slice({24, 0, 16, 16}, 4), kInk, nnh::colors::white};
    s.states[1] = {slice({40, 0, 16, 16}, 4), kDark, nnh::colors::white};
    s.states[2] = {slice({56, 0, 16, 16}, 4), kDark, nnh::colors::white};
    s.states[3] = {slice({72, 0, 16, 16}, 4), nnh::rgb(150, 150, 165), nnh::rgba(255, 255, 255, 140)};
    s.states[4] = s.states[0];
    s.padding[0] = s.padding[2] = 7;
    s.padding[1] = s.padding[3] = 4;
    s.text.shadow = true;
    s.text.shadow_color = nnh::rgba(0, 0, 0, 120);
}

nnh::UiTheme make() {
    nnh::UiTheme t = nnh::ui::default_theme();
    for (nnh::UiLook& l : t.panel.states) l.background = slice({0, 0, 24, 24}, 6);
    for (std::uint8_t& p : t.panel.padding) p = 8;
    framed(t.button);
    framed(t.toggle);
    framed(t.slider);
    framed(t.choice);
    t.toggle.toggle.off = {.texture = 0, .region = {0, 24, 10, 10}};
    t.toggle.toggle.on = {.texture = 0, .region = {10, 24, 10, 10}};
    t.slider.slider.track = slice({88, 0, 16, 6}, 3);
    t.slider.slider.fill = slice({112, 0, 16, 6}, 3);
    t.slider.slider.knob = slice({104, 0, 6, 12}, 0);
    t.slider.slider.track_height = 6;
    t.choice.choice.left = {.texture = 0, .region = {28, 24, 8, 10}};
    t.choice.choice.right = {.texture = 0, .region = {20, 24, 8, 10}};
    for (nnh::UiLook& l : t.progress.states) l.background = slice({64, 24, 16, 8}, 2);
    t.progress.progress.fill = slice({80, 24, 16, 8}, 2);
    for (nnh::UiLook& l : t.list.states) l.background = slice({0, 0, 24, 24}, 6);
    for (std::uint8_t& p : t.list.padding) p = 4;
    t.list.list.row[1] = slice({96, 24, 16, 12}, 3);
    t.list.states[1].text = kDark;
    t.list.list.row_height = 14;
    t.grid = t.list;
    t.grid.list.row_height = 0;
    t.cursor.image = {.texture = 0, .region = {36, 24, 12, 10}};
    t.cursor.anchor = nnh::UiAnchor::Left;
    t.cursor.x = -1;
    t.cursor.pulse_frames = 40;
    t.cursor.pulse_opacity = 200;
    t.cursor.pulse_x = -3;
    return t;
}

} // namespace

const nnh::UiTheme& nine_slice_theme() {
    static const nnh::UiTheme theme = make();
    return theme;
}

const nnh::UiTheme& current_theme(const Demo& demo) {
    return demo.theme == 0 ? nine_slice_theme() : nnh::ui::default_theme();
}

void apply_look(nnh::Ui& ui, const Demo& demo) {
    if (demo.art != nullptr) ui.set_texture(0, *demo.art);
    ui.set_theme(current_theme(demo));
}
