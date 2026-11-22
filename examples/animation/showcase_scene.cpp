#include "showcase_scene.hpp"

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

void ShowcaseScene::on_enter() {
    gallery_.load();
    hero_ok_ = hero_.load();
}

void ShowcaseScene::on_exit() {
    hero_.unload();
    gallery_.unload();
}

void ShowcaseScene::update() {
    gallery_.update();
    if (hero_ok_) hero_.update();
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.check);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void ShowcaseScene::draw() {
    gallery_.draw();
    if (hero_ok_) {
        hero_.draw();
    } else {
        nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 8}, "art/hero failed to load (see the log)",
                            {.color = nnh::colors::red});
    }
}
