#include "self_check_scene.hpp"

#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

NNH_LOG_CHANNEL(tweens);

void SelfCheckScene::on_enter() {
    check_.emplace();
    rounding_ok_ = rounding_probe_ok();
    verdict_[0] = '\0';
    std::strcpy(last_line_, "tweens: self-check started");
    NNH_LOG_INFO(tweens, "tweens: self-check started, rounding probe %s", rounding_ok_ ? "ok" : "WRONG");
}

void SelfCheckScene::update() {
    if (check_ && !check_->done() && check_->update()) {
        format_checkpoint(last_line_, sizeof last_line_, check_->frame(), check_->checksum());
        NNH_LOG_INFO(tweens, "%s", last_line_);
        if (check_->done()) {
            format_verdict(verdict_, sizeof verdict_, check_->checksum(), rounding_ok_);
            if (std::strncmp(verdict_, "tweens: PASS", 12) == 0) {
                NNH_LOG_INFO(tweens, "%s", verdict_);
            } else {
                NNH_LOG_ERROR(tweens, "%s", verdict_);
            }
        }
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.bench);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void SelfCheckScene::draw() {
    const nnh::Screen top = nnh::Screen::Top;
    nnh::gfx::draw_text(top, {200, 40}, "TWEENS SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    nnh::gfx::draw_text(top, {200, 90}, last_line_, {.align = nnh::TextAlign::Center});
    if (verdict_[0] != '\0') {
        const bool pass = std::strncmp(verdict_, "tweens: PASS", 12) == 0;
        nnh::gfx::draw_text(
            top, {200, 120}, verdict_,
            {.color = pass ? nnh::colors::green : nnh::colors::red, .align = nnh::TextAlign::Center});
    }
    nnh::gfx::draw_text(
        nnh::Screen::Bottom, {8, 8},
        "600 frames of tweens, sequences,\nloops, yoyos and timers; a checksum\nof every value and "
        "callback.\nThe PC and the 3DS must agree.\n\nSELECT bench  START quit");
}
