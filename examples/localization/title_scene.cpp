#include <nnh/app.hpp>

#include "scenes.hpp"

using nnh::UiText;

namespace {

enum class Key { NewGame, Continue, Shop, Dialog, Fonts, Options, Quit };

const char* const kNames[] = {"new_game", "continue", "shop", "dialog", "fonts", "options", "quit"};

} // namespace

bool TitleScene::create() {
    if (!make_ui({.widgets = 24, .text_bytes = 0, .styles = 1, .functions = 2})) return false;
    const nnh::UiStyleId title = ui_.add_style(title_style());
    const nnh::UiWidget top = ui_.root(nnh::Screen::Top);
    ui_.label(top, {.text = UiText::name("game.title"), .style = title,
                    .layout = {.place = {.anchor = nnh::UiAnchor::Center, .y = -16}}});
    ui_.label(top, {.text = UiText::name("game.hint"), .layout = {.place = {.anchor = nnh::UiAnchor::Bottom, .y = -24}}});
    const nnh::UiWidget menu = ui_.vbox(ui_.root(nnh::Screen::Bottom),
                                        {.spacing = 2, .layout = {.width = 200, .place = {.anchor = nnh::UiAnchor::Center}}});
    ui_.button(menu, {.text = UiText::name("menu.new_game"), .key = Key::NewGame});
    const nnh::UiWidget cont = ui_.button(menu, {.text = UiText::name("menu.continue"), .key = Key::Continue});
    ui_.set_enabled(cont, false); // no save: skipped by the focus
    ui_.button(menu, {.text = UiText::name("menu.shop"), .key = Key::Shop});
    ui_.button(menu, {.text = UiText::name("menu.dialog"), .key = Key::Dialog});
    ui_.button(menu, {.text = UiText::name("menu.fonts"), .key = Key::Fonts});
    ui_.button(menu, {.text = UiText::name("menu.options"), .key = Key::Options});
    ui_.button(menu, {.text = UiText::name("menu.quit"), .key = Key::Quit});
    return true;
}

void TitleScene::step() {
    if (!announced_) { // in the first frame (a script's window starts at frame 1)
        announced_ = true;
        NNH_LOG_INFO(example, "loc: showcase");
    }
    const std::uint64_t now = nnh::frame_count();
    if (last_frame_ != 0 && now > last_frame_ + 1 && !ui_.focused_key().none()) { // back from a scene above
        NNH_LOG_INFO(example, "loc: focus %s", kNames[ui_.focused_key().value]);
    }
    last_frame_ = now;
    for (const nnh::UiResult& r : ui_.results()) {
        if (r.kind == nnh::UiResultKind::FocusChanged && !r.key.none()) {
            NNH_LOG_INFO(example, "loc: focus %s", kNames[r.key.value]);
        }
    }
    nnh::Scene* next = nullptr;
    if (ui_.activated(Key::NewGame) || ui_.activated(Key::Dialog)) next = demo_.dialog;
    if (ui_.activated(Key::Shop)) next = demo_.shop;
    if (ui_.activated(Key::Fonts)) next = demo_.gallery;
    if (ui_.activated(Key::Options)) next = demo_.options;
    if (next != nullptr) nnh::scenes::push(*next);
    if (ui_.activated(Key::Quit)) nnh::scenes::pop(); // the last scene: the program ends
}
