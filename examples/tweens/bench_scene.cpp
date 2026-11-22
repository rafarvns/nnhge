#include "bench_scene.hpp"

#include <cstdio>
#include <nnh/input.hpp>
#include <nnh/memory.hpp>

using nnh::Frames;
using nnh::LoopMode;

namespace {

nnh::TweenOptions options(int i) {
    return {.ease = static_cast<nnh::Ease>(i % nnh::kEaseCount),
            .loops = nnh::kForever,
            .loop_mode = LoopMode::Yoyo};
}

float coordinate(int i, int range) {
    return static_cast<float>((i * 97 + 13) % range);
}

} // namespace

void BenchScene::on_enter() {
    const std::size_t before = nnh::mem::region_stats(nnh::MemRegion::Heap).bytes;
    set_ = nnh::TweenSet({.tweens = kTweens});
    set_bytes_ = nnh::mem::region_stats(nnh::MemRegion::Heap).bytes - before;
    for (int i = 0; i < kTweens; ++i) {
        const Frames time{30 + (i * 7) % 50};
        const int k = i / 3;
        switch (i % 3) {
        case 0:
            set_.from_to(&f_[k], 0.0f, 100.0f, time, options(i));
            break;
        case 1:
            set_.from_to(&v_[k], {coordinate(i, 400), coordinate(i * 3, 240)},
                         {coordinate(i * 5, 400), coordinate(i * 7, 240)}, time, options(i));
            break;
        default:
            set_.from_to(&c_[k], nnh::rgb(255, 60, 60), nnh::rgb(60, 120, 255), time, options(i));
            break;
        }
    }
    frames_ = 0;
    total_ = worst_ = nnh::Duration{};
    result_[0] = '\0';
    NNH_LOG_INFO(tweens, "tweens bench: started, %d tweens, set block %u bytes", set_.active(),
                 static_cast<unsigned>(set_bytes_));
}

void BenchScene::update() {
    const nnh::Duration start = nnh::time::now();
    set_.update();
    const nnh::Duration spent = nnh::time::now() - start;
    if (frames_ < kFrames) {
        total_ += spent;
        if (spent > worst_) worst_ = spent;
        if (++frames_ == kFrames) report();
    }
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.showcase);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void BenchScene::report() {
    const double avg = total_.seconds() * 1000.0 / kFrames, worst = worst_.seconds() * 1000.0;
    std::snprintf(result_, sizeof result_,
                  "tweens bench: %d tweens avg %.2f ms worst %.2f ms (target < 1 ms) %s", kTweens, avg, worst,
                  avg < 1.0 ? "PASS" : "OVER");
    NNH_LOG_INFO(tweens, "%s", result_);
    std::snprintf(shown_, sizeof shown_, "avg %.2f ms  worst %.2f ms  %s", avg, worst,
                  avg < 1.0 ? "PASS" : "OVER");
}

void BenchScene::draw() {
    const int n = kTweens / 3 + 1;
    for (int k = 0; k < n; ++k) {
        nnh::gfx::fill_rect(nnh::Screen::Top, {v_[k].x, v_[k].y, 4, 4}, c_[k]);
        nnh::gfx::fill_rect(nnh::Screen::Bottom, {4 + k * 3.6f, 180 - f_[k], 2, f_[k]}, c_[k]);
    }
    char info[96];
    std::snprintf(info, sizeof info, "%d tweens, %u bytes (%u per slot)\nframe %d/%d", set_.active(),
                  static_cast<unsigned>(set_bytes_), static_cast<unsigned>(set_bytes_ / kTweens), frames_,
                  kFrames);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 190}, info);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 216}, result_[0] != '\0' ? shown_ : "measuring...",
                        {.color = nnh::colors::yellow});
}
