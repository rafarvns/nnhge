// The pause state machine (dev_pause.hpp). Pure.

#include "app/dev_pause.hpp"

namespace nnh::detail {

void PauseState::pause() {
    mode_ = Mode::Paused;
    steps_ = 0;
}

void PauseState::resume() {
    mode_ = Mode::Running;
    steps_ = 0;
    held_ = false;
    held_ticks_ = 0;
}

void PauseState::toggle() {
    if (mode_ == Mode::Running) pause();
    else resume();
}

void PauseState::step(std::uint16_t frames) {
    mode_ = Mode::Stepping;
    steps_ = frames == 0 ? 1 : frames;
    held_ = false;
    held_ticks_ = 0;
}

void PauseState::hold() {
    pause();
    held_ = true;
    held_ticks_ = 0;
}

bool PauseState::begin_frame() {
    if (mode_ == Mode::Running) return true;
    if (mode_ == Mode::Paused) return false;
    if (--steps_ == 0) mode_ = Mode::Paused; // the last step's frame runs; the next start pauses
    return true;
}

bool PauseState::tick(bool attached) {
    if (!held_ || attached) return false;
    if (++held_ticks_ < kHoldTicks) return false;
    resume();
    return true;
}

} // namespace nnh::detail
