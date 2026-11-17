#pragma once
// Pure fixed-timestep logic used by nnh::time::FixedStep. Unit tested in tests/time/test_fixed_step.cpp.

#include "nnh/time.hpp"

namespace nnh::detail {

constexpr Duration step_for_rate(int steps_per_second) {
    return Duration{1'000'000'000 / (steps_per_second > 0 ? steps_per_second : 1)};
}

constexpr int clamp_max_steps(int max_steps) { return max_steps > 0 ? max_steps : 1; }

/// Adds elapsed time to an accumulator, keeping at most max_steps steps pending (drops the excess).
constexpr Duration accumulate(Duration accumulated, Duration elapsed, Duration step, int max_steps) {
    const Duration total = accumulated + elapsed;
    const Duration cap = step * max_steps;
    return total > cap ? cap : total;
}

/// Takes one step from the accumulator if enough time is pending.
constexpr bool consume_step(Duration& accumulated, Duration step) {
    if (accumulated < step) return false;
    accumulated -= step;
    return true;
}

constexpr float step_alpha(Duration accumulated, Duration step) {
    return static_cast<float>(accumulated.ns) / static_cast<float>(step.ns);
}

/// Standalone accumulator (no engine clock), convenient for tests and tools.
class FixedStepAccumulator {
public:
    FixedStepAccumulator(int steps_per_second, int max_steps_per_frame)
        : step_(step_for_rate(steps_per_second)), max_steps_(clamp_max_steps(max_steps_per_frame)) {}

    void add(Duration elapsed) { accumulated_ = accumulate(accumulated_, elapsed, step_, max_steps_); }
    bool consume() { return consume_step(accumulated_, step_); }
    Duration step() const { return step_; }
    float alpha() const { return step_alpha(accumulated_, step_); }

private:
    Duration step_;
    Duration accumulated_{};
    int max_steps_;
};

} // namespace nnh::detail
