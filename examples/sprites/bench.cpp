#include "bench.hpp"

#include <cstdint>

#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/time.hpp>

namespace {

constexpr float kWidth = 400 - 16;
constexpr float kHeight = 240 - 16;

/// Small deterministic generator, so every run starts the same.
float random01(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / 16777216.0f;
}

} // namespace

Bench::Bench(const nnh::Texture& hero, int count) : hero_(hero) {
    std::uint32_t seed = 12345;
    for (int i = 0; i < kMaxSprites; ++i) {
        x_[i] = random01(seed) * kWidth;
        y_[i] = random01(seed) * kHeight;
        vx_[i] = (random01(seed) - 0.5f) * 4;
        vy_[i] = (random01(seed) - 0.5f) * 4;
    }
    set_count(count);
}

void Bench::set_count(int count) { count_ = count < 0 ? 0 : (count > kMaxSprites ? kMaxSprites : count); }

void Bench::update() {
    if (nnh::input::pressed(nnh::Button::R)) set_count(count_ + 100);
    if (nnh::input::pressed(nnh::Button::L)) set_count(count_ - 100);
    if (nnh::input::pressed(nnh::Button::X)) set_count(count_ + 1000);
    if (nnh::input::pressed(nnh::Button::Y)) set_count(count_ - 1000);
    for (int i = 0; i < count_; ++i) {
        x_[i] += vx_[i];
        y_[i] += vy_[i];
        if (x_[i] < 0 || x_[i] > kWidth) vx_[i] = -vx_[i];
        if (y_[i] < 0 || y_[i] > kHeight) vy_[i] = -vy_[i];
    }
    const double ms = nnh::time::delta().seconds() * 1000.0;
    frame_ms_ = frame_ms_ == 0 ? ms : frame_ms_ * 0.95 + ms * 0.05;
}

void Bench::draw() const {
    for (int i = 0; i < count_; ++i) {
        nnh::gfx::draw(nnh::Screen::Top, hero_, {x_[i], y_[i]}, {.region = {(i % 4) * 16, 0, 16, 16}});
    }
    const nnh::Screen bottom = nnh::Screen::Bottom;
    const int line = nnh::debug::line_height();
    nnh::debug::printf(bottom, 20, 20, "BENCHMARK  %d sprites", count_);
    nnh::debug::printf(bottom, 20, 20 + line, "frame %.1f ms  (%.0f FPS)", frame_ms_,
                       frame_ms_ > 0 ? 1000.0 / frame_ms_ : 0.0);
    nnh::debug::print(bottom, 20, 20 + 3 * line, "L/R: -/+100   Y/X: -/+1000");
    nnh::debug::print(bottom, 20, 20 + 4 * line, "SELECT: back to the world");
    nnh::debug::print(bottom, 20, 20 + 5 * line, "START: exit");
    const nnh::gfx::Stats s = nnh::gfx::stats();
    nnh::debug::printf(bottom, 20, 20 + 7 * line, "draws %d / %d", s.draws_last_frame, s.max_draws);
}
