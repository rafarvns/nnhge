// nnh::Timer, the plain timer value (spec 015 FR-14): integer ticks of the frame clock, no float
// accumulation.

#include "nnh/assert.hpp"
#include "nnh/log.hpp"
#include "nnh/tween.hpp"

NNH_LOG_CHANNEL_EXTERN(tween);

namespace nnh {
namespace {

constexpr std::uint8_t kRunning = 1;
constexpr std::uint8_t kPaused = 2;
constexpr std::uint8_t kRepeating = 4;
constexpr std::uint8_t kFired = 8; // a one-shot timer that has fired (progress 1 even for a time of 0)

bool valid_time(FrameTime time, const char* what) {
    if (time.valid()) return true;
    NNH_LOG_ERROR(tween, "Timer: %s is negative or over 1 hour (ignored)", what);
    NNH_ASSERT(false, "nnh::Timer: invalid %s", what);
    return false;
}

} // namespace

Timer Timer::once(FrameTime time) {
    Timer t;
    if (!valid_time(time, "time")) return t;
    t.period_ = time.ticks;
    t.flags_ = kRunning;
    return t;
}

Timer Timer::repeating(FrameTime period) {
    Timer t;
    if (!valid_time(period, "period")) return t;
    if (period.ticks == 0) {
        NNH_LOG_ERROR(tween, "Timer: a repeating period of 0 would fire forever in one update (stopped)");
        NNH_ASSERT(false, "nnh::Timer::repeating: the period must be at least 1 tick");
        return t;
    }
    t.period_ = period.ticks;
    t.flags_ = kRunning | kRepeating;
    return t;
}

int Timer::update() {
    return advance(Frames{1});
}

int Timer::advance(FrameTime time) {
    if (!valid_time(time, "advance")) return 0;
    if ((flags_ & kRunning) == 0 || (flags_ & kPaused) != 0) return 0;
    elapsed_ += time.ticks; // both ≤ 1 hour of ticks: no overflow
    if (elapsed_ < period_) return 0;
    if ((flags_ & kRepeating) == 0) {
        elapsed_ = period_;
        flags_ = kFired;
        return 1;
    }
    const int fired = elapsed_ / period_;
    elapsed_ -= fired * period_;
    return fired;
}

bool Timer::running() const {
    return (flags_ & kRunning) != 0;
}
bool Timer::paused() const {
    return (flags_ & (kRunning | kPaused)) == (kRunning | kPaused);
}
FrameTime Timer::elapsed() const {
    return FrameTime::from_ticks(elapsed_);
}
FrameTime Timer::remaining() const {
    return FrameTime::from_ticks(period_ - elapsed_);
}

float Timer::progress() const {
    if (period_ == 0) return (flags_ & kFired) != 0 ? 1.0f : 0.0f;
    return static_cast<float>(elapsed_) / static_cast<float>(period_);
}

void Timer::pause() {
    if (running()) flags_ |= kPaused;
}
void Timer::resume() {
    flags_ &= static_cast<std::uint8_t>(~kPaused);
}
void Timer::stop() {
    flags_ &= kRepeating;
    elapsed_ = 0;
}
void Timer::restart() {
    flags_ = static_cast<std::uint8_t>((flags_ & kRepeating) | kRunning);
    elapsed_ = 0;
}

} // namespace nnh
