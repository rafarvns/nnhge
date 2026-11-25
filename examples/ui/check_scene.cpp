#include "check_scene.hpp"

#include <cstdio>
#include <new>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace uc = ui_check;

void CheckScene::on_enter() {
    check_.reset(new (std::nothrow) uc::SelfCheck());
    if (check_ && !check_->begin()) check_.reset();
    verdict_[0] = '\0';
    failed_ = false;
    linger_ = 0;
    std::snprintf(line_, sizeof line_, "ui: check started");
    NNH_LOG_INFO(example, "ui: check started");
    if (!check_) NNH_LOG_ERROR(example, "ui: FAIL no memory for the self-check");
}

void CheckScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::pop();
        return;
    }
    if (!check_ || failed_) return;
    if (check_->done()) {
        if (verdict_[0] == '\0') {
            std::snprintf(verdict_, sizeof verdict_, "ui: PASS %08X", static_cast<unsigned>(check_->checksum()));
            NNH_LOG_INFO(example, "%s", verdict_);
        }
        if (++linger_ == 40) nnh::scenes::pop();
        return;
    }
    for (int i = 0; i < kPerFrame && !check_->done(); ++i) {
        if (!check_->step()) continue;
        const int k = check_->frame() / uc::kCheckpointEvery - 1;
        std::snprintf(line_, sizeof line_, "ui: %d sum %08X", check_->frame(), static_cast<unsigned>(check_->checksum()));
        NNH_LOG_INFO(example, "%s", line_);
        if (check_->checksum() != uc::kUiCheckpoints[k]) {
            failed_ = true;
            std::snprintf(verdict_, sizeof verdict_, "ui: FAIL frame %d expected %08X got %08X", check_->frame(),
                          static_cast<unsigned>(uc::kUiCheckpoints[k]), static_cast<unsigned>(check_->checksum()));
            NNH_LOG_ERROR(example, "%s", verdict_);
            return;
        }
    }
}

void CheckScene::draw() {
    namespace gfx = nnh::gfx;
    // the self-check's UI draws itself inside its steps (once per display frame); the scene adds its status
    gfx::draw_text(nnh::Screen::Top, {200, 200}, line_, {.align = nnh::TextAlign::Center, .shadow = true, .layer = 15});
    if (verdict_[0] != '\0') {
        gfx::draw_text(nnh::Screen::Top, {200, 214}, verdict_,
                       {.color = failed_ ? nnh::colors::red : nnh::colors::green, .align = nnh::TextAlign::Center,
                        .shadow = true, .layer = 15});
    }
    (void)demo_;
}
