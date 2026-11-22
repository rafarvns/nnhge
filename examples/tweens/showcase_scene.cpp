#include "showcase_scene.hpp"

#include "easing_gallery.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

using nnh::Button;

void ShowcaseScene::on_enter() {
    // Everything this scene started was cleared when it left the stack: start again.
    tweens_.from_to(&phase_, 0.0f, 1.0f, nnh::Duration::from_seconds(2), {.loops = nnh::kForever});
    intro_.start(tweens_);
    if (overlay_at_start_) {
        overlay_at_start_ = false;
        nnh::scenes::push(*demos_.overlay);
    }
}

void ShowcaseScene::update() {
    intro_.update();
    const nnh::TweenHandle intro = intro_.handle();
    if (nnh::input::pressed(Button::A)) {
        if (tweens_.paused(intro)) {
            tweens_.resume(intro);
        } else {
            tweens_.pause(intro);
        }
    }
    if (nnh::input::pressed(Button::B)) tweens_.complete(intro);
    if (nnh::input::pressed(Button::Y)) intro_.start(tweens_);
    if (nnh::input::pressed(Button::X)) nnh::scenes::push(*demos_.overlay);
    if (nnh::input::pressed(Button::Select)) nnh::scenes::replace(*demos_.check);
    if (nnh::input::pressed(Button::Start)) nnh::scenes::pop();
}

void ShowcaseScene::draw() {
    draw_easing_gallery(phase_);
    intro_.draw();
    const nnh::TweenHandle intro = intro_.handle();
    char status[64];
    std::snprintf(status, sizeof status, "intro: %s %3d%%  tweens: %d",
                  !tweens_.running(intro) ? "done" : (tweens_.paused(intro) ? "paused" : "playing"),
                  static_cast<int>(tweens_.progress(intro) * 100 + 0.5f), tweens_.active());
    const nnh::Screen bottom = nnh::Screen::Bottom;
    nnh::gfx::draw_text(bottom, {8, 6}, status);
    nnh::gfx::draw_text(bottom, {8, 200},
                        "A pause  B complete  Y restart\nX overlay  SELECT next  START quit",
                        {.color = nnh::rgb(170, 180, 200)});
}
