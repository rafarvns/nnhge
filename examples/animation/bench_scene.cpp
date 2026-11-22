#include "bench_scene.hpp"

#include <cstdio>
#include <new>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>

void BenchScene::on_enter() {
    gallery_ = make_gallery_library();
    knight_ = make_knight_library();
    auto texture = nnh::load_texture("art/gallery");
    if (texture) texture_ = std::move(texture.value());
    states_.reset(new (std::nothrow) KnightStates(knight_));
    animators_.reset(new (std::nothrow) nnh::SpriteAnimator[kAnimators]);
    if (!states_ || !animators_) return;
    for (int i = 0; i < kAnimators; ++i) {
        nnh::SpriteAnimator& a = animators_[i];
        if (i % 2 == 0) {
            a = nnh::SpriteAnimator{gallery_};
            a.play(nnh::SpriteClipId{static_cast<std::uint16_t>(i / 2 % kGalleryClips)});
            a.set_speed(i % 3 == 0 ? 1.5f : 1.0f);
            if (i % 6 == 0) {
                a.on_notice([this](nnh::SpriteAnimator&, const nnh::SpriteNotice&) { ++markers_; });
            }
        } else {
            a = nnh::SpriteAnimator{knight_, states_->idle};
        }
    }
    frames_ = markers_ = 0;
    total_ = worst_ = nnh::Duration{};
    result_[0] = '\0';
    NNH_LOG_INFO(anim, "animation bench: started, %d animators of %u bytes, libraries %u + %u bytes", kAnimators,
                 static_cast<unsigned>(sizeof(nnh::SpriteAnimator)), static_cast<unsigned>(gallery_.memory_used()),
                 static_cast<unsigned>(knight_.memory_used()));
}

void BenchScene::on_exit() {
    animators_.reset();
    states_.reset();
    texture_.reset();
    gallery_ = nnh::SpriteLibrary{};
    knight_ = nnh::SpriteLibrary{};
}

void BenchScene::update() {
    if (animators_ && states_) {
        const KnightStates& s = *states_;
        const nnh::Duration start = nnh::time::now();
        {
            NNH_PROFILE_SCOPE("animation bench");
            for (int i = 0; i < kAnimators; ++i) {
                nnh::SpriteAnimator& a = animators_[i];
                if (i % 2 == 1) {
                    const int t = (frames_ + i * 13) % 90;
                    a.request(t < 30 ? s.idle : (t < 60 ? s.run : (t < 62 ? s.attack : s.fall)));
                }
                (void)a.update();
            }
        }
        const nnh::Duration spent = nnh::time::now() - start;
        if (frames_ < kFrames) {
            total_ += spent;
            if (spent > worst_) worst_ = spent;
            if (++frames_ == kFrames) report();
        }
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.showcase);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void BenchScene::report() {
    const double avg = total_.seconds() * 1000.0 / kFrames, worst = worst_.seconds() * 1000.0;
    const char* verdict = avg < 0.8 ? "PASS" : "OVER";
    std::snprintf(result_, sizeof result_,
                  "animation bench: %d animators avg %.3f ms worst %.3f ms (target < 0.8 ms) %s", kAnimators, avg,
                  worst, verdict);
    NNH_LOG_INFO(anim, "%s", result_);
    std::snprintf(shown_, sizeof shown_, "avg %.3f ms  worst %.3f ms  %s", avg, worst, verdict);
}

void BenchScene::draw() {
    if (animators_) {
        for (int i = 0; i < kAnimators; i += 2) { // the gallery half, 16 x 16 cells
            const int k = i / 2;
            nnh::gfx::draw(nnh::Screen::Top, texture_, {static_cast<float>(8 + (k % 24) * 16 + 8),
                                                        static_cast<float>(16 + (k / 24) * 18 + 8)},
                           animators_[i].draw_options());
        }
    }
    char info[128];
    std::snprintf(info, sizeof info, "%d animators, %u bytes each\nlibraries %u + %u bytes\nframe %d/%d  markers %d",
                  kAnimators, static_cast<unsigned>(sizeof(nnh::SpriteAnimator)),
                  static_cast<unsigned>(gallery_.memory_used()), static_cast<unsigned>(knight_.memory_used()), frames_,
                  kFrames, markers_);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 8}, info);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 216}, result_[0] != '\0' ? shown_ : "measuring...",
                        {.color = nnh::colors::yellow});
}
