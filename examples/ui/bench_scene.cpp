#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <new>
#include <nnh/build.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>
#include <nnh/time.hpp>

namespace {

const char* const kNames[BenchScene::kPhases] = {"base",      "idle128", "move128",  "geo128",    "relayout128",
                                                 "list10000", "list20",  "draw_ui", "draw_hand", "frame"};
// AC-6's targets as the spec wrote them (they predate any console measurement).
constexpr double kTargets[BenchScene::kPhases] = {0, 0.05, 0.15, 0.15, 0.6, 0.2, 0.2, 0, 0, 0};

double scope_ms(const char* name) {
    nnh::profile::ScopeStats scopes[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(scopes, nnh::profile::kMaxScopes);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(scopes[i].name, name) == 0) return static_cast<double>(scopes[i].total.ns) / 1e6;
    }
    return 0;
}

bool judged() { return nnh::build::flavor() == nnh::build::Flavor::Development; }

/// The block's bytes per widget and per pooled row, from two blocks' sizes (16-byte table alignment cancels out).
int bytes_per(bool rows) {
    auto a = nnh::Ui::create(rows ? nnh::UiDesc{.rows = 64} : nnh::UiDesc{.widgets = 64});
    auto b = nnh::Ui::create(rows ? nnh::UiDesc{.rows = 128} : nnh::UiDesc{.widgets = 128});
    if (!a || !b) return 0;
    return static_cast<int>(b.value().stats().block_bytes - a.value().stats().block_bytes) / 64;
}

} // namespace

void BenchScene::on_enter() {
    bench_.reset(new (std::nothrow) UiBench());
    screen_.reset(new (std::nothrow) OptionsScreen());
    if (bench_ && !bench_->init()) bench_.reset();
    if (screen_ && !screen_->build(demo_.art)) screen_.reset();
    for (Sum& s : sums_) s = {};
    phase_ = frame_ = missed_ = draws_ui_ = 0;
    worst_frame_ = 0;
    failed_[0] = '\0';
    done_ = false;
    memory_logged_ = false;
    NNH_LOG_INFO(example, "ui bench: started (10 runs of 300 frames, the frame test 600: about 1 min)");
    if (!bench_ || !screen_) NNH_LOG_ERROR(example, "ui bench: FAIL no memory");
}

void BenchScene::on_exit() {
    bench_.reset();
    screen_.reset();
}

double BenchScene::cost(Phase p) const {
    const Sum& s = sums_[p];
    const Sum& b = sums_[kBase];
    const double n = s.n > 0 ? s.n : 1, bn = b.n > 0 ? b.n : 1;
    // the list phases time the scroll only: their submit is the untimed draw of the shown rows (the draw phases'
    // business), the same for 20 rows or 10 000, so adding it would hide the scroll's cost under the drawing's
    if (p == kList10000 || p == kList20) return s.own / n - b.own / bn;
    return (s.own / n - b.own / bn) + (s.submit / n - b.submit / bn);
}

void BenchScene::finish() {
    const Phase p = static_cast<Phase>(phase_);
    const char* judge = judged() ? "" : " not judged (Debug)";
    char line[160] = {};
    bool pass = true;
    if (p == kBase) {
        const double n = sums_[p].n > 0 ? sums_[p].n : 1;
        std::snprintf(line, sizeof line, "ui bench: base gfx submit avg %.4f ms timer %.4f ms", sums_[p].submit / n,
                      sums_[p].own / n);
    } else if (p == kList20) {
        const double a = cost(kList10000), b = cost(kList20);
        const double ratio = b > 0 ? a / b : 1;
        pass = a < kTargets[kList10000] && ratio <= 1.10;
        std::snprintf(line, sizeof line,
                      "ui bench: list10000 avg %.3f ms list20 avg %.3f ms ratio %.2f (target < 0.2 ms, <= 1.10)%s %s",
                      a, b, ratio, judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else if (p == kDrawHand) {
        const int draws = draws_ui_ > 0 ? draws_ui_ : 1;
        const double ui_us = cost(kDrawUi) * 1000.0 / draws, hand_us = cost(kDrawHand) * 1000.0 / draws;
        const double ratio = hand_us > 0 ? ui_us / hand_us : 0;
        pass = ratio <= 1.15;
        std::snprintf(line, sizeof line, "ui bench: draw ui %.2f us hand %.2f us per draw ratio %.2f draws %d (target <= 1.15)%s %s",
                      ui_us, hand_us, ratio, draws_ui_, judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else if (p == kFrame) {
        pass = missed_ == 0;
        std::snprintf(line, sizeof line, "ui bench: frame worst %.1f ms missed %d of %d%s %s", worst_frame_, missed_,
                      kFrameTestFrames, judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    } else if (p == kList10000 || p == kDrawUi) {
        line[0] = '\0'; // reported with their pair
    } else {
        const double avg = cost(p);
        pass = avg < kTargets[p];
        std::snprintf(line, sizeof line, "ui bench: %s avg %.3f ms worst %.3f ms (target < %g ms)%s %s", kNames[p], avg,
                      sums_[p].own_worst, kTargets[p], judge, judged() ? (pass ? "PASS" : "FAIL") : "");
    }
    if (judged() && !pass) {
        const std::size_t at = std::strlen(failed_);
        std::snprintf(failed_ + at, sizeof failed_ - at, "%s%s", at == 0 ? "" : ", ", kNames[p]);
    }
    if (line[0] != '\0') NNH_LOG_INFO(example, "%s", line);
    ++phase_;
    frame_ = 0;
}

void BenchScene::verdict() {
    if (!memory_logged_) { // the memory line, then the verdict in the next frame (each line its own expect)
        memory_logged_ = true;
        const nnh::UiStats s = screen_->ui().stats();
        NNH_LOG_INFO(example, "ui bench: memory widget %d B row %d B options %u B for %d widgets", bytes_per(false),
                     bytes_per(true), static_cast<unsigned>(s.block_bytes), s.widgets);
        return;
    }
    done_ = true;
    if (failed_[0] == '\0') {
        NNH_LOG_INFO(example, "ui bench: PASS");
    } else {
        NNH_LOG_INFO(example, "ui bench: FAIL (%s)", failed_);
    }
}

void BenchScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::pop();
        return;
    }
    if (!bench_ || !screen_ || done_) return;
    if (phase_ >= kPhases) {
        verdict();
        return;
    }
    Sum& s = sums_[phase_];
    if (frame_ >= 2) {
        s.submit += scope_ms("gfx submit");
        ++s.n;
    }
    const nnh::Duration start = nnh::time::now();
    switch (phase_) {
    case kIdle: bench_->idle128(); break;
    case kMove: bench_->move128(frame_); break;
    case kGeo: bench_->geo128(frame_); break;
    case kRelayout: bench_->relayout128(); break;
    case kList10000: bench_->list10000(frame_); break;
    case kList20: bench_->list20(frame_); break;
    case kDrawUi:
    case kFrame: screen_->ui().draw(); break;
    case kDrawHand: screen_->draw_by_hand(); break;
    default: break;
    }
    const double ms = (nnh::time::now() - start).seconds() * 1000.0;
    if (phase_ == kList10000 || phase_ == kList20) bench_->draw_list(phase_ == kList10000); // untimed
    if (phase_ == kDrawUi) draws_ui_ = screen_->ui().stats().draws;
    if (phase_ == kFrame && demo_.art != nullptr) { // 300 game sprites on the top screen
        for (int i = 0; i < 300; ++i) {
            nnh::gfx::draw(nnh::Screen::Top, *demo_.art,
                           {static_cast<float>((i * 37 + frame_) % 384), static_cast<float>((i * 53) % 224)},
                           {.region = {(i % 8) * 16, 40, 16, 16}, .layer = 2});
        }
    }
    if (frame_ >= 2) {
        s.own += ms;
        if (ms > s.own_worst) s.own_worst = ms;
    }
    if (phase_ == kFrame && frame_ >= 2) {
        const double delta = nnh::time::delta().seconds() * 1000.0;
        if (delta > worst_frame_) worst_frame_ = delta;
        if (delta > 20) ++missed_;
    }
    if (++frame_ >= (phase_ == kFrame ? kFrameTestFrames : kFrames)) finish();
}

void BenchScene::draw() {
    nnh::gfx::draw_text(nnh::Screen::Bottom, {8, 8}, done_ ? "UI bench done. SELECT back" : "UI bench running...",
                        {.shadow = true, .layer = 15});
}
