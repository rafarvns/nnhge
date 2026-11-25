#include "title_scene.hpp"

#include "names.hpp"
#include "texts.hpp"

using nnh::UiText;

namespace {

UiText t(Text x) { return UiText::key(x); }

} // namespace

bool TitleScene::create() {
    if (!make_ui({.widgets = 40, .text_bytes = 64, .styles = 1, .functions = 2})) return false;
    nnh::UiStyle big = nnh::ui::default_theme().label;
    big.text.scale = 3;
    big.text.shadow = true;
    const nnh::UiStyleId title_style = ui_.add_style(big);
    const nnh::UiWidget top = ui_.root(nnh::Screen::Top);
    ui_.label(top, {.text = t(Text::GameName), .style = title_style,
                    .layout = {.place = {.anchor = nnh::UiAnchor::Center, .y = -10}}});
    ui_.label(top, {.text = t(Text::Hint), .layout = {.place = {.anchor = nnh::UiAnchor::Bottom, .y = -12}}});
    menu_ = ui_.vbox(ui_.root(nnh::Screen::Bottom),
                     {.spacing = 3, .layout = {.width = 160, .place = {.anchor = nnh::UiAnchor::Center}}});
    ui_.button(menu_, {.text = t(Text::NewGame), .key = Key::TitleNew});
    const nnh::UiWidget cont = ui_.button(menu_, {.text = t(Text::Continue), .key = Key::TitleContinue});
    ui_.set_enabled(cont, false); // no save yet: skipped by the focus
    ui_.button(menu_, {.text = t(Text::Options), .key = Key::TitleOptions});
    ui_.button(menu_, {.text = t(Text::Inventory), .key = Key::TitleInventory});
    ui_.button(menu_, {.text = t(Text::Gallery), .key = Key::TitleGallery});
    // a locked entry that explains itself: focusable but inert (refused)
    const nnh::UiWidget locked = ui_.vbox(menu_, {.spacing = 0, .disabled_focusable = true});
    const nnh::UiWidget extras = ui_.button(locked, {.text = t(Text::Extras), .key = Key::TitleExtras});
    ui_.set_enabled(extras, false);
    ui_.button(menu_, {.text = t(Text::Quit), .key = Key::TitleQuit});
    return true;
}

void TitleScene::on_enter() {
    NNH_LOG_INFO(example, "ui: showcase");
    if (shot_is("modal")) prompt_ = ui_.prompt(nnh::Screen::Bottom, {.message = t(Text::QuitAsk),
                                                                     .buttons = {t(Text::Yes), t(Text::No)},
                                                                     .keys = {Key::QuitYes, Key::QuitNo},
                                                                     .default_button = 1, .cancel_button = 1});
}

void TitleScene::step() {
    if (ui_.activated(Key::TitleNew) && demo_.dialog != nullptr) nnh::scenes::push(*demo_.dialog);
    if (ui_.activated(Key::TitleOptions) && demo_.options != nullptr) nnh::scenes::push(*demo_.options);
    if (ui_.activated(Key::TitleInventory) && demo_.inventory != nullptr) nnh::scenes::push(*demo_.inventory);
    if (ui_.activated(Key::TitleGallery) && demo_.gallery != nullptr) nnh::scenes::push(*demo_.gallery);
    if (ui_.activated(Key::TitleQuit)) {
        prompt_ = ui_.prompt(nnh::Screen::Bottom, {.message = t(Text::QuitAsk), .buttons = {t(Text::Yes), t(Text::No)},
                                                   .keys = {Key::QuitYes, Key::QuitNo}, .default_button = 1,
                                                   .cancel_button = 1});
    }
    if (ui_.activated(Key::QuitYes)) nnh::scenes::pop(); // the last scene: the program ends
}
