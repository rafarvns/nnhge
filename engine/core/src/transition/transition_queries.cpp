// The running transition's state and its queries (spec 023 FR-15): state is queried, never published.

#include "transition/transition_internal.hpp"

NNH_LOG_CHANNEL(transition);

namespace nnh {

detail::TransitionState detail::g_transition;

namespace {

const detail::TransitionState& state() { return detail::g_transition; }

} // namespace

bool transition::running() { return state().running; }

transition::Phase transition::phase() { return state().running ? state().timeline.phase : Phase::None; }

float transition::coverage(Screen screen) {
    const detail::TransitionState& t = state();
    if (!t.running) return 0.0f;
    const int index = (screen == Screen::Bottom ? detail::kBottomOut : detail::kTopOut) +
                      (t.timeline.phase == Phase::In ? 1 : 0);
    return t.fx[index].kind == Kind::None ? 0.0f : t.timeline.coverage;
}

float transition::phase_progress() {
    return state().running ? detail::timeline_phase_progress(state().timeline) : 0.0f;
}

float transition::loading_progress() { return state().running ? state().loading_progress : 0.0f; }

int transition::frames() { return state().running ? static_cast<int>(state().frames) : 0; }

transition::Timing transition::timing() {
    const detail::Timeline& t = state().timeline;
    return Timing{Frames{t.out}, Frames{t.hold}, Frames{t.in}};
}

float transition::seconds(Frames frames) { return static_cast<float>(frames.count) / 60.0f; }

} // namespace nnh
