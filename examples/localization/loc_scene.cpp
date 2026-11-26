#include "loc_scene.hpp"

#include <nnh/input.hpp>

#include "hud.hpp"

bool LocScene::make_ui(const nnh::UiDesc& desc) {
    auto r = nnh::Ui::create(desc);
    if (!r) {
        NNH_LOG_ERROR(example, "loc: no memory for the %s screen: %s", name_, r.error().message);
        return false;
    }
    ui_ = static_cast<nnh::Ui&&>(r.value());
    ui_.set_theme(example_theme());
    ui_.bind(*this);
    return true;
}

void LocScene::on_enter() {
    nnh::loc::apply(ui_, kRoles); // the language may have changed while another scene was on top
    language_ = nnh::events::subscribe<&LocScene::on_language>(this);
    language_changed();
}

void LocScene::on_exit() { language_.reset(); }

void LocScene::on_language(const nnh::LanguageChanged&) {
    nnh::loc::apply(ui_, kRoles); // in the frame the switch took effect: the new texts and fonts are drawn now
    language_changed();
}

void LocScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) { // every scene popped: run() returns
        for (int n = nnh::scenes::size(); n > 0; --n) nnh::scenes::pop();
        return;
    }
    if (nnh::input::pressed(nnh::Button::Select)) next_language();
    if (nnh::input::pressed(nnh::Button::Y) && demo_.check != nullptr) {
        nnh::scenes::push(*demo_.check);
        return;
    }
    if (nnh::input::pressed(nnh::Button::X) && demo_.bench != nullptr) {
        nnh::scenes::push(*demo_.bench);
        return;
    }
    step();
}

void LocScene::draw() {
    if (nnh::scenes::top() != this) return;
    ++demo_.frames;
    demo_.hud->draw(demo_.frames);
    paint();
    ui_.draw();
}
