#pragma once
// The emitter's clock, as update() uses it (emitter_clock.cpp).

#include <cstdint>

#include "particles/particle_block.hpp"

namespace nnh::detail {

/// One frame of the clock: [t0, t1) in ticks since the emission's start.
struct ClockFrame {
    bool running = false; ///< started and not ended
    bool rate = false;    ///< the rate emits this frame (t0 at or after the delay, before the duration's end)
    std::int64_t t0 = 0;
    std::int64_t t1 = 0;
};

/// The frame about to run (nothing changes).
ClockFrame clock_begin(const ParticleBlock& b);
/// Fires the bursts due before t1 (on the current transform, age 0), then moves the clock (wrapping when looping, or
/// ending the emission at the duration's end).
void clock_end(ParticleBlock& b, const ClockFrame& f);

} // namespace nnh::detail
