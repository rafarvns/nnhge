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

/// A whole number of 60 Hz frames: `nnh::Frames{30}` is half a second of game frames.
struct Frames {
    std::int32_t count = 0;
};

/// A span of the deterministic frame clock (tweens, timers, animation), in ticks of 1/4096 of a 60 Hz frame.
/// Converts implicitly from a Duration (rounded to the nearest tick, half up: 1 s = 60 frames = 245 760 ticks
/// exactly, 16.67 ms = 4096) and from Frames (exact). The range is 0 to kMaxFrameTime (1 hour); anything outside
/// it (a negative or longer time) becomes the invalid marker `ticks == kInvalid`, which the functions taking a
/// FrameTime reject (they assert in Debug/Development and log in Release). Plain value, no state.
/// @code
/// nnh::FrameTime a = nnh::Duration::from_ms(500); // 30 frames
/// nnh::FrameTime b = nnh::Frames{12};
/// @endcode
struct FrameTime {
    static constexpr std::int32_t kTicksPerFrame = 4096;
    static constexpr std::int32_t kInvalid = -1;
    static constexpr std::int32_t kMaxTicks = 3600 * 60 * kTicksPerFrame; ///< 1 hour: 884 736 000 ticks
    static constexpr std::int32_t kMaxFrames = 3600 * 60;

    constexpr FrameTime() = default;
    // NOLINTNEXTLINE(google-explicit-constructor): implicit on purpose, `to(&x, 1, Duration::from_ms(400))`.
    constexpr FrameTime(Duration d) : ticks(from_ns(d.ns)) {}
    // NOLINTNEXTLINE(google-explicit-constructor)
    constexpr FrameTime(Frames f)
        : ticks(f.count >= 0 && f.count <= kMaxFrames ? f.count * kTicksPerFrame : kInvalid) {}

    /// A raw tick count (0..kMaxTicks; anything else is kept as the invalid marker).
    static constexpr FrameTime from_ticks(std::int64_t t) {
        FrameTime f;
        f.ticks = t >= 0 && t <= kMaxTicks ? static_cast<std::int32_t>(t) : kInvalid;
        return f;
    }

    constexpr bool valid() const { return ticks >= 0; }
    /// Back to nanoseconds, rounded to the nearest one. Duration{-1} for the invalid marker.
    constexpr Duration duration() const {
        return valid() ? Duration{(static_cast<std::int64_t>(ticks) * 25'000'000 + 3072) / 6144} : Duration{-1};
    }
    /// In frames (fractional): 2048 ticks = 0.5.
    constexpr float frames() const { return static_cast<float>(ticks) / static_cast<float>(kTicksPerFrame); }

    constexpr bool operator==(const FrameTime&) const = default;

    std::int32_t ticks = 0;

private:
    // ticks = ns · 4096 · 60 / 10⁹ = ns · 6144 / 25 000 000, rounded half up. Bounds first: no overflow.
    static constexpr std::int32_t from_ns(std::int64_t ns) {
        if (ns < 0 || ns > 3'600'000'000'000) return kInvalid;
        return static_cast<std::int32_t>((ns * 6144 + 12'500'000) / 25'000'000);
    }
};

/// The longest FrameTime: 1 hour of frames. Longer waits: count with your own variables.
inline constexpr FrameTime kMaxFrameTime = FrameTime::from_ticks(FrameTime::kMaxTicks);

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
