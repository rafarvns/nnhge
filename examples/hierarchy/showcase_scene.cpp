#include "showcase_scene.hpp"

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

void ShowcaseScene::on_enter() {
    if (!started_) {
        started_ = true;
        ready_ = yard_.start();
        if (!ready_) NNH_LOG_ERROR(example, "yard: out of memory or art missing");
    }
    if (ready_) yard_.restart();
}

void ShowcaseScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::replace(*demos_.check);
        return;
    }
    if (nnh::input::pressed(nnh::Button::Y)) {
        nnh::scenes::replace(*demos_.bench);
        return;
    }
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
    if (ready_) yard_.update();
}

void ShowcaseScene::draw() {
    if (ready_) yard_.draw();
}
