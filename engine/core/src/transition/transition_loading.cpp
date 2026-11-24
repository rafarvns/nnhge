// The covered phase's work (spec 023 FR-13, FR-16): on_covered on its first frame (after the stack change), then the
// loading step once per covered frame until it reports done (progress clamped to 0..1, NaN → 0); a step still not
// done at covered frame 600 (10 s) warns once in Debug/Development and keeps being called.

#include "nnh/build.hpp"
#include "transition/transition_internal.hpp"

namespace nnh::detail {

void transition_covered_frame(TransitionState& t) {
    if (t.timeline.covered == 1 && t.on_covered) t.on_covered();
    if (t.loading_done) return;
    if (!t.loading) {
        t.loading_done = true;
        t.loading_progress = 1.0f;
        return;
    }
    const transition::LoadStep step = t.loading();
    t.loading_progress = clamp_unit(step.progress);
    t.loading_done = step.done;
    if constexpr (build::kDevToolsEnabled) {
        if (!t.loading_done && !t.warned && t.timeline.covered >= static_cast<std::uint32_t>(kMaxPhaseFrames)) {
            t.warned = true;
            NNH_LOG_WARN(transition, "loading step still not done after 10 s (600 covered frames)");
        }
    }
}

} // namespace nnh::detail
