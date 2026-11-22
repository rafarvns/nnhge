#include "bench_scene.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/profile.hpp>
#include <nnh/system.hpp>

using nnh::AnalogStick;
using nnh::Button;
using nnh::Direction;
using nnh::Source;

namespace {

constexpr int kActions = 64;

Button button(int i) { return static_cast<Button>(1u << (i % 14)); }

/// Four mixed slots for action `i` of each kind.
nnh::ActionDesc desc(int i, const char* name) {
    const auto stick = static_cast<AnalogStick>(i & 1);
    const auto dir = static_cast<Direction>(i % 4);
    nnh::ActionDesc d{.name = name};
    switch (i % 3) {
    case 0:
        d.kind = nnh::ActionKind::Button;
        d.bindings[0] = {Source::button(button(i))};
        d.bindings[1] = {Source::stick_direction(stick, dir)};
        d.bindings[2] = {Source::button(button(i + 5))};
        d.bindings[3] = {Source::touch(i * 4, 0, 16, 16)};
        break;
    case 1:
        d.kind = nnh::ActionKind::Axis;
        d.bindings[0] = {Source::stick_axis(stick, nnh::StickAxis::X)};
        d.bindings[1] = {Source::buttons(button(i), button(i + 1))};
        d.bindings[2] = {Source::stick_direction(stick, dir)};
        d.bindings[3] = {.source = Source::stick_axis(stick, nnh::StickAxis::Y), .dead_zone = 0.2f};
        break;
    default:
        d.kind = nnh::ActionKind::Vector;
        d.bindings[0] = {.source = Source::stick(stick), .dead_zone = 0.1f};
        d.bindings[1] = {Source::dpad()};
        d.bindings[2] = {Source::buttons(Button::X, Button::B, Button::Y, Button::A)};
        d.bindings[3] = {.source = Source::stick(stick), .invert_y = true};
        break;
    }
    return d;
}

/// Time of the profiler's `actions` scope in the previous frame (every set's evaluation), or a negative value.
double actions_ms() {
    nnh::profile::ScopeStats stats[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(stats, nnh::profile::kMaxScopes);
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(stats[i].name, "actions") == 0) return stats[i].total.seconds() * 1000.0;
    }
    return -1.0;
}

} // namespace

void BenchScene::on_enter() {
    bench_ = nnh::ActionSet({.actions = kActions});
    for (int i = 0; i < kActions; ++i) {
        char name[8];
        std::snprintf(name, sizeof name, "b%02d", i);
        bench_.add(nnh::ActionId(i), desc(i, name)); // names are copied
    }
    bench_.set_events_enabled(false);
    frames_ = 0;
    total_ = worst_ = nnh::Duration{};
    measured_ = false;
    result_[0] = shown_[0] = verdict_[0] = '\0';
    log_line("actions: bench started, 64 actions x 4 slots");
}

void BenchScene::on_exit() { bench_ = nnh::ActionSet{}; }

void BenchScene::update() {
    if (frames_ > kFrames) {
        if (nnh::input::pressed(nnh::Button::B)) nnh::scenes::pop();
        return;
    }
    if (frames_ == kFrames) { // the line after the bench's: a script expects one line per frame
        s_.check.verdict(verdict_, sizeof verdict_);
        if (s_.check.passed()) {
            log_line(verdict_);
        } else {
            NNH_LOG_WARN(example, "%s", verdict_);
        }
        ++frames_;
        return;
    }
    const double ms = actions_ms();
    if (frames_ > 0 && ms >= 0.0) { // frame 0's previous frame had no bench set
        measured_ = true;
        const nnh::Duration spent{static_cast<std::int64_t>(ms * 1e6)};
        total_ += spent;
        if (spent > worst_) worst_ = spent;
    }
    if (++frames_ == kFrames) report();
}

void BenchScene::report() {
    const double avg = measured_ ? total_.seconds() * 1000.0 / (kFrames - 1) : -1.0;
    const bool console = std::strcmp(nnh::system::info().name, "PC simulator") != 0;
    if (measured_) {
        std::snprintf(result_, sizeof result_, "actions: bench avg %.3f ms worst %.3f ms (target < 0.2 ms) %s", avg,
                      worst_.seconds() * 1000.0, avg < 0.2 ? "PASS" : "OVER");
        std::snprintf(shown_, sizeof shown_, "avg %.3f ms  worst %.3f ms", avg, worst_.seconds() * 1000.0);
    } else {
        std::snprintf(result_, sizeof result_, "actions: bench not measured (no profiler in Release)");
        std::snprintf(shown_, sizeof shown_, "not measured (Release)");
    }
    log_line(result_);
    s_.check.bench(static_cast<float>(avg), console);
}

void BenchScene::draw() {
    char info[64];
    std::snprintf(info, sizeof info, "ACTIONS BENCH  frame %d/%d", frames_ < kFrames ? frames_ : kFrames, kFrames);
    const nnh::Screen top = nnh::Screen::Top;
    nnh::gfx::fill_rect(top, {0, 0, 400, 240}, nnh::rgb(12, 14, 26)); // covers the screens under it
    nnh::gfx::draw_text(top, {200, 90}, info, {.align = nnh::TextAlign::Center});
    nnh::gfx::draw_text(top, {200, 110}, result_[0] != '\0' ? shown_ : "measuring...",
                        {.color = nnh::colors::yellow, .align = nnh::TextAlign::Center});
    if (verdict_[0] != '\0') {
        nnh::gfx::draw_text(top, {200, 140}, verdict_,
                            {.color = s_.check.passed() ? nnh::colors::green : nnh::colors::red,
                             .scale = 2,
                             .align = nnh::TextAlign::Center});
        nnh::gfx::draw_text(top, {200, 200}, "B: back", {.align = nnh::TextAlign::Center});
    }
}
