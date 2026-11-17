#pragma once
/// @file nnh/time.hpp
/// Time: durations, the engine clock, frame delta and a fixed-timestep helper. See docs/api/time.md.

#include <cstdint>

namespace nnh {

/// A span of time in integer nanoseconds (no float drift over long sessions). Cheap to copy.
/// @code
/// nnh::Duration cooldown = nnh::Duration::from_ms(250);
/// if (nnh::time::now() - last_shot > cooldown) shoot();
/// @endcode
struct Duration {
    std::int64_t ns = 0;

    static constexpr Duration from_seconds(std::int64_t s) { return Duration{s * 1'000'000'000}; }
    static constexpr Duration from_ms(std::int64_t ms) { return Duration{ms * 1'000'000}; }
    static constexpr Duration from_us(std::int64_t us) { return Duration{us * 1'000}; }

    constexpr std::int64_t ms() const { return ns / 1'000'000; }
    constexpr double seconds() const { return static_cast<double>(ns) / 1e9; }

    constexpr Duration operator+(Duration o) const { return Duration{ns + o.ns}; }
    constexpr Duration operator-(Duration o) const { return Duration{ns - o.ns}; }
    constexpr Duration operator*(std::int64_t k) const { return Duration{ns * k}; }
    constexpr Duration& operator+=(Duration o) { ns += o.ns; return *this; }
    constexpr Duration& operator-=(Duration o) { ns -= o.ns; return *this; }
    constexpr bool operator==(Duration o) const { return ns == o.ns; }
    constexpr bool operator!=(Duration o) const { return ns != o.ns; }
    constexpr bool operator<(Duration o) const { return ns < o.ns; }
    constexpr bool operator>(Duration o) const { return ns > o.ns; }
    constexpr bool operator<=(Duration o) const { return ns <= o.ns; }
    constexpr bool operator>=(Duration o) const { return ns >= o.ns; }
};

namespace time {

/// Time since nnh::init(), from a monotonic clock. Valid after init(); 0 before.
Duration now();

/// Time between the start of the previous frame and the start of the current one
/// (about 16.7 ms at 60 Hz). 0 on the first frame.
Duration delta();

/// Runs game logic at a fixed rate, independent of frame hiccups. Call next() in a loop each frame:
/// it returns true once per logic step due since the last frame. After a long stall it catches up at
/// most `max_steps_per_frame` steps and drops the rest, so the game slows down instead of freezing.
/// @code
/// nnh::time::FixedStep logic(60);
/// while (nnh::frame()) {
///     while (logic.next()) update_physics(logic.step());
///     draw();
/// }
/// @endcode
class FixedStep {
public:
    explicit FixedStep(int steps_per_second, int max_steps_per_frame = 4);

    /// True if one more logic step should run this frame.
    bool next();

    /// Duration of one logic step (1 / steps_per_second).
    Duration step() const { return step_; }

    /// Fraction [0, 1) of a step accumulated but not run yet; useful to interpolate drawing.
    float alpha() const;

private:
    Duration step_;
    Duration accumulated_;
    int max_steps_;
    std::uint64_t last_frame_ = 0;
};

} // namespace time
} // namespace nnh
