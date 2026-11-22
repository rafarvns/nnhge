#include "self_check_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

NNH_LOG_CHANNEL(anim);

void SelfCheckScene::on_enter() {
    check_.emplace();
    verdict_[0] = '\0';
    std::strcpy(last_line_, "animation: self-check started");
    NNH_LOG_INFO(anim, "animation: self-check started, %d animators, sizeof(SpriteAnimator) = %u",
                 AnimationSelfCheck::kAnimators, static_cast<unsigned>(sizeof(nnh::SpriteAnimator)));
}

void SelfCheckScene::update() {
    if (check_ && !check_->done() && check_->update()) {
        format_animation_checkpoint(last_line_, sizeof last_line_, check_->frame(), check_->checksum());
        NNH_LOG_INFO(anim, "%s", last_line_);
        if (check_->done()) {
            format_animation_verdict(verdict_, sizeof verdict_, check_->frame(), check_->checksum());
            if (std::strncmp(verdict_, "animation: PASS", 15) == 0) {
                NNH_LOG_INFO(anim, "%s", verdict_);
            } else {
                NNH_LOG_ERROR(anim, "%s", verdict_);
            }
        }
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.bench);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void SelfCheckScene::draw() {
    const nnh::Screen top = nnh::Screen::Top;
    nnh::gfx::draw_text(top, {200, 40}, "ANIMATION SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    nnh::gfx::draw_text(top, {200, 90}, last_line_, {.align = nnh::TextAlign::Center});
    if (verdict_[0] != '\0') {
        const bool pass = std::strncmp(verdict_, "animation: PASS", 15) == 0;
        nnh::gfx::draw_text(top, {200, 120}, verdict_,
                            {.color = pass ? nnh::colors::green : nnh::colors::red, .align = nnh::TextAlign::Center});
    }
    char info[64];
    std::snprintf(info, sizeof info, "\n\nnotices so far: %d", check_ ? check_->notices() : 0);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 8},
                        "600 frames of 64 animators: every\nmode, queues, speeds, states, markers,\ncallbacks that "
                        "change clips; a checksum\nof every frame and notice. The PC and\nthe 3DS must agree.\n\n"
                        "SELECT bench  START quit");
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 100}, info);
}
