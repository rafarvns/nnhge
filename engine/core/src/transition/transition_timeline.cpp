// The frame timeline (plan 023 "The frame timeline"): every phase a whole number of frames; coverage from spec 015's
// libm-free easings of an exact integer ratio (one IEEE division), clamped and ratcheted with plain compares. Pure.

#include "transition/transition_internal.hpp"

namespace nnh::detail {

using transition::Phase;

int frames_of(FrameTime time) {
    if (!time.valid()) return 0;
    const std::int32_t n = (time.ticks + FrameTime::kTicksPerFrame / 2) / FrameTime::kTicksPerFrame;
    return n > kMaxPhaseFrames ? kMaxPhaseFrames : static_cast<int>(n);
}

float clamp_unit(float value) {
    if (!(value > 0.0f)) return 0.0f; // NaN too
    return value < 1.0f ? value : 1.0f;
}

void timeline_start(Timeline& t, int out, int hold, int in) {
    t = Timeline{};
    t.out = static_cast<std::uint16_t>(out);
    t.hold = static_cast<std::uint16_t>(hold);
    t.in = static_cast<std::uint16_t>(in);
    t.phase = out > 0 ? Phase::Out : Phase::Covered;
    t.coverage = out > 0 ? 0.0f : 1.0f;
}

void timeline_step(Timeline& t, const Easing& out_ease, const Easing& in_ease) {
    switch (t.phase) {
    case Phase::Out: {
        ++t.k;
        if (t.k >= t.out) {
            t.coverage = 1.0f;
            break;
        }
        const float c = clamp_unit(out_ease(static_cast<float>(t.k) / static_cast<float>(t.out)));
        if (c > t.coverage) t.coverage = c; // ratchet up: overshooting curves plateau
        break;
    }
    case Phase::Covered:
        ++t.covered;
        t.coverage = 1.0f;
        break;
    case Phase::In: {
        ++t.k;
        if (t.k >= t.in) {
            t.coverage = 0.0f;
            break;
        }
        const float c = clamp_unit(in_ease(static_cast<float>(t.in - t.k) / static_cast<float>(t.in)));
        if (c < t.coverage) t.coverage = c; // ratchet down
        break;
    }
    case Phase::None: break;
    }
}

TimelineEnd timeline_end(Timeline& t, bool loading_done, bool skip_in) {
    switch (t.phase) {
    case Phase::Out:
        if (t.k < t.out) return TimelineEnd::Continue;
        t.phase = Phase::Covered;
        t.covered = 0;
        return TimelineEnd::OutEnded;
    case Phase::Covered: {
        const std::uint32_t at_least = t.hold > 0 ? t.hold : 1;
        if (!loading_done || t.covered < at_least) return TimelineEnd::Continue;
        if (t.in == 0 || skip_in) return TimelineEnd::Done;
        t.phase = Phase::In;
        t.k = 0;
        return TimelineEnd::Continue;
    }
    case Phase::In: return t.k < t.in ? TimelineEnd::Continue : TimelineEnd::Done;
    case Phase::None: break;
    }
    return TimelineEnd::Done;
}

float timeline_phase_progress(const Timeline& t) {
    switch (t.phase) {
    case Phase::Out: return t.out > 0 ? static_cast<float>(t.k) / static_cast<float>(t.out) : 1.0f;
    case Phase::In: return t.in > 0 ? static_cast<float>(t.k) / static_cast<float>(t.in) : 1.0f;
    case Phase::Covered:
        if (t.hold == 0 || t.covered >= t.hold) return 1.0f;
        return static_cast<float>(t.covered) / static_cast<float>(t.hold);
    case Phase::None: break;
    }
    return 0.0f;
}

} // namespace nnh::detail
