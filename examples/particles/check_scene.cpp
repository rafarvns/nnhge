#include "check_scene.hpp"

#include <cstdio>
#include <new>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace pc = particles_check;

void CheckScene::on_enter() {
    check_.reset(new (std::nothrow) pc::SelfCheck());
    if (check_ && !check_->begin()) check_.reset();
    verdict_[0] = '\0';
    failed_ = false;
    linger_ = 0;
    std::snprintf(line_, sizeof line_, "particles: check started");
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    NNH_LOG_INFO(example, "particles: check started");
    if (!check_) NNH_LOG_ERROR(example, "particles: FAIL no memory for the self-check");
}

void CheckScene::update() {
    if (nnh::input::pressed(nnh::Button::Select) || nnh::input::pressed(nnh::Button::Start)) {
        nnh::scenes::replace(*demos_.showcase);
        return;
    }
    if (!check_ || failed_) return;
    if (check_->done()) { // each line in its own frame: the verdict, then the drawn frame's stats
        if (verdict_[0] == '\0') {
            std::snprintf(verdict_, sizeof verdict_, "particles: PASS %08X", static_cast<unsigned>(check_->checksum()));
            NNH_LOG_INFO(example, "%s", verdict_);
        } else if (linger_ == 2) { // the last frame's particles were drawn in the previous frame
            const nnh::gfx::Stats g = nnh::gfx::stats();
            NNH_LOG_INFO(example, "particles: stats drawn %d of %d dropped %d", g.particles_last_frame,
                         g.max_particles, g.dropped_particles);
        }
        if (++linger_ == 40) nnh::scenes::replace(*demos_.showcase);
        return;
    }
    for (int i = 0; i < kPerFrame && !check_->done(); ++i) {
        if (!check_->step()) continue;
        const int k = check_->frame() / pc::kCheckpointEvery - 1;
        std::snprintf(line_, sizeof line_, "particles: %d sum %08X", check_->frame(),
                      static_cast<unsigned>(check_->checksum()));
        NNH_LOG_INFO(example, "%s", line_);
        if (check_->checksum() != pc::kParticlesCheckpoints[k]) {
            failed_ = true;
            std::snprintf(verdict_, sizeof verdict_, "particles: FAIL frame %d expected %08X got %08X",
                          check_->frame(), static_cast<unsigned>(pc::kParticlesCheckpoints[k]),
                          static_cast<unsigned>(check_->checksum()));
            NNH_LOG_ERROR(example, "%s", verdict_);
            return;
        }
    }
}

void CheckScene::draw() {
    namespace gfx = nnh::gfx;
    if (check_) { // the script's emitters as squares (world space), on the top screen
        for (int i = 0; i < pc::kEmitters; ++i) check_->emitter(i).draw(nnh::Screen::Top, {.layer = 1});
    }
    gfx::draw_text(nnh::Screen::Top, {200, 8}, "PARTICLES SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center,
                                                                         .shadow = true, .layer = 3});
    gfx::draw_text(nnh::Screen::Top, {200, 40}, line_, {.align = nnh::TextAlign::Center, .shadow = true, .layer = 3});
    if (verdict_[0] != '\0') {
        gfx::draw_text(nnh::Screen::Top, {200, 56}, verdict_,
                       {.color = failed_ ? nnh::colors::red : nnh::colors::green, .align = nnh::TextAlign::Center,
                        .shadow = true, .layer = 3});
    }
    gfx::draw_text(nnh::Screen::Bottom, {8, 8},
                   "8 emitters, 3600 frames of a fixed\nscript (every shape, rates, bursts,\nemit_at, world and local "
                   "space, a\nmirrored emitter, prewarm, restarts,\nset_desc, one emitter kept full).\nA checksum of every "
                   "particle's bits\nevery 600 frames: the PC and the 3DS\nmust agree.\n\nSELECT back");
}
