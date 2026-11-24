#include "check_scene.hpp"

#include <cstdio>
#include <new>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace pd = pathfinding_demo;

void CheckScene::on_enter() {
    check_.reset(new (std::nothrow) pd::SelfCheck());
    if (check_ && !check_->init()) check_.reset();
    verdict_[0] = '\0';
    failed_ = false;
    linger_ = 0;
    std::snprintf(line_, sizeof line_, "pathfinding: check started");
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "pathfinding: check started");
    if (!check_) NNH_LOG_ERROR(example, "pathfinding: FAIL no memory for the self-check");
}

void CheckScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) {
        nnh::scenes::pop();
        return;
    }
    if (!check_ || failed_) return;
    if (check_->done()) { // the verdict in the frame after the last checkpoint (each line expectable on its own)
        if (verdict_[0] == '\0') {
            std::snprintf(verdict_, sizeof verdict_, "pathfinding: PASS %08X",
                          static_cast<unsigned>(check_->checksum()));
            NNH_LOG_INFO(example, "%s", verdict_);
        }
        if (++linger_ == 30) nnh::scenes::replace(*demos_.showcase); // a short pause on it, then the showcase
        return;
    }
    const int before = check_->searches();
    check_->run(kPerFrame);
    const int n = check_->searches();
    if (n / pd::kCheckpointEvery != before / pd::kCheckpointEvery) {
        const int k = n / pd::kCheckpointEvery - 1;
        const std::uint32_t got = check_->checkpoint(k);
        std::snprintf(line_, sizeof line_, "pathfinding: %d sum %08X", n, static_cast<unsigned>(got));
        NNH_LOG_INFO(example, "%s", line_);
        if (got != pd::kPathfindingCheckpoints[k]) {
            failed_ = true;
            std::snprintf(verdict_, sizeof verdict_, "pathfinding: FAIL search %d expected %08X got %08X", n,
                          static_cast<unsigned>(pd::kPathfindingCheckpoints[k]), static_cast<unsigned>(got));
            NNH_LOG_ERROR(example, "%s", verdict_);
            return;
        }
    }
}

void CheckScene::draw() {
    namespace gfx = nnh::gfx;
    gfx::draw_text(nnh::Screen::Top, {200, 40}, "PATHFINDING SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    gfx::draw_text(nnh::Screen::Top, {200, 90}, line_, {.align = nnh::TextAlign::Center});
    if (verdict_[0] != '\0') {
        gfx::draw_text(nnh::Screen::Top, {200, 120}, verdict_,
                       {.color = failed_ ? nnh::colors::red : nnh::colors::green, .align = nnh::TextAlign::Center});
    }
    if (check_) {
        const float done = static_cast<float>(check_->searches()) / pd::kCheckSearches;
        gfx::draw_rect(nnh::Screen::Top, {40, 160, 320, 12}, nnh::colors::gray);
        gfx::fill_rect(nnh::Screen::Top, {41, 161, 318 * done, 10}, nnh::colors::yellow);
    }
    gfx::draw_text(nnh::Screen::Bottom, {8, 8},
                   "512 searches on four seeded maps\n(every diagonal mode, limits, partial\npaths, slices, doors), "
                   "64 smoothings\nand 16 distance fields. A checksum\nof every result's bits: the PC and\nthe 3DS "
                   "must agree.\n\nSTART quit");
}
