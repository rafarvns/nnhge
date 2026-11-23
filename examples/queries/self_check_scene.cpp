#include "self_check_scene.hpp"

#include <cstring>
#include <new>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

void SelfCheckScene::on_enter() {
    check_.reset(new (std::nothrow) QueriesSelfCheck());
    verdict_[0] = '\0';
    std::strcpy(last_line_, "queries: self-check started");
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "queries: self-check started");
}

void SelfCheckScene::update() {
    for (int i = 0; check_ && !check_->done() && i < kPerFrame; ++i) {
        if (check_->step()) {
            format_queries_checkpoint(last_line_, sizeof last_line_, check_->rays(), check_->checksum());
            NNH_LOG_INFO(example, "%s", last_line_);
        }
        if (check_->done()) {
            format_queries_verdict(verdict_, sizeof verdict_, check_->checksum());
            if (std::strncmp(verdict_, "queries: PASS", 13) == 0) {
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
    gfx::draw_text(top, {200, 40}, "QUERIES SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    gfx::draw_text(top, {200, 90}, last_line_, {.align = nnh::TextAlign::Center});
    if (verdict_[0] != '\0') {
        const bool pass = std::strncmp(verdict_, "queries: PASS", 13) == 0;
        gfx::draw_text(top, {200, 120}, verdict_,
                       {.color = pass ? nnh::colors::green : nnh::colors::red, .align = nnh::TextAlign::Center});
    }
    if (check_) {
        const float done = static_cast<float>(check_->rays() + check_->casts()) /
                           static_cast<float>(QueriesSelfCheck::kRays + QueriesSelfCheck::kCasts);
        gfx::draw_rect(top, {40, 160, 320, 12}, nnh::colors::gray);
        gfx::fill_rect(top, {41, 161, 318 * done, 10}, nnh::colors::yellow);
    }
    gfx::draw_text(nnh::Screen::Bottom, {8, 8},
                   "4096 rays and 512 box casts on\na grid and 64 boxes, with layer\nmasks, ignored boxes, starts "
                   "inside\nand one-way cells. A checksum of\nevery hit's bits: the PC and the\n3DS must agree.\n\n"
                   "SELECT bench  START quit");
}
