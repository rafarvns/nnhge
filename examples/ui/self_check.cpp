// The UI self-check (self_check.hpp): its UI, the screens switched by results (the example's own pure logic), the
// events at frames 2000, 2400 and 3000, and the checksums.

#include "self_check.hpp"

#include <cstdio>

namespace ui_check {

// Recorded from pc-debug (WinLibs GCC) on 2026-11-25; pc-release, GCC 13/14, clang 18, the 32-bit host build and the
// console must give the same.
const std::uint32_t kUiCheckpoints[kCheckpoints] = {0x89D4068Fu, 0xD85440B0u, 0x7C85E5EAu,
                                                    0x907D59A4u, 0x49E1D0E0u, 0x30400DA6u};
const std::uint32_t kUiCheckGolden = 0x30400DA6u;

namespace {

enum K : int { New = 1, Cont, Opt, Lst, Grd, Extras, Quit, Info, SliderA, SliderB, Tog, ChoiceW, ChoiceN, Off, Back,
               ListK, GridK, Yes, No };

// Two languages for the title's keyed texts (the switch at 2400 re-resolves them).
const char* const kTitles[2][7] = {{"New game", "Continue", "Options", "List", "Grid", "Extras", "Quit"},
                                   {"Nouveau", "Continuer", "Options", "Liste", "Grille", "Bonus", "Quitter"}};
int g_language = 0;
const char* resolve(std::uint32_t key) { return key < 7 ? kTitles[g_language][key] : nullptr; }

const nnh::UiText kChoices3[3] = {"Easy", "Normal", "Hard"};
const nnh::UiText kChoices4[4] = {"Off", "Low", "Mid", "High"};

void fill(int i, nnh::UiRow& row) {
    row.text = (i % 3) == 0 ? "Potion" : ((i % 3) == 1 ? "Ether" : "Herb");
    row.number = i * 7 % 99;
    row.enabled = i % 7 != 3;
}
void fill_grid(int i, nnh::UiRow& row) {
    row.text = (i % 2) == 0 ? "o" : "x";
    row.enabled = i % 11 != 5;
}
/// "75%": a formatter (FR-25).
int percent(int v, char* out, int capacity) {
    char text[16];
    const int n = std::snprintf(text, sizeof text, "%d%%", v);
    int k = 0;
    for (; k < n && k < capacity; ++k) out[k] = text[k];
    return k;
}

} // namespace

void SelfCheck::fold(std::uint32_t word) {
    for (int i = 0; i < 4; ++i) {
        hash_ ^= (word >> (8 * i)) & 0xFFu;
        hash_ *= 16777619u;
    }
}

nnh::UiWidget SelfCheck::keep(nnh::UiWidget w) {
    if (made_count_ < kMade) made_[made_count_++] = w;
    return w;
}

void SelfCheck::build_title() {
    const nnh::UiWidget bottom = ui_.root(nnh::Screen::Bottom);
    title_ = keep(ui_.vbox(bottom, {.spacing = 2, .layout = {.width = 150, .place = {.anchor = nnh::UiAnchor::Center}}}));
    const int keys[7] = {New, Cont, Opt, Lst, Grd, Extras, Quit};
    for (int i = 0; i < 7; ++i) {
        nnh::UiWidget parent = title_;
        if (keys[i] == Extras) parent = keep(ui_.vbox(title_, {.spacing = 0, .disabled_focusable = true}));
        const nnh::UiWidget b = keep(ui_.button(parent, {.text = nnh::UiText::key(static_cast<std::uint32_t>(i)),
                                                         .key = nnh::UiKey(keys[i])}));
        if (keys[i] == Cont || keys[i] == Extras) ui_.set_enabled(b, false);
        if (i == 0) {
            ui_.set_neighbour(b, nnh::UiDir::Up, info_);
            ui_.set_neighbour(info_, nnh::UiDir::Down, b);
        }
    }
}

void SelfCheck::build_rest() {
    const nnh::UiWidget top = ui_.root(nnh::Screen::Top);
    const nnh::UiWidget bottom = ui_.root(nnh::Screen::Bottom);
    keep(top);
    keep(bottom);
    health_ = keep(ui_.progress(top, {.value = 70, .max = 100, .show_text = true,
                                      .layout = {.width = 120, .height = 12, .place = {.x = 4, .y = 4}}}));
    score_ = keep(ui_.label(top, {.text = nnh::UiText::borrow(score_text_),
                                  .layout = {.place = {.anchor = nnh::UiAnchor::TopRight, .x = -4, .y = 4}}}));
    info_ = keep(ui_.button(top, {.text = "Info", .key = nnh::UiKey(Info),
                                  .layout = {.place = {.anchor = nnh::UiAnchor::Top, .y = 30}}}));
    options_ = keep(ui_.scroll_box(bottom, {.spacing = 2, .margin = 1,
                                            .layout = {.width = 280, .height = 150,
                                                       .place = {.anchor = nnh::UiAnchor::Center}}}));
    keep(ui_.slider(options_, {.text = "Music", .min = 0, .max = 10, .value = 7, .key = nnh::UiKey(SliderA)}));
    keep(ui_.slider(options_, {.text = "Sound", .min = 0, .max = 100, .step = 5, .value = 50, .format = &percent,
                               .key = nnh::UiKey(SliderB)}));
    keep(ui_.toggle(options_, {.text = "Invert", .key = nnh::UiKey(Tog)}));
    keep(ui_.choice(options_, {.text = "Level", .options = kChoices3, .count = 3, .index = 1, .key = nnh::UiKey(ChoiceW)}));
    keep(ui_.choice(options_, {.text = "Rumble", .options = kChoices4, .count = 4, .wrap = false,
                               .key = nnh::UiKey(ChoiceN)}));
    const nnh::UiWidget off = keep(ui_.button(options_, {.text = "Online", .key = nnh::UiKey(Off)}));
    ui_.set_enabled(off, false);
    for (int i = 0; i < 3; ++i) keep(ui_.toggle(options_, {.text = "More", .checked = i == 1}));
    const nnh::UiWidget back = keep(ui_.button(options_, {.text = "Back", .key = nnh::UiKey(Back)}));
    ui_.set_back(options_, back);
    list_panel_ = keep(ui_.panel(bottom, {.layout = {.place = {.anchor = nnh::UiAnchor::Center}}}));
    list_ = keep(ui_.list(list_panel_, {.rows = 12, .count = 1000, .fill = &fill, .margin = 2, .cell_width = 200,
                                        .key = nnh::UiKey(ListK)}));
    grid_ = keep(ui_.list(bottom, {.rows = 4, .columns = 6, .count = 64, .fill = &fill_grid, .tap_selects_first = true,
                                   .row_height = 30, .cell_width = 40, .key = nnh::UiKey(GridK),
                                   .layout = {.place = {.anchor = nnh::UiAnchor::Center}}}));
    dialog_ = keep(ui_.panel(bottom, {.layout = {.height = 60, .place = {.anchor = nnh::UiAnchor::Bottom, .x = 4,
                                                                         .y = -4, .fill_x = true, .right = 4}}}));
    dialog_text_ = keep(ui_.label(dialog_, {.text = "The gate opens at dawn: bring a lantern, twelve coins and a "
                                                    "well-made map of the north-east roads, or stay home.",
                                            .wrap = true, .max_lines = 4, .reveal_rate = 0.5f}));
}

bool SelfCheck::begin() {
    auto r = nnh::Ui::create({.widgets = 512, .text_bytes = 2048, .texts = 8, .rows = 64, .functions = 16});
    if (!r) return false;
    ui_ = static_cast<nnh::Ui&&>(r.value());
    g_language = 0;
    made_count_ = 0;
    ui_.set_resolver(&resolve);
    build_rest();
    build_title();
    show(0);
    frame_ = 0;
    hash_ = 2166136261u;
    return true;
}

void SelfCheck::show(int screen) {
    screen_ = screen;
    reached_ |= 1 << screen;
    ui_.set_visible(title_, screen == 0);
    ui_.set_visible(options_, screen == 1);
    ui_.set_visible(list_panel_, screen == 2);
    ui_.set_visible(grid_, screen == 3);
    ui_.set_visible(dialog_, screen == 4);
    const nnh::UiWidget target[5] = {title_, options_, list_, grid_, title_};
    ui_.focus(target[screen]);
    if (screen == 4) ui_.set_reveal(dialog_text_, 0);
}

void SelfCheck::react() {
    const nnh::UiInput in = script_input(frame_);
    for (const nnh::UiResult& r : ui_.results()) {
        if (r.kind == nnh::UiResultKind::Refused) reached_ |= 1 << 7;
        if (r.kind == nnh::UiResultKind::Blocked) reached_ |= 1 << 8;
        if (r.kind == nnh::UiResultKind::Page) reached_ |= 1 << 9;
        if (r.kind == nnh::UiResultKind::Cancelled) reached_ |= 1 << 10;
        if (r.kind == nnh::UiResultKind::ValueChanged && (in.touching || in.touch_ended)) reached_ |= 1 << 11;
        fold(static_cast<std::uint32_t>(r.kind) | (static_cast<std::uint32_t>(r.key.value) << 8));
        fold(r.widget.bits());
        fold(r.other.bits());
        fold(static_cast<std::uint32_t>(r.value));
        fold(static_cast<std::uint32_t>(r.index));
    }
    if (screen_ == 0 && ui_.activated(nnh::UiKey(New))) show(4);
    else if (screen_ == 0 && ui_.activated(nnh::UiKey(Opt))) show(1);
    else if (screen_ == 0 && ui_.activated(nnh::UiKey(Lst))) show(2);
    else if (screen_ == 0 && ui_.activated(nnh::UiKey(Grd))) show(3);
    else if (screen_ == 0 && ui_.activated(nnh::UiKey(Quit))) {
        reached_ |= 1 << 5;
        prompt_ = keep(ui_.prompt(nnh::Screen::Bottom, {.message = "Really quit?", .buttons = {"Yes", "No"},
                                                        .keys = {nnh::UiKey(Yes), nnh::UiKey(No)}, .default_button = 1}));
    } else if (ui_.activated(nnh::UiKey(Yes))) {
        reached_ |= 1 << 6;
        ++quits_;
    } else if (screen_ == 1 && ui_.activated(nnh::UiKey(Back))) {
        show(0);
    } else if ((screen_ == 2 || screen_ == 3) && ui_.cancelled()) {
        show(0);
    }
    if (screen_ == 4) {
        if ((in.pressed & nnh::ui_bit(nnh::UiControl::Confirm)) != 0) {
            if (ui_.fully_shown(dialog_text_)) show(0);
            else ui_.show_all(dialog_text_);
        }
    }
}

void SelfCheck::checkpoint() {
    // every widget the check made that is still alive, in the order made
    for (int i = 0; i < made_count_; ++i) {
        const nnh::UiWidget w = made_[i];
        if (!ui_.alive(w)) continue;
        const nnh::UiRect r = ui_.rect(w);
        fold(w.bits());
        fold(static_cast<std::uint32_t>(ui_.kind(w)) | (static_cast<std::uint32_t>(ui_.state(w)) << 8) |
             (ui_.visible(w) ? 0x10000u : 0u) | (ui_.enabled(w) ? 0x20000u : 0u));
        fold(static_cast<std::uint32_t>(r.x) ^ (static_cast<std::uint32_t>(r.y) << 16));
        fold(static_cast<std::uint32_t>(r.w) ^ (static_cast<std::uint32_t>(r.h) << 16));
        fold(static_cast<std::uint32_t>(ui_.value(w)));
    }
    fold(ui_.focused().bits());
    const nnh::UiListInfo l = ui_.list_info(list_), g = ui_.list_info(grid_);
    fold(static_cast<std::uint32_t>(l.first_visible) ^ (static_cast<std::uint32_t>(l.focused) << 16));
    fold(static_cast<std::uint32_t>(g.first_visible) ^ (static_cast<std::uint32_t>(g.focused) << 16));
    fold(static_cast<std::uint32_t>(quits_));
}

bool SelfCheck::step() {
    if (done()) return false;
    if (frame_ == 2000) ui_.set_item_count(list_, 600);
    if (frame_ == 2400) {
        g_language = 1;
        ui_.text_changed();
    }
    if (frame_ == 3000) { // rebuild the title, restoring its focus by key
        const nnh::UiKey kept = ui_.focused_key();
        ui_.destroy(title_);
        build_title();
        show(0);
        ui_.focus(kept);
    }
    if (frame_ % 100 == 0) {
        std::snprintf(score_text_, sizeof score_text_, "%d", frame_ * 3);
        ui_.mark_text_changed(score_);
        ui_.set_value(health_, 100 - (frame_ / 100) % 60);
    }
    ui_.update(script_input(frame_));
    react();
    if (frame_ % 10 == 9) {
        ui_.draw();
        fold(ui_.stats().draw_hash);
        fold(static_cast<std::uint32_t>(ui_.stats().draws));
    }
    ++frame_;
    if (frame_ % kCheckpointEvery != 0) return false;
    checkpoint();
    return true;
}

} // namespace ui_check
