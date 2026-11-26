#include <cstring>

#include "scenes.hpp"
#include "settings.hpp"

using nnh::UiText;

namespace {

enum class Key { Language, Back };

} // namespace

bool OptionsScene::create() {
    if (!make_ui({.widgets = 16, .text_bytes = 0, .texts = nnh::loc::kMaxLanguages + 1, .styles = 1, .functions = 2}))
        return false;
    const nnh::UiStyleId title = ui_.add_style(title_style());
    ui_.label(ui_.root(nnh::Screen::Top), {.text = UiText::name("options.title"), .style = title,
                                           .layout = {.place = {.anchor = nnh::UiAnchor::Center}}});
    // each shipped language by its own name (the manifest's text, alive while localization runs), the pseudo-language
    // only in Debug/Development builds (it is shipped only there)
    names_[0] = UiText::name("options.system");
    count_ = 1;
    for (int i = 0; i < nnh::loc::language_count(); ++i) names_[count_++] = UiText::borrow(nnh::loc::language_name(i));
    const nnh::UiWidget box = ui_.vbox(ui_.root(nnh::Screen::Bottom),
                                       {.spacing = 4, .layout = {.width = 280, .place = {.anchor = nnh::UiAnchor::Center}}});
    choice_ = ui_.choice(box, {.text = UiText::name("options.language"), .options = names_, .count = count_,
                               .key = Key::Language});
    const nnh::UiWidget back = ui_.button(box, {.text = UiText::name("options.back"), .key = Key::Back});
    ui_.set_back(box, back);
    return true;
}

void OptionsScene::on_enter() {
    LocScene::on_enter();
    ui_.focus(choice_);
}

void OptionsScene::language_changed() {
    // the choice shows "System language" while the game follows it, else the current language
    ui_.set_value(choice_, demo_.following ? 0 : nnh::loc::index() + 1);
}

void OptionsScene::step() {
    if (ui_.changed(Key::Language)) {
        const int v = ui_.value(Key::Language);
        demo_.following = v == 0;
        if (v == 0) {
            nnh::loc::follow_system(true);
            NNH_LOG_INFO(example, "loc: follow system");
        } else {
            (void)nnh::loc::set_language(nnh::loc::language_code(v - 1));
        }
        store_language_choice(v == 0 ? "" : nnh::loc::language_code(v - 1)); // the choice survives a restart
    }
    if (ui_.activated(Key::Back)) nnh::scenes::pop();
}
