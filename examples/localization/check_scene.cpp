#include "check_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

#include "self_check.hpp"

namespace lc = loc_check;

void CheckScene::on_enter() {
    std::snprintf(previous_, sizeof previous_, "%s", nnh::loc::code());
    step_ = 0;
    linger_ = 0;
    hash_ = 2166136261u;
    failed_ = false;
    verdict_[0] = '\0';
    std::snprintf(line_, sizeof line_, "loc: check started");
    NNH_LOG_INFO(example, "loc: check started");
    (void)nnh::loc::set_language(lc::kCodes[0]); // takes effect at the start of the next frame
}

void CheckScene::update() {
    if (nnh::input::pressed(nnh::Button::Select) || linger_ > 0) {
        if (linger_ == 0 || ++linger_ > 40) {
            (void)nnh::loc::set_language(previous_);
            nnh::scenes::pop();
        }
        return;
    }
    if (step_ >= lc::kLanguages) { // the verdict a frame after the last sum (each line its own expect)
        if (failed_) {
            NNH_LOG_ERROR(example, "%s", verdict_);
        } else {
            std::snprintf(verdict_, sizeof verdict_, "loc: PASS %08X", static_cast<unsigned>(hash_));
            NNH_LOG_INFO(example, "%s", verdict_);
        }
        linger_ = 1;
        return;
    }
    const char* code = lc::kCodes[step_];
    if (std::strcmp(nnh::loc::code(), code) != 0) { // the switch failed: a damaged table or font
        failed_ = true;
        std::snprintf(verdict_, sizeof verdict_, "loc: FAIL language %s not switched (%s)", code, nnh::loc::code());
        NNH_LOG_ERROR(example, "%s", verdict_);
        step_ = lc::kLanguages;
        return;
    }
    hash_ = lc::language_sum(hash_);
    std::snprintf(line_, sizeof line_, "loc: %s sum %08X", code, static_cast<unsigned>(hash_));
    NNH_LOG_INFO(example, "%s", line_);
    if (hash_ != lc::kCheckpoints[step_] && !failed_) { // the first difference is the verdict; the rest still logged
        failed_ = true;
        std::snprintf(verdict_, sizeof verdict_, "loc: FAIL %s expected %08X got %08X", code,
                      static_cast<unsigned>(lc::kCheckpoints[step_]), static_cast<unsigned>(hash_));
    }
    if (++step_ < lc::kLanguages) (void)nnh::loc::set_language(lc::kCodes[step_]);
}

void CheckScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 100}, line_, {.align = nnh::TextAlign::Center, .shadow = true});
    if (verdict_[0] != '\0') {
        gfx::draw_text(nnh::Screen::Top, {200, 120}, verdict_,
                       {.color = failed_ ? nnh::colors::red : nnh::colors::green, .align = nnh::TextAlign::Center,
                        .shadow = true});
    }
    (void)demo_;
}
