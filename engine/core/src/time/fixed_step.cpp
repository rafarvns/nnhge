// nnh::time::FixedStep — adds each new frame's delta once, then hands out logic steps.

#include "nnh/app.hpp"
#include "nnh/time.hpp"
#include "time/fixed_step_accumulator.hpp"

namespace nnh::time {

FixedStep::FixedStep(int steps_per_second, int max_steps_per_frame)
    : step_(detail::step_for_rate(steps_per_second)), max_steps_(detail::clamp_max_steps(max_steps_per_frame)) {}

bool FixedStep::next() {
    if (frame_count() != last_frame_) {
        last_frame_ = frame_count();
        accumulated_ = detail::accumulate(accumulated_, delta(), step_, max_steps_);
    }
    return detail::consume_step(accumulated_, step_);
}

float FixedStep::alpha() const { return detail::step_alpha(accumulated_, step_); }

} // namespace nnh::time
