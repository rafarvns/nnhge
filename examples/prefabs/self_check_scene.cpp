#include "self_check_scene.hpp"

#include <cstring>
#include <new>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

void SelfCheckScene::on_enter() {
    check_.reset(new (std::nothrow) PrefabsSelfCheck());
    if (check_ && !check_->start()) check_.reset();
    verdict_[0] = '\0';
    std::strcpy(last_line_, "prefabs: self-check started");
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "prefabs: self-check started");
}

void SelfCheckScene::update() {
    bool logged = false;
    for (int i = 0; check_ && !check_->done() && i < kPerFrame; ++i) {
        if (check_->step()) {
            format_prefabs_checkpoint(last_line_, sizeof last_line_, check_->operations(), check_->checksum());
            NNH_LOG_INFO(example, "%s", last_line_);
            logged = true;
            break; // one line per frame: each can be expected on its own
        }
    }
    if (check_ && check_->done() && verdict_[0] == '\0' && !logged) { // the frame after the last checkpoint
        format_prefabs_verdict(verdict_, sizeof verdict_, *check_);
        if (std::strncmp(verdict_, "prefabs: PASS", 13) == 0) {
            NNH_LOG_INFO(example, "%s", verdict_);
        } else {
            NNH_LOG_ERROR(example, "%s", verdict_);
        }
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.showcase);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void SelfCheckScene::draw() {
    namespace gfx = nnh::gfx;
    const nnh::Screen top = nnh::Screen::Top;
    gfx::draw_text(top, {200, 40}, "PREFABS SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    gfx::draw_text(top, {200, 90}, last_line_, {.align = nnh::TextAlign::Center});
    if (verdict_[0] != '\0') {
        const bool pass = std::strncmp(verdict_, "prefabs: PASS", 13) == 0;
        gfx::draw_text(top, {200, 120}, verdict_,
                       {.color = pass ? nnh::colors::green : nnh::colors::red, .align = nnh::TextAlign::Center});
    }
    if (check_) {
        const float done = static_cast<float>(check_->operations()) / PrefabsSelfCheck::kOperations;
        gfx::draw_rect(top, {40, 160, 320, 12}, nnh::colors::gray);
        gfx::fill_rect(top, {41, 161, 318 * done, 10}, nnh::colors::yellow);
    }
    gfx::draw_text(nnh::Screen::Bottom, {8, 8},
                   "50 000 operations on two worlds of\n1024 entities: one by prefab spawns\n(variants, hooks, "
                   "overrides,\npositions), one by hand-written\nspawn/add/add_tag. They must agree\nafter every step, "
                   "and the checksum\nmust be the PC's on the 3DS.\n\nSELECT showcase  START quit");
}
