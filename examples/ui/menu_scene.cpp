#include "menu_scene.hpp"

#include <nnh/app.hpp>
#include <nnh/input.hpp>

#include "hud.hpp"
#include "names.hpp"
#include "texts.hpp"

bool MenuScene::make_ui(const nnh::UiDesc& desc) {
    auto r = nnh::Ui::create(desc);
    if (!r) {
        NNH_LOG_ERROR(example, "ui: no memory for the %s screen: %s", screen_, r.error().message);
        return false;
    }
    ui_ = static_cast<nnh::Ui&&>(r.value());
    apply_look(ui_, demo_);
    theme_seen_ = demo_.theme;
    ui_.set_resolver(resolver_for(demo_.language));
    language_seen_ = demo_.language;
    ui_.bind(*this);
    return true;
}

void MenuScene::update() {
    // the UI was updated by its binding right before this: its results are this frame's
    const std::uint64_t now = nnh::frame_count();
    if (last_frame_ != 0 && now > last_frame_ + 1) {
        returned();
        if (ui_.focused()) NNH_LOG_INFO(example, "ui: focus %s", key_name(ui_.focused_key()));
    }
    last_frame_ = now;
    log_results(ui_, screen_);
    if (nnh::input::pressed(nnh::Button::Select)) {
        demo_.theme ^= 1;
        NNH_LOG_INFO(example, "ui: theme %s", demo_.theme == 0 ? "nine-slice" : "default");
        apply_look(demo_.hud->ui(), demo_);
    }
    if (theme_seen_ != demo_.theme) {
        apply_look(ui_, demo_);
        theme_seen_ = demo_.theme;
    }
    if (language_seen_ != demo_.language) {
        ui_.set_resolver(resolver_for(demo_.language)); // every key re-resolved: the localization hook
        language_seen_ = demo_.language;
    }
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

void MenuScene::draw() {
    if (nnh::scenes::top() != this) return; // scenes below stay quiet
    ++demo_.frames;
    demo_.hud->step(demo_.frames);
    demo_.hud->draw(demo_.frames);
    paint();
    ui_.draw();
}
