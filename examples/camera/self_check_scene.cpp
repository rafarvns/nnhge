#include "self_check_scene.hpp"

#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

void SelfCheckScene::on_enter() {
    check_.emplace();
    verdict_[0] = '\0';
    std::strcpy(last_line_, "camera: self-check started");
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "camera: self-check started");
}

void SelfCheckScene::update() {
    if (check_ && !check_->done() && check_->update()) {
        format_camera_checkpoint(last_line_, sizeof last_line_, check_->frame(), check_->checksum());
        NNH_LOG_INFO(example, "%s", last_line_);
        if (check_->done()) {
            format_camera_verdict(verdict_, sizeof verdict_, check_->checksum());
            if (std::strncmp(verdict_, "camera: PASS", 12) == 0) {
                NNH_LOG_INFO(example, "%s", verdict_);
            } else {
                NNH_LOG_ERROR(example, "%s", verdict_);
            }
        }
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.bench);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void SelfCheckScene::draw() {
    namespace gfx = nnh::gfx;
    const nnh::Screen top = nnh::Screen::Top;
    gfx::draw_text(top, {200, 40}, "CAMERA SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    gfx::draw_text(top, {200, 90}, last_line_, {.align = nnh::TextAlign::Center});
    if (verdict_[0] != '\0') {
        const bool pass = std::strncmp(verdict_, "camera: PASS", 12) == 0;
        gfx::draw_text(
            top, {200, 120}, verdict_,
            {.color = pass ? nnh::colors::green : nnh::colors::red, .align = nnh::TextAlign::Center});
    }
    if (check_) {
        // The rig's view as a small picture: where its primary screen looks in the 1280 × 768 world.
        const nnh::Vec2 p = check_->rig().position();
        gfx::draw_rect(top, {40, 150, 320, 80}, nnh::colors::gray);
        gfx::fill_rect(top, {40 + p.x / 4, 150 + p.y / 9.6f, 6, 6}, nnh::colors::yellow);
    }
    gfx::draw_text(
        nnh::Screen::Bottom, {8, 8},
        "600 updates of a camera rig: a target\nthat runs, stops and jumps, a dead\nzone, smoothing, "
        "limits changed\nmid-way, trauma, zoom changes and\nthe tall-world layout. A checksum of\n"
        "every position's bits: the PC and\nthe 3DS must agree.\n\nSELECT bench  START quit");
}
